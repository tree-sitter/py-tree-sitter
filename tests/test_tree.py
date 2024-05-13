from unittest import TestCase

from tree_sitter import Language, Parser

import tree_sitter_python
import tree_sitter_rust


class TestTree(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.python = Language(tree_sitter_python.language())
        cls.rust = Language(tree_sitter_rust.language())

    def test_edit(self):
        parser = Parser(self.python)
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
            str(new_tree.root_node),
            "(module (function_definition"
            + " name: (identifier)"
            + " parameters: (parameters (identifier))"
            + " body: (block"
            + " (expression_statement (call"
            + " function: (identifier)"
            + " arguments: (argument_list))))))",
        )

    def test_changed_ranges(self):
        parser = Parser(self.python)
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
        changed_ranges = tree.changed_ranges(new_tree)

        self.assertEqual(len(changed_ranges), 1)
        self.assertEqual(changed_ranges[0].start_byte, edit_offset)
        self.assertEqual(changed_ranges[0].start_point, (0, edit_offset))
        self.assertEqual(changed_ranges[0].end_byte, edit_offset + 2)
        self.assertEqual(changed_ranges[0].end_point, (0, edit_offset + 2))

    def test_walk(self):
        parser = Parser(self.rust)

        tree = parser.parse(
            b"""
                struct Stuff {
                    a: A,
                    b: Option<B>,
                }
            """
        )

        cursor = tree.walk()

        # Node always returns the same instance
        self.assertIs(cursor.node, cursor.node)

        self.assertEqual(cursor.node.type, "source_file")

        self.assertEqual(cursor.goto_first_child(), True)
        self.assertEqual(cursor.node.type, "struct_item")

        self.assertEqual(cursor.goto_first_child(), True)
        self.assertEqual(cursor.node.type, "struct")
        self.assertEqual(cursor.node.is_named, False)

        self.assertEqual(cursor.goto_next_sibling(), True)
        self.assertEqual(cursor.node.type, "type_identifier")
        self.assertEqual(cursor.node.is_named, True)

        self.assertEqual(cursor.goto_next_sibling(), True)
        self.assertEqual(cursor.node.type, "field_declaration_list")
        self.assertEqual(cursor.node.is_named, True)

        self.assertEqual(cursor.goto_last_child(), True)
        self.assertEqual(cursor.node.type, "}")
        self.assertEqual(cursor.node.is_named, False)
        self.assertEqual(cursor.node.start_point, (4, 16))

        self.assertEqual(cursor.goto_previous_sibling(), True)
        self.assertEqual(cursor.node.type, ",")
        self.assertEqual(cursor.node.is_named, False)
        self.assertEqual(cursor.node.start_point, (3, 32))

        self.assertEqual(cursor.goto_previous_sibling(), True)
        self.assertEqual(cursor.node.type, "field_declaration")
        self.assertEqual(cursor.node.is_named, True)
        self.assertEqual(cursor.node.start_point, (3, 20))

        self.assertEqual(cursor.goto_previous_sibling(), True)
        self.assertEqual(cursor.node.type, ",")
        self.assertEqual(cursor.node.is_named, False)
        self.assertEqual(cursor.node.start_point, (2, 24))

        self.assertEqual(cursor.goto_previous_sibling(), True)
        self.assertEqual(cursor.node.type, "field_declaration")
        self.assertEqual(cursor.node.is_named, True)
        self.assertEqual(cursor.node.start_point, (2, 20))

        self.assertEqual(cursor.goto_previous_sibling(), True)
        self.assertEqual(cursor.node.type, "{")
        self.assertEqual(cursor.node.is_named, False)
        self.assertEqual(cursor.node.start_point, (1, 29))

        copy = tree.walk()
        copy.reset_to(cursor)

        self.assertEqual(copy.node.type, "{")
        self.assertEqual(copy.node.is_named, False)

        self.assertEqual(copy.goto_parent(), True)
        self.assertEqual(copy.node.type, "field_declaration_list")
        self.assertEqual(copy.node.is_named, True)

        self.assertEqual(copy.goto_parent(), True)
        self.assertEqual(copy.node.type, "struct_item")
