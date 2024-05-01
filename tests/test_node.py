from unittest import TestCase

import tree_sitter_python
import tree_sitter_javascript
import tree_sitter_json

from tree_sitter import Language, Parser

JSON_EXAMPLE = b"""

[
  123,
  false,
  {
    "x": null
  }
]
"""


def get_all_nodes(tree):
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


class TestNode(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.javascript = Language(tree_sitter_javascript.language())
        cls.json = Language(tree_sitter_json.language())
        cls.python = Language(tree_sitter_python.language())

    def test_child_by_field_id(self):
        parser = Parser(self.python)
        tree = parser.parse(b"def foo():\n  bar()")
        root_node = tree.root_node
        fn_node = tree.root_node.children[0]

        self.assertIsNone(self.python.field_id_for_name("noname"))
        name_field = self.python.field_id_for_name("name")
        alias_field = self.python.field_id_for_name("alias")
        self.assertIsNone(root_node.child_by_field_id(alias_field))
        self.assertIsNone(root_node.child_by_field_id(name_field))
        self.assertIsNone(fn_node.child_by_field_id(alias_field))
        self.assertIsNone(fn_node.child_by_field_name("noname"))
        self.assertEqual(fn_node.child_by_field_name("name"), fn_node.child_by_field_name("name"))

    def test_child_by_field_name(self):
        parser = Parser(self.python)
        tree = parser.parse(b"while a:\n  pass")
        while_node = tree.root_node.child(0)
        self.assertIsNotNone(while_node)
        self.assertEqual(while_node.type, "while_statement")
        self.assertEqual(while_node.child_by_field_name("body"), while_node.child(3))

    def test_children_by_field_id(self):
        parser = Parser(self.javascript)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]
        attribute_field = self.javascript.field_id_for_name("attribute")
        attributes = jsx_node.children_by_field_id(attribute_field)
        self.assertListEqual([a.type for a in attributes], ["jsx_attribute", "jsx_attribute"])

    def test_children_by_field_name(self):
        parser = Parser(self.javascript)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]
        attributes = jsx_node.children_by_field_name("attribute")
        self.assertListEqual([a.type for a in attributes], ["jsx_attribute", "jsx_attribute"])

    def test_field_name_for_child(self):
        parser = Parser(self.javascript)
        tree = parser.parse(b"<div a={1} b={2} />")
        jsx_node = tree.root_node.children[0].children[0]

        self.assertIsNone(jsx_node.field_name_for_child(0))
        self.assertEqual(jsx_node.field_name_for_child(1), "name")

    def test_root_node_with_offset(self):
        parser = Parser(self.javascript)
        tree = parser.parse(b"  if (a) b")

        node = tree.root_node_with_offset(6, (2, 2))
        self.assertIsNotNone(node)
        self.assertEqual(node.byte_range, (8, 16))
        self.assertEqual(node.start_point, (2, 4))
        self.assertEqual(node.end_point, (2, 12))

        child = node.child(0).child(2)
        self.assertIsNotNone(child)
        self.assertEqual(child.type, "expression_statement")
        self.assertEqual(child.byte_range, (15, 16))
        self.assertEqual(child.start_point, (2, 11))
        self.assertEqual(child.end_point, (2, 12))

        cursor = node.walk()
        cursor.goto_first_child()
        cursor.goto_first_child()
        cursor.goto_next_sibling()
        child = cursor.node
        self.assertIsNotNone(child)
        self.assertEqual(child.type, "parenthesized_expression")
        self.assertEqual(child.byte_range, (11, 14))
        self.assertEqual(child.start_point, (2, 7))
        self.assertEqual(child.end_point, (2, 10))

    def test_descendant_count(self):
        parser = Parser(self.json)
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

    def test_descendant_for_byte_range(self):
        parser = Parser(self.json)
        tree = parser.parse(JSON_EXAMPLE)
        array_node = tree.root_node

        colon_index = JSON_EXAMPLE.index(b":")

        # Leaf node exactly matches the given bounds - byte query
        colon_node = array_node.descendant_for_byte_range(colon_index, colon_index + 1)
        self.assertIsNotNone(colon_node)
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # Leaf node exactly matches the given bounds - point query
        colon_node = array_node.descendant_for_point_range((6, 7), (6, 8))
        self.assertIsNotNone(colon_node)
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # The given point is between two adjacent leaf nodes - byte query
        colon_node = array_node.descendant_for_byte_range(colon_index, colon_index)
        self.assertIsNotNone(colon_node)
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # The given point is between two adjacent leaf nodes - point query
        colon_node = array_node.descendant_for_point_range((6, 7), (6, 7))
        self.assertIsNotNone(colon_node)
        self.assertEqual(colon_node.type, ":")
        self.assertEqual(colon_node.start_byte, colon_index)
        self.assertEqual(colon_node.end_byte, colon_index + 1)
        self.assertEqual(colon_node.start_point, (6, 7))
        self.assertEqual(colon_node.end_point, (6, 8))

        # Leaf node starts at the lower bound, ends after the upper bound - byte query
        string_index = JSON_EXAMPLE.index(b'"x"')
        string_node = array_node.descendant_for_byte_range(string_index, string_index + 2)
        self.assertIsNotNone(string_node)
        self.assertEqual(string_node.type, "string")
        self.assertEqual(string_node.start_byte, string_index)
        self.assertEqual(string_node.end_byte, string_index + 3)
        self.assertEqual(string_node.start_point, (6, 4))
        self.assertEqual(string_node.end_point, (6, 7))

        # Leaf node starts at the lower bound, ends after the upper bound - point query
        string_node = array_node.descendant_for_point_range((6, 4), (6, 6))
        self.assertIsNotNone(string_node)
        self.assertEqual(string_node.type, "string")
        self.assertEqual(string_node.start_byte, string_index)
        self.assertEqual(string_node.end_byte, string_index + 3)
        self.assertEqual(string_node.start_point, (6, 4))
        self.assertEqual(string_node.end_point, (6, 7))

        # Leaf node starts before the lower bound, ends at the upper bound - byte query
        null_index = JSON_EXAMPLE.index(b"null")
        null_node = array_node.descendant_for_byte_range(null_index + 1, null_index + 4)
        self.assertIsNotNone(null_node)
        self.assertEqual(null_node.type, "null")
        self.assertEqual(null_node.start_byte, null_index)
        self.assertEqual(null_node.end_byte, null_index + 4)
        self.assertEqual(null_node.start_point, (6, 9))
        self.assertEqual(null_node.end_point, (6, 13))

        # Leaf node starts before the lower bound, ends at the upper bound - point query
        null_node = array_node.descendant_for_point_range((6, 11), (6, 13))
        self.assertIsNotNone(null_node)
        self.assertEqual(null_node.type, "null")
        self.assertEqual(null_node.start_byte, null_index)
        self.assertEqual(null_node.end_byte, null_index + 4)
        self.assertEqual(null_node.start_point, (6, 9))
        self.assertEqual(null_node.end_point, (6, 13))

        # The bounds span multiple leaf nodes - return the smallest node that does span it.
        pair_node = array_node.descendant_for_byte_range(string_index + 2, string_index + 4)
        self.assertIsNotNone(pair_node)
        self.assertEqual(pair_node.type, "pair")
        self.assertEqual(pair_node.start_byte, string_index)
        self.assertEqual(pair_node.end_byte, string_index + 9)
        self.assertEqual(pair_node.start_point, (6, 4))
        self.assertEqual(pair_node.end_point, (6, 13))

        self.assertEqual(colon_node.parent, pair_node)

        # No leaf spans the given range - return the smallest node that does span it.
        pair_node = array_node.descendant_for_point_range((6, 6), (6, 8))
        self.assertIsNotNone(pair_node)
        self.assertEqual(pair_node.type, "pair")
        self.assertEqual(pair_node.start_byte, string_index)
        self.assertEqual(pair_node.end_byte, string_index + 9)
        self.assertEqual(pair_node.start_point, (6, 4))
        self.assertEqual(pair_node.end_point, (6, 13))

    def test_children(self):
        parser = Parser(self.python)
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

    def test_is_extra(self):
        parser = Parser(self.javascript)
        tree = parser.parse(b"foo(/* hi */);")

        root_node = tree.root_node
        comment_node = root_node.descendant_for_byte_range(7, 7)
        self.assertIsNotNone(comment_node)

        self.assertEqual(root_node.type, "program")
        self.assertEqual(comment_node.type, "comment")
        self.assertEqual(root_node.is_extra, False)
        self.assertEqual(comment_node.is_extra, True)

    def test_properties(self):
        parser = Parser(self.python)
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

    def test_numeric_symbols_respect_simple_aliases(self):
        parser = Parser(self.python)

        # Example 1:
        # Python argument lists can contain "splat" arguments, which are not allowed within
        # other expressions. This includes `parenthesized_list_splat` nodes like `(*b)`. These
        # `parenthesized_list_splat` nodes are aliased as `parenthesized_expression`. Their numeric
        # `symbol`, aka `kind_id` should match that of a normal `parenthesized_expression`.
        tree = parser.parse(b"(a((*b)))")
        root_node = tree.root_node
        self.assertEqual(
            str(root_node),
            "(module (expression_statement (parenthesized_expression (call "
            + "function: (identifier) arguments: (argument_list (parenthesized_expression "
            + "(list_splat (identifier))))))))",
        )

        outer_expr_node = root_node.child(0).child(0)
        self.assertIsNotNone(outer_expr_node)
        self.assertEqual(outer_expr_node.type, "parenthesized_expression")

        inner_expr_node = (
            outer_expr_node.named_child(0).child_by_field_name("arguments").named_child(0)
        )
        self.assertIsNotNone(inner_expr_node)

        self.assertEqual(inner_expr_node.type, "parenthesized_expression")
        self.assertEqual(inner_expr_node.kind_id, outer_expr_node.kind_id)

    def test_tree(self):
        code = b"def foo():\n  bar()\n\ndef foo():\n  bar()"
        parser = Parser(self.python)

        for item in parser.parse(code).root_node.children:
            self.assertIsNotNone(item.is_named)

        for item in parser.parse(code).root_node.children:
            self.assertIsNotNone(item.is_named)

    def test_text(self):
        parser = Parser(self.python)
        tree = parser.parse(b"[0, [1, 2, 3]]")

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

    def test_hash(self):
        parser = Parser(self.python)
        source_code = b"def foo():\n  bar()\n  bar()"
        tree = parser.parse(source_code)
        root_node = tree.root_node
        first_function_node = root_node.children[0]
        second_function_node = root_node.children[0]

        # Uniqueness and consistency
        self.assertEqual(hash(first_function_node), hash(first_function_node))
        self.assertNotEqual(hash(root_node), hash(first_function_node))

        # Equality implication
        self.assertEqual(hash(first_function_node), hash(second_function_node))
        self.assertEqual(first_function_node, second_function_node)

        # Different nodes with different properties
        different_tree = parser.parse(b"def baz():\n  qux()")
        different_node = different_tree.root_node.children[0]
        self.assertNotEqual(hash(first_function_node), hash(different_node))

        # Same code, different parse trees
        another_tree = parser.parse(source_code)
        another_node = another_tree.root_node.children[0]
        self.assertNotEqual(hash(first_function_node), hash(another_node))
