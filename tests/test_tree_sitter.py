import re
from os import path
from typing import List, Optional, Tuple
from unittest import TestCase

from tree_sitter import Language, Parser, Tree
from tree_sitter.binding import LookaheadIterator, Node

LIB_PATH = path.join("build", "languages.so")

# cibuildwheel uses a funny working directory when running tests.
# This is by design, this way tests import whatever is installed and not from the project.
#
# The languages binary is still relative to current working directory to prevent reusing
# a 32-bit languages binary in a 64-bit build. The working directory is clean every time.
project_root = path.dirname(path.dirname(path.abspath(__file__)))
Language.build_library(
    LIB_PATH,
    [
        path.join(project_root, "tests", "fixtures", "tree-sitter-embedded-template"),
        path.join(project_root, "tests", "fixtures", "tree-sitter-javascript"),
        path.join(project_root, "tests", "fixtures", "tree-sitter-json"),
        path.join(project_root, "tests", "fixtures", "tree-sitter-python"),
        path.join(project_root, "tests", "fixtures", "tree-sitter-rust"),
    ],
)

EMBEDDED_TEMPLATE = Language(LIB_PATH, "embedded_template")
JAVASCRIPT = Language(LIB_PATH, "javascript")
JSON = Language(LIB_PATH, "json")
PYTHON = Language(LIB_PATH, "python")
RUST = Language(LIB_PATH, "rust")

JSON_EXAMPLE: bytes = b"""

[
  123,
  false,
  {
    "x": null
  }
]
"""


