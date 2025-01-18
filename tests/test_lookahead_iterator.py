from unittest import TestCase

from tree_sitter import Language, Parser

import tree_sitter_rust


class TestLookaheadIterator(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rust = Language(tree_sitter_rust.language())

    def test_lookahead_iterator(self):
        parser = Parser(self.rust)
        cursor = parser.parse(b"struct Stuff{}").walk()

        self.assertEqual(cursor.goto_first_child(), True)  # struct
        self.assertEqual(cursor.goto_first_child(), True)  # struct keyword

        next_state = cursor.node.next_parse_state

        self.assertNotEqual(next_state, 0)
        self.assertEqual(
            next_state, self.rust.next_state(cursor.node.parse_state, cursor.node.grammar_id)
        )
        self.assertLess(next_state, self.rust.parse_state_count)
        self.assertEqual(cursor.goto_next_sibling(), True)  # type_identifier
        self.assertEqual(next_state, cursor.node.parse_state)
        self.assertEqual(cursor.node.grammar_name, "identifier")
        self.assertNotEqual(cursor.node.grammar_id, cursor.node.kind_id)

        expected_symbols = ["//", "/*", "identifier", "line_comment", "block_comment"]
        lookahead = self.rust.lookahead_iterator(next_state)
        self.assertEqual(lookahead.language, self.rust)
        self.assertListEqual(lookahead.names(), expected_symbols)

        lookahead.reset(next_state)
        self.assertListEqual(
            list(map(self.rust.node_kind_for_id, lookahead.symbols())), expected_symbols
        )

        lookahead.reset(next_state, self.rust)
        self.assertTupleEqual(
            (self.rust.id_for_node_kind("//", False), "//"),
            next(lookahead)
        )
