import unittest
from tree_sitter import Parser, Language


lib_path = "build/python.so"
Language.build("tests/fixtures/tree-sitter-python", lib_path)
language = Language(lib_path, "python")


class TestTreeSitter(unittest.TestCase):
    def test_upper(self):
        parser = Parser()
        parser.set_language(language)
        tree = parser.parse("def foo():\n  bar()")
        self.assertEqual(tree.root_node.type, "module")
