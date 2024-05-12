from unittest import TestCase

from tree_sitter import Language, Query

import tree_sitter_html
import tree_sitter_javascript
import tree_sitter_json
import tree_sitter_python
import tree_sitter_rust


class TestLanguage(TestCase):
    def setUp(self):
        self.html = tree_sitter_html.language()
        self.javascript = tree_sitter_javascript.language()
        self.json = tree_sitter_json.language()
        self.python = tree_sitter_python.language()
        self.rust = tree_sitter_rust.language()

    def test_init_invalid(self):
        self.assertRaises(ValueError, Language, -1)
        self.assertRaises(ValueError, Language, 42)

    def test_properties(self):
        lang = Language(self.python)
        self.assertEqual(lang.version, 14)
        self.assertEqual(lang.node_kind_count, 274)
        self.assertEqual(lang.parse_state_count, 2831)
        self.assertEqual(lang.field_count, 32)

    def test_node_kind_for_id(self):
        lang = Language(self.json)
        self.assertEqual(lang.node_kind_for_id(1), "{")
        self.assertEqual(lang.node_kind_for_id(3), "}")

    def test_id_for_node_kind(self):
        lang = Language(self.json)
        self.assertEqual(lang.id_for_node_kind(":", False), 4)
        self.assertEqual(lang.id_for_node_kind("string", True), 20)

    def test_node_kind_is_named(self):
        lang = Language(self.json)
        self.assertFalse(lang.node_kind_is_named(4))
        self.assertTrue(lang.node_kind_is_named(20))

    def test_node_kind_is_visible(self):
        lang = Language(self.json)
        self.assertTrue(lang.node_kind_is_visible(2))

    def test_field_name_for_id(self):
        lang = Language(self.json)
        self.assertEqual(lang.field_name_for_id(1), "key")
        self.assertEqual(lang.field_name_for_id(2), "value")

    def test_field_id_for_name(self):
        lang = Language(self.json)
        self.assertEqual(lang.field_id_for_name("key"), 1)
        self.assertEqual(lang.field_id_for_name("value"), 2)

    def test_next_state(self):
        lang = Language(self.javascript)
        self.assertNotEqual(lang.next_state(1, 1), 0)

    def test_lookahead_iterator(self):
        lang = Language(self.javascript)
        self.assertIsNotNone(lang.lookahead_iterator(0))
        self.assertIsNone(lang.lookahead_iterator(9999))

    def test_query(self):
        lang = Language(self.json)
        query = lang.query("(string) @string")
        self.assertIsInstance(query, Query)

    def test_eq(self):
        self.assertEqual(Language(self.json), Language(self.json))
        self.assertNotEqual(Language(self.rust), Language(self.html))

    def test_int(self):
        for name in ["html", "javascript", "json", "python", "rust"]:
            with self.subTest(language=name):
                ptr = getattr(self, name)
                lang = Language(ptr)
                self.assertEqual(int(lang), ptr)
                self.assertEqual(hash(lang), ptr)