class TestParser(TestCase):
    def test_set_language(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")
        self.assertEqual(
            tree.root_node.sexp(),
            trim(
                """(module (function_definition
                name: (identifier)
                parameters: (parameters)
                body: (block (expression_statement (call
                    function: (identifier)
                    arguments: (argument_list))))))"""
            ),
        )
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"function foo() {\n  bar();\n}")
        self.assertEqual(
            tree.root_node.sexp(),
            trim(
                """(program (function_declaration
                name: (identifier)
                parameters: (formal_parameters)
                body: (statement_block
                    (expression_statement
                         (call_expression
                            function: (identifier)
                            arguments: (arguments))))))"""
            ),
        )

    def test_read_callback(self):
        parser = Parser()
        parser.set_language(PYTHON)
        source_lines = ["def foo():\n", "  bar()"]

        def read_callback(_byte_offset: int, point: Tuple[int, int]) -> Optional[bytes]:
            row, column = point
            if row >= len(source_lines):
                return None
            if column >= len(source_lines[row]):
                return None
            return source_lines[row][column:].encode("utf8")

        tree = parser.parse(read_callback)
        self.assertEqual(
            tree.root_node.sexp(),
            trim(
                """(module (function_definition
                name: (identifier)
                parameters: (parameters)
                body: (block (expression_statement (call
                    function: (identifier)
                    arguments: (argument_list))))))"""
            ),
        )

    def test_multibyte_characters(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        source_code = bytes("'😎' && '🐍'", "utf8")
        tree = parser.parse(source_code)
        root_node = tree.root_node
        statement_node = root_node.children[0]
        binary_node = statement_node.children[0]
        snake_node = binary_node.children[2]

        self.assertEqual(binary_node.type, "binary_expression")
        self.assertEqual(snake_node.type, "string")
        self.assertEqual(
            source_code[snake_node.start_byte:snake_node.end_byte].decode("utf8"),
            "'🐍'",
        )

    def test_buffer_protocol(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        parser.parse(b"test")
        parser.parse(memoryview(b"test"))
        parser.parse(bytearray(b"test"))

    def test_multibyte_characters_via_read_callback(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        source_code = bytes("'😎' && '🐍'", "utf8")

        def read(byte_position, point):
            return source_code[byte_position:byte_position + 1]

        tree = parser.parse(read)
        root_node = tree.root_node
        statement_node = root_node.children[0]
        binary_node = statement_node.children[0]
        snake_node = binary_node.children[2]

        self.assertEqual(binary_node.type, "binary_expression")
        self.assertEqual(snake_node.type, "string")
        self.assertEqual(
            source_code[snake_node.start_byte:snake_node.end_byte].decode("utf8"),
            "'🐍'",
        )


class TestNode(TestCase):
    def test_child_by_field_id(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")
        root_node = tree.root_node
        fn_node = tree.root_node.children[0]

        self.assertEqual(PYTHON.field_id_for_name("nameasdf"), None)
        name_field = PYTHON.field_id_for_name("name")
        alias_field = PYTHON.field_id_for_name("alias")
        self.assertIsInstance(alias_field, int)
        self.assertIsInstance(name_field, int)
        self.assertEqual(root_node.child_by_field_id(alias_field), None)
        self.assertEqual(root_node.child_by_field_id(name_field), None)
        self.assertEqual(fn_node.child_by_field_id(alias_field), None)
        self.assertEqual(fn_node.child_by_field_id(name_field).type, "identifier")
        self.assertRaises(TypeError, root_node.child_by_field_id, "")
        self.assertRaises(TypeError, root_node.child_by_field_name, True)
        self.assertRaises(TypeError, root_node.child_by_field_name, 1)

        self.assertEqual(fn_node.child_by_field_name("name").type, "identifier")
        self.assertEqual(fn_node.child_by_field_name("asdfasdfname"), None)

        self.assertEqual(
            fn_node.child_by_field_name("name"),
            fn_node.child_by_field_name("name"),
        )

    def test_children_by_field_id(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]
        attribute_field = PYTHON.field_id_for_name("attribute")

        attributes = jsx_node.children_by_field_id(attribute_field)
        self.assertEqual(
            [a.type for a in attributes], ["jsx_attribute", "jsx_attribute"]
        )

    def test_children_by_field_name(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]

        attributes = jsx_node.children_by_field_name("attribute")
        self.assertEqual(
            [a.type for a in attributes], ["jsx_attribute", "jsx_attribute"]
        )

    def test_node_child_by_field_name_with_extra_hidden_children(self):
        parser = Parser()
        parser.set_language(PYTHON)

        tree = parser.parse(b"while a:\n  pass")
        while_node = tree.root_node.child(0)
        if while_node is None:
            self.fail("while_node is None")
        self.assertEqual(while_node.type, "while_statement")
        self.assertEqual(while_node.child_by_field_name('body'), while_node.child(3))

    def test_node_descendant_count(self):
        parser = Parser()
        parser.set_language(JSON)
        tree = parser.parse(JSON_EXAMPLE)
        value_node = tree.root_node
        all_nodes = get_all_nodes(tree)

        self.assertEqual(value_node.descendant_count, len(all_nodes))

        cursor = value_node.walk()
        for i, node in enumerate(all_nodes):
            cursor.goto_descendant(i)
            self.assertEqual(cursor.node, node, f"index {i}")

        for i, node in reversed(list(enumerate(all_nodes))):
            cursor.goto_descendant(i)
            self.assertEqual(cursor.node, node, f"rev index {i}")

    def test_descendant_count_single_node_tree(self):
        parser = Parser()
        parser.set_language(EMBEDDED_TEMPLATE)
        tree = parser.parse(b"hello")

        nodes = get_all_nodes(tree)
        self.assertEqual(len(nodes), 2)
        self.assertEqual(tree.root_node.descendant_count, 2)

        cursor = tree.walk()

        cursor.goto_descendant(0)
        self.assertEqual(cursor.depth, 0)
        self.assertEqual(cursor.node, nodes[0])
        cursor.goto_descendant(1)
        self.assertEqual(cursor.depth, 1)
        self.assertEqual(cursor.node, nodes[1])

    def test_field_name_for_child(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]

        self.assertEqual(jsx_node.field_name_for_child(0), None)
        self.assertEqual(jsx_node.field_name_for_child(1), "name")

    def test_descendant_for_byte_range(self):
        parser = Parser()
        parser.set_language(JSON)
        tree = parser.parse(JSON_EXAMPLE)
        array_node = tree.root_node

        colon_index = JSON_EXAMPLE.index(b":")

        # Leaf node exactly matches the given bounds - byte query
        colon_node = array_node.descendant_for_byte_range(
            colon_index, colon_index + 1
        )
        if colon_node is None:
            self.fail("colon_node is None")
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # Leaf node exactly matches the given bounds - point query
        colon_node = array_node.descendant_for_point_range((6, 7), (6, 8))
        if colon_node is None:
            self.fail("colon_node is None")
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # The given point is between two adjacent leaf nodes - byte query
        colon_node = array_node.descendant_for_byte_range(
            colon_index, colon_index
        )
        if colon_node is None:
            self.fail("colon_node is None")
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # The given point is between two adjacent leaf nodes - point query
        colon_node = array_node.descendant_for_point_range((6, 7), (6, 7))
        if colon_node is None:
            self.fail("colon_node is None")
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # Leaf node starts at the lower bound, ends after the upper bound - byte query
        string_index = JSON_EXAMPLE.index(b'"x"')
        string_node = array_node.descendant_for_byte_range(
            string_index, string_index + 2
        )
        if string_node is None:
            self.fail("string_node is None")
        self.assertEqual(string_node.type, "string")
        self.assertEqual(string_node.start_byte, string_index)
        self.assertEqual(string_node.end_byte, string_index + 3)
        self.assertEqual(string_node.start_point, (6, 4))
        self.assertEqual(string_node.end_point, (6, 7))

        # Leaf node starts at the lower bound, ends after the upper bound - point query
        string_node = array_node.descendant_for_point_range((6, 4), (6, 6))
        if string_node is None:
            self.fail("string_node is None")
        self.assertEqual(string_node.type, "string")
        self.assertEqual(string_node.start_byte, string_index)
        self.assertEqual(string_node.end_byte, string_index + 3)
        self.assertEqual(string_node.start_point, (6, 4))
        self.assertEqual(string_node.end_point, (6, 7))

        # Leaf node starts before the lower bound, ends at the upper bound - byte query
        null_index = JSON_EXAMPLE.index(b"null")
        null_node = array_node.descendant_for_byte_range(
            null_index + 1, null_index + 4
        )
        if null_node is None:
            self.fail("null_node is None")
        self.assertEqual(null_node.type, "null")
        self.assertEqual(null_node.start_byte, null_index)
        self.assertEqual(null_node.end_byte, null_index + 4)
        self.assertEqual(null_node.start_point, (6, 9))
        self.assertEqual(null_node.end_point, (6, 13))

        # Leaf node starts before the lower bound, ends at the upper bound - point query
        null_node = array_node.descendant_for_point_range((6, 11), (6, 13))
        if null_node is None:
            self.fail("null_node is None")
        self.assertEqual(null_node.type, "null")
        self.assertEqual(null_node.start_byte, null_index)
        self.assertEqual(null_node.end_byte, null_index + 4)
        self.assertEqual(null_node.start_point, (6, 9))
        self.assertEqual(null_node.end_point, (6, 13))

        # The bounds span multiple leaf nodes - return the smallest node that does span it.
        pair_node = array_node.descendant_for_byte_range(
            string_index + 2, string_index + 4
        )
        if pair_node is None:
            self.fail("pair_node is None")
        self.assertEqual(pair_node.type, "pair")
        self.assertEqual(pair_node.start_byte, string_index)
        self.assertEqual(pair_node.end_byte, string_index + 9)
        self.assertEqual(pair_node.start_point, (6, 4))
        self.assertEqual(pair_node.end_point, (6, 13))

        self.assertEqual(colon_node.parent, pair_node)

        # No leaf spans the given range - return the smallest node that does span it.
        pair_node = array_node.descendant_for_point_range((6, 6), (6, 8))
        if pair_node is None:
            self.fail("pair_node is None")
        self.assertEqual(pair_node.type, "pair")
        self.assertEqual(pair_node.start_byte, string_index)
        self.assertEqual(pair_node.end_byte, string_index + 9)
        self.assertEqual(pair_node.start_point, (6, 4))
        self.assertEqual(pair_node.end_point, (6, 13))

    def test_root_node_with_offset(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"  if (a) b")

        node = tree.root_node_with_offset(6, (2, 2))
        if node is None:
            self.fail("node is None")
        self.assertEqual(node.byte_range, (8, 16))
        self.assertEqual(node.start_point, (2, 4))
        self.assertEqual(node.end_point, (2, 12))

        child = node.child(0).child(2)
        if child is None:
            self.fail("child is None")
        self.assertEqual(child.type, "expression_statement")
        self.assertEqual(child.byte_range, (15, 16))
        self.assertEqual(child.start_point, (2, 11))
        self.assertEqual(child.end_point, (2, 12))

        cursor = node.walk()
        cursor.goto_first_child()
        cursor.goto_first_child()
        cursor.goto_next_sibling()
        child = cursor.node
        if child is None:
            self.fail("child is None")
        self.assertEqual(child.type, "parenthesized_expression")
        self.assertEqual(child.byte_range, (11, 14))
        self.assertEqual(child.start_point, (2, 7))
        self.assertEqual(child.end_point, (2, 10))

    def test_node_is_extra(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        tree = parser.parse(b"foo(/* hi */);")

        root_node = tree.root_node
        comment_node = root_node.descendant_for_byte_range(7, 7)
        if comment_node is None:
            self.fail("comment_node is None")

        self.assertEqual(root_node.type, "program")
        self.assertEqual(comment_node.type, "comment")
        self.assertEqual(root_node.is_extra, False)
        self.assertEqual(comment_node.is_extra, True)

    def test_children(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")

        root_node = tree.root_node
        self.assertEqual(root_node.type, "module")
        self.assertEqual(root_node.start_byte, 0)
        self.assertEqual(root_node.end_byte, 18)
        self.assertEqual(root_node.start_point, (0, 0))
        self.assertEqual(root_node.end_point, (1, 7))

        # List object is reused
        self.assertIs(root_node.children, root_node.children)

        fn_node = root_node.children[0]
        self.assertEqual(fn_node, root_node.child(0))
        self.assertEqual(fn_node.type, "function_definition")
        self.assertEqual(fn_node.start_byte, 0)
        self.assertEqual(fn_node.end_byte, 18)
        self.assertEqual(fn_node.start_point, (0, 0))
        self.assertEqual(fn_node.end_point, (1, 7))

        def_node = fn_node.children[0]
        self.assertEqual(def_node, fn_node.child(0))
        self.assertEqual(def_node.type, "def")
        self.assertEqual(def_node.is_named, False)

        id_node = fn_node.children[1]
        self.assertEqual(id_node, fn_node.child(1))
        self.assertEqual(id_node.type, "identifier")
        self.assertEqual(id_node.is_named, True)
        self.assertEqual(len(id_node.children), 0)

        params_node = fn_node.children[2]
        self.assertEqual(params_node, fn_node.child(2))
        self.assertEqual(params_node.type, "parameters")
        self.assertEqual(params_node.is_named, True)

        colon_node = fn_node.children[3]
        self.assertEqual(colon_node, fn_node.child(3))
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.is_named, False)

        statement_node = fn_node.children[4]
        self.assertEqual(statement_node, fn_node.child(4))
        self.assertEqual(statement_node.type, "block")
        self.assertEqual(statement_node.is_named, True)

    def test_named_and_sibling_and_count_and_parent(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"[1, 2, 3]")

        root_node = tree.root_node
        self.assertEqual(root_node.type, "module")
        self.assertEqual(root_node.start_byte, 0)
        self.assertEqual(root_node.end_byte, 9)
        self.assertEqual(root_node.start_point, (0, 0))
        self.assertEqual(root_node.end_point, (0, 9))

        exp_stmt_node = root_node.children[0]
        self.assertEqual(exp_stmt_node, root_node.child(0))
        self.assertEqual(exp_stmt_node.type, "expression_statement")
        self.assertEqual(exp_stmt_node.start_byte, 0)
        self.assertEqual(exp_stmt_node.end_byte, 9)
        self.assertEqual(exp_stmt_node.start_point, (0, 0))
        self.assertEqual(exp_stmt_node.end_point, (0, 9))
        self.assertEqual(exp_stmt_node.parent, root_node)

        list_node = exp_stmt_node.children[0]
        self.assertEqual(list_node, exp_stmt_node.child(0))
        self.assertEqual(list_node.type, "list")
        self.assertEqual(list_node.start_byte, 0)
        self.assertEqual(list_node.end_byte, 9)
        self.assertEqual(list_node.start_point, (0, 0))
        self.assertEqual(list_node.end_point, (0, 9))
        self.assertEqual(list_node.parent, exp_stmt_node)

        named_children = list_node.named_children

        open_delim_node = list_node.children[0]
        self.assertEqual(open_delim_node, list_node.child(0))
        self.assertEqual(open_delim_node.type, "[")
        self.assertEqual(open_delim_node.start_byte, 0)
        self.assertEqual(open_delim_node.end_byte, 1)
        self.assertEqual(open_delim_node.start_point, (0, 0))
        self.assertEqual(open_delim_node.end_point, (0, 1))
        self.assertEqual(open_delim_node.parent, list_node)

        first_num_node = list_node.children[1]
        self.assertEqual(first_num_node, list_node.child(1))
        self.assertEqual(first_num_node, open_delim_node.next_named_sibling)
        self.assertEqual(first_num_node.parent, list_node)
        self.assertEqual(named_children[0], first_num_node)
        self.assertEqual(first_num_node, list_node.named_child(0))

        first_comma_node = list_node.children[2]
        self.assertEqual(first_comma_node, list_node.child(2))
        self.assertEqual(first_comma_node, first_num_node.next_sibling)
        self.assertEqual(first_num_node, first_comma_node.prev_sibling)
        self.assertEqual(first_comma_node.parent, list_node)

        second_num_node = list_node.children[3]
        self.assertEqual(second_num_node, list_node.child(3))
        self.assertEqual(second_num_node, first_comma_node.next_sibling)
        self.assertEqual(second_num_node, first_num_node.next_named_sibling)
        self.assertEqual(first_num_node, second_num_node.prev_named_sibling)
        self.assertEqual(second_num_node.parent, list_node)
        self.assertEqual(named_children[1], second_num_node)
        self.assertEqual(second_num_node, list_node.named_child(1))

        second_comma_node = list_node.children[4]
        self.assertEqual(second_comma_node, list_node.child(4))
        self.assertEqual(second_comma_node, second_num_node.next_sibling)
        self.assertEqual(second_num_node, second_comma_node.prev_sibling)
        self.assertEqual(second_comma_node.parent, list_node)

        third_num_node = list_node.children[5]
        self.assertEqual(third_num_node, list_node.child(5))
        self.assertEqual(third_num_node, second_comma_node.next_sibling)
        self.assertEqual(third_num_node, second_num_node.next_named_sibling)
        self.assertEqual(second_num_node, third_num_node.prev_named_sibling)
        self.assertEqual(third_num_node.parent, list_node)
        self.assertEqual(named_children[2], third_num_node)
        self.assertEqual(third_num_node, list_node.named_child(2))

        close_delim_node = list_node.children[6]
        self.assertEqual(close_delim_node, list_node.child(6))
        self.assertEqual(close_delim_node.type, "]")
        self.assertEqual(close_delim_node.start_byte, 8)
        self.assertEqual(close_delim_node.end_byte, 9)
        self.assertEqual(close_delim_node.start_point, (0, 8))
        self.assertEqual(close_delim_node.end_point, (0, 9))
        self.assertEqual(close_delim_node, third_num_node.next_sibling)
        self.assertEqual(third_num_node, close_delim_node.prev_sibling)
        self.assertEqual(third_num_node, close_delim_node.prev_named_sibling)
        self.assertEqual(close_delim_node.parent, list_node)

        self.assertEqual(list_node.child_count, 7)
        self.assertEqual(list_node.named_child_count, 3)

    def test_node_text(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"[0, [1, 2, 3]]")

        self.assertEqual(tree.text, b"[0, [1, 2, 3]]")

        root_node = tree.root_node
        self.assertEqual(root_node.text, b"[0, [1, 2, 3]]")

        exp_stmt_node = root_node.children[0]
        self.assertEqual(exp_stmt_node.text, b"[0, [1, 2, 3]]")

        list_node = exp_stmt_node.children[0]
        self.assertEqual(list_node.text, b"[0, [1, 2, 3]]")

        open_delim_node = list_node.children[0]
        self.assertEqual(open_delim_node.text, b"[")

        first_num_node = list_node.children[1]
        self.assertEqual(first_num_node.text, b"0")

        first_comma_node = list_node.children[2]
        self.assertEqual(first_comma_node.text, b",")

        child_list_node = list_node.children[3]
        self.assertEqual(child_list_node.text, b"[1, 2, 3]")

        close_delim_node = list_node.children[4]
        self.assertEqual(close_delim_node.text, b"]")

        edit_offset = len(b"[0, [")
        tree.edit(
            start_byte=edit_offset,
            old_end_byte=edit_offset,
            new_end_byte=edit_offset + 2,
            start_point=(0, edit_offset),
            old_end_point=(0, edit_offset),
            new_end_point=(0, edit_offset + 2),
        )
        self.assertEqual(tree.text, None)

        root_node_again = tree.root_node
        self.assertEqual(root_node_again.text, None)

        tree_text_false = parser.parse(b"[0, [1, 2, 3]]", keep_text=False)
        self.assertIsNone(tree_text_false.text)
        root_node_text_false = tree_text_false.root_node
        self.assertIsNone(root_node_text_false.text)

        tree_text_true = parser.parse(b"[0, [1, 2, 3]]", keep_text=True)
        self.assertEqual(tree_text_true.text, b"[0, [1, 2, 3]]")
        root_node_text_true = tree_text_true.root_node
        self.assertEqual(root_node_text_true.text, b"[0, [1, 2, 3]]")

    def test_tree(self):
        code = b"def foo():\n  bar()\n\ndef foo():\n  bar()"
        parser = Parser()
        parser.set_language(PYTHON)

        def parse_root(bytes_):
            tree = parser.parse(bytes_)
            return tree.root_node

        root = parse_root(code)
        for item in root.children:
            self.assertIsNotNone(item.is_named)

        def parse_root_children(bytes_):
            tree = parser.parse(bytes_)
            return tree.root_node.children

        children = parse_root_children(code)
        for item in children:
            self.assertIsNotNone(item.is_named)


class TestTree(TestCase):
    def test_tree_cursor_without_tree(self):
        parser = Parser()
        parser.set_language(PYTHON)

        def parse():
            tree = parser.parse(b"def foo():\n  bar()")
            return tree.walk()

        cursor = parse()
        self.assertIs(cursor.node, cursor.node)
        for item in cursor.node.children:
            self.assertIsNotNone(item.is_named)

        cursor = cursor.copy()
        self.assertIs(cursor.node, cursor.node)
        for item in cursor.node.children:
            self.assertIsNotNone(item.is_named)

    def test_walk(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")
        cursor = tree.walk()

        # Node always returns the same instance
        self.assertIs(cursor.node, cursor.node)

        self.assertEqual(cursor.node.type, "module")
        self.assertEqual(cursor.node.start_byte, 0)
        self.assertEqual(cursor.node.end_byte, 18)
        self.assertEqual(cursor.node.start_point, (0, 0))
        self.assertEqual(cursor.node.end_point, (1, 7))
        self.assertEqual(cursor.field_name, None)

        self.assertTrue(cursor.goto_first_child())
        self.assertEqual(cursor.node.type, "function_definition")
        self.assertEqual(cursor.node.start_byte, 0)
        self.assertEqual(cursor.node.end_byte, 18)
        self.assertEqual(cursor.node.start_point, (0, 0))
        self.assertEqual(cursor.node.end_point, (1, 7))
        self.assertEqual(cursor.field_name, None)

        self.assertTrue(cursor.goto_first_child())
        self.assertEqual(cursor.node.type, "def")
        self.assertEqual(cursor.node.is_named, False)
        self.assertEqual(cursor.node.sexp(), '("def")')
        self.assertEqual(cursor.field_name, None)
        def_node = cursor.node

        # Node remains cached after a failure to move
        self.assertFalse(cursor.goto_first_child())
        self.assertIs(cursor.node, def_node)

        self.assertTrue(cursor.goto_next_sibling())
        self.assertEqual(cursor.node.type, "identifier")
        self.assertEqual(cursor.node.is_named, True)
        self.assertEqual(cursor.field_name, "name")
        self.assertFalse(cursor.goto_first_child())

        self.assertTrue(cursor.goto_next_sibling())
        self.assertEqual(cursor.node.type, "parameters")
        self.assertEqual(cursor.node.is_named, True)
        self.assertEqual(cursor.field_name, "parameters")

    def test_edit(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")

        edit_offset = len(b"def foo(")
        tree.edit(
            start_byte=edit_offset,
            old_end_byte=edit_offset,
            new_end_byte=edit_offset + 2,
            start_point=(0, edit_offset),
            old_end_point=(0, edit_offset),
            new_end_point=(0, edit_offset + 2),
        )

        fn_node = tree.root_node.children[0]
        self.assertEqual(fn_node.type, "function_definition")
        self.assertTrue(fn_node.has_changes)
        self.assertFalse(fn_node.children[0].has_changes)
        self.assertFalse(fn_node.children[1].has_changes)
        self.assertFalse(fn_node.children[3].has_changes)

        params_node = fn_node.children[2]
        self.assertEqual(params_node.type, "parameters")
        self.assertTrue(params_node.has_changes)
        self.assertEqual(params_node.start_point, (0, edit_offset - 1))
        self.assertEqual(params_node.end_point, (0, edit_offset + 3))

        new_tree = parser.parse(b"def foo(ab):\n  bar()", tree)
        self.assertEqual(
            new_tree.root_node.sexp(),
            trim(
                """(module (function_definition
                name: (identifier)
                parameters: (parameters (identifier))
                body: (block
                    (expression_statement (call
                        function: (identifier)
                        arguments: (argument_list))))))"""
            ),
        )

    def test_get_changed_ranges(self):
        parser = Parser()
        parser.set_language(PYTHON)
        tree = parser.parse(b"def foo():\n  bar()")

        edit_offset = len(b"def foo(")
        tree.edit(
            start_byte=edit_offset,
            old_end_byte=edit_offset,
            new_end_byte=edit_offset + 2,
            start_point=(0, edit_offset),
            old_end_point=(0, edit_offset),
            new_end_point=(0, edit_offset + 2),
        )

        new_tree = parser.parse(b"def foo(ab):\n  bar()", tree)
        changed_ranges = tree.get_changed_ranges(new_tree)

        self.assertEqual(len(changed_ranges), 1)
        self.assertEqual(changed_ranges[0].start_byte, edit_offset)
        self.assertEqual(changed_ranges[0].start_point, (0, edit_offset))
        self.assertEqual(changed_ranges[0].end_byte, edit_offset + 2)
        self.assertEqual(changed_ranges[0].end_point, (0, edit_offset + 2))


class TestQuery(TestCase):
    def test_errors(self):
        with self.assertRaisesRegex(NameError, "Invalid node type foo"):
            PYTHON.query("(list (foo))")
        with self.assertRaisesRegex(NameError, "Invalid field name buzz"):
            PYTHON.query("(function_definition buzz: (identifier))")
        with self.assertRaisesRegex(NameError, "Invalid capture name garbage"):
            PYTHON.query("((function_definition) (eq? @garbage foo))")
        with self.assertRaisesRegex(SyntaxError, "Invalid syntax at offset 6"):
            PYTHON.query("(list))")
        PYTHON.query("(function_definition)")

    def test_captures(self):
        parser = Parser()
        parser.set_language(PYTHON)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = PYTHON.query(
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )

        captures = query.captures(tree.root_node)

        self.assertEqual(captures[0][0].start_point, (0, 4))
        self.assertEqual(captures[0][0].end_point, (0, 7))
        self.assertEqual(captures[0][1], "func-def")

        self.assertEqual(captures[1][0].start_point, (1, 2))
        self.assertEqual(captures[1][0].end_point, (1, 5))
        self.assertEqual(captures[1][1], "func-call")

        self.assertEqual(captures[2][0].start_point, (2, 4))
        self.assertEqual(captures[2][0].end_point, (2, 7))
        self.assertEqual(captures[2][1], "func-def")

        self.assertEqual(captures[3][0].start_point, (3, 2))
        self.assertEqual(captures[3][0].end_point, (3, 6))
        self.assertEqual(captures[3][1], "func-call")

    def test_text_predicates(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        source = b"""
            keypair_object = {
                key1: value1,
                equal: equal
            }

            function fun1(arg) {
                return 1;
            }

            function fun2(arg) {
                return 2;
            }
        """
        tree = parser.parse(source)
        root_node = tree.root_node

        # function with name equal to 'fun1' -> test for #eq? @capture string
        query1 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
            )
            (#eq? @function-name fun1)
        )
        """
        )
        captures1 = query1.captures(root_node)
        self.assertEqual(1, len(captures1))
        self.assertEqual(b"fun1", captures1[0][0].text)
        self.assertEqual("function-name", captures1[0][1])

        # functions with name not equal to 'fun1' -> test for #not-eq? @capture string
        query2 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
            )
            (#not-eq? @function-name fun1)
        )
        """
        )
        captures2 = query2.captures(root_node)
        self.assertEqual(1, len(captures2))
        self.assertEqual(b"fun2", captures2[0][0].text)
        self.assertEqual("function-name", captures2[0][1])

        # key pairs whose key is equal to its value -> test for #eq? @capture1 @capture2
        query3 = JAVASCRIPT.query(
            """
                    (
                      (pair
                        key: (property_identifier) @key-name
                        value: (identifier) @value-name)
                      (#eq? @key-name @value-name)
                    )
                    """
        )
        captures3 = query3.captures(root_node)
        self.assertEqual(2, len(captures3))
        self.assertSetEqual({b"equal"}, set([c[0].text for c in captures3]))
        self.assertSetEqual({"key-name", "value-name"}, set([c[1] for c in captures3]))

        # key pairs whose key is not equal to its value
        # -> test for #not-eq? @capture1 @capture2
        query4 = JAVASCRIPT.query(
            """
                    (
                      (pair
                        key: (property_identifier) @key-name
                        value: (identifier) @value-name)
                      (#not-eq? @key-name @value-name)
                    )
                    """
        )
        captures4 = query4.captures(root_node)
        self.assertEqual(2, len(captures4))
        self.assertSetEqual({b"key1", b"value1"}, set([c[0].text for c in captures4]))
        self.assertSetEqual({"key-name", "value-name"}, set([c[1] for c in captures4]))

        # equality that is satisfied by *another* capture
        query5 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters (identifier) @parameter-name)
            )
            (#eq? @function-name arg)
        )
        """
        )
        captures5 = query5.captures(root_node)
        self.assertEqual(0, len(captures5))

        # functions that match the regex .*1 -> test for #match @capture regex
        query6 = JAVASCRIPT.query(
            """
                (
                    (function_declaration
                        name: (identifier) @function-name
                    )
                    (#match? @function-name ".*1")
                )
                """
        )
        captures6 = query6.captures(root_node)
        self.assertEqual(1, len(captures6))
        self.assertEqual(b"fun1", captures6[0][0].text)

        # functions that do not match the regex .*1 -> test for #not-match @capture regex
        query6 = JAVASCRIPT.query(
            """
                        (
                            (function_declaration
                                name: (identifier) @function-name
                            )
                            (#not-match? @function-name ".*1")
                        )
                        """
        )
        captures6 = query6.captures(root_node)
        self.assertEqual(1, len(captures6))
        self.assertEqual(b"fun2", captures6[0][0].text)

        # after editing there is no text property, so predicates are ignored
        tree.edit(
            start_byte=0,
            old_end_byte=0,
            new_end_byte=2,
            start_point=(0, 0),
            old_end_point=(0, 0),
            new_end_point=(0, 2),
        )
        captures_notext = query1.captures(root_node)
        self.assertEqual(2, len(captures_notext))
        self.assertSetEqual({"function-name"}, set([c[1] for c in captures_notext]))

    def test_text_predicates_errors(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        with self.assertRaises(RuntimeError):
            JAVASCRIPT.query(
                """
            (
                (function_declaration
                    name: (identifier) @function-name
                )
                (#eq? @function-name @function-name fun1)
            )
            """
            )

        with self.assertRaises(RuntimeError):
            JAVASCRIPT.query(
                """
            (
                (function_declaration
                    name: (identifier) @function-name
                )
                (#eq? fun1 @function-name)
            )
            """
            )

        with self.assertRaises(RuntimeError):
            JAVASCRIPT.query(
                """
            (
                (function_declaration
                    name: (identifier) @function-name
                )
                (#match? @function-name @function-name fun1)
            )
            """
            )

        with self.assertRaises(RuntimeError):
            JAVASCRIPT.query(
                """
            (
                (function_declaration
                    name: (identifier) @function-name
                )
                (#match? fun1 @function-name)
            )
            """
            )

        with self.assertRaises(RuntimeError):
            JAVASCRIPT.query(
                """
            (
                (function_declaration
                    name: (identifier) @function-name
                )
                (#match? @function-name @function-name)
            )
            """
            )

    def test_multiple_text_predicates(self):
        parser = Parser()
        parser.set_language(JAVASCRIPT)
        source = b"""
            keypair_object = {
                key1: value1,
                equal: equal
            }

            function fun1(arg) {
                return 1;
            }

            function fun1(notarg) {
                return 1 + 1;
            }

            function fun2(arg) {
                return 2;
            }
        """
        tree = parser.parse(source)
        root_node = tree.root_node

        # function with name equal to 'fun1' -> test for first #eq? @capture string
        query1 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name
                )
            )
            (#eq? @function-name fun1)
        )
        """
        )
        captures1 = query1.captures(root_node)
        self.assertEqual(4, len(captures1))
        self.assertEqual(b"fun1", captures1[0][0].text)
        self.assertEqual("function-name", captures1[0][1])
        self.assertEqual(b"arg", captures1[1][0].text)
        self.assertEqual("argument-name", captures1[1][1])
        self.assertEqual(b"fun1", captures1[2][0].text)
        self.assertEqual("function-name", captures1[2][1])
        self.assertEqual(b"notarg", captures1[3][0].text)
        self.assertEqual("argument-name", captures1[3][1])

        # function with argument equal to 'arg' -> test for second #eq? @capture string
        query2 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name
                )
            )
            (#eq? @argument-name arg)
        )
        """
        )
        captures2 = query2.captures(root_node)
        self.assertEqual(4, len(captures2))
        self.assertEqual(b"fun1", captures2[0][0].text)
        self.assertEqual("function-name", captures2[0][1])
        self.assertEqual(b"arg", captures2[1][0].text)
        self.assertEqual("argument-name", captures2[1][1])
        self.assertEqual(b"fun2", captures2[2][0].text)
        self.assertEqual("function-name", captures2[2][1])
        self.assertEqual(b"arg", captures2[3][0].text)
        self.assertEqual("argument-name", captures2[3][1])

        # function with name equal to 'fun1' & argument 'arg' -> test for both together
        query3 = JAVASCRIPT.query(
            """
        (
            (function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name
                )
            )
            (#eq? @function-name fun1)
            (#eq? @argument-name arg)
        )
        """
        )
        captures3 = query3.captures(root_node)
        self.assertEqual(2, len(captures3))
        self.assertEqual(b"fun1", captures3[0][0].text)
        self.assertEqual("function-name", captures3[0][1])
        self.assertEqual(b"arg", captures3[1][0].text)
        self.assertEqual("argument-name", captures3[1][1])

    def test_byte_range_captures(self):
        parser = Parser()
        parser.set_language(PYTHON)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = PYTHON.query(
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )

        captures = query.captures(tree.root_node, start_byte=10, end_byte=20)
        self.assertEqual(captures[0][0].start_point, (1, 2))
        self.assertEqual(captures[0][0].end_point, (1, 5))
        self.assertEqual(captures[0][1], "func-call")

    def test_point_range_captures(self):
        parser = Parser()
        parser.set_language(PYTHON)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = PYTHON.query(
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )

        captures = query.captures(tree.root_node, start_point=(1, 0), end_point=(2, 0))
        # FIXME: this test is incorrect
        self.assertEqual(captures[1][0].start_point, (1, 2))
        self.assertEqual(captures[1][0].end_point, (1, 5))
        self.assertEqual(captures[1][1], "func-call")


class TestLookaheadIterator(TestCase):
    def test_lookahead_iterator(self):
        parser = Parser()
        parser.set_language(RUST)
        tree = parser.parse(b"struct Stuff{}")

        cursor = tree.walk()

        self.assertEqual(cursor.goto_first_child(), True)  # struct
        self.assertEqual(cursor.goto_first_child(), True)  # struct keyword

        next_state = cursor.node.next_parse_state

        self.assertNotEqual(next_state, 0)
        self.assertEqual(
            next_state,
            RUST.next_state(cursor.node.parse_state, cursor.node.grammar_id)
        )
        self.assertLess(next_state, RUST.parse_state_count)
        self.assertEqual(cursor.goto_next_sibling(), True)  # type_identifier
        self.assertEqual(next_state, cursor.node.parse_state)
        self.assertEqual(cursor.node.grammar_name, "identifier")
        self.assertNotEqual(cursor.node.grammar_id, cursor.node.kind_id)

        expected_symbols = ["identifier", "block_comment", "line_comment"]
        lookahead: LookaheadIterator = RUST.lookahead_iterator(next_state)
        self.assertEqual(lookahead.language, RUST.language_id)
        self.assertEqual(list(lookahead.iter_names()), expected_symbols)

        lookahead.reset_state(next_state)
        self.assertEqual(list(lookahead.iter_names()), expected_symbols)

        lookahead.reset(RUST.language_id, next_state)
        self.assertEqual(list(map(RUST.node_kind_for_id, list(iter(lookahead)))), expected_symbols)


def trim(string):
    return re.sub(r"\s+", " ", string).strip()


def get_all_nodes(tree: Tree) -> List[Node]:
    result = []
    visited_children = False
    cursor = tree.walk()
    while True:
        if not visited_children:
            result.append(cursor.node)
            if not cursor.goto_first_child():
                visited_children = True
        elif cursor.goto_next_sibling():
            visited_children = False
        elif not cursor.goto_parent():
            break
    return result
