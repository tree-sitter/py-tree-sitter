import unittest
from tree_sitter import Parser, Language

LIB_PATH = "build/languages.so"
Language.build_library(LIB_PATH, [
    "tests/fixtures/tree-sitter-python",
    "tests/fixtures/tree-sitter-javascript",
])
PYTHON = Language(LIB_PATH, "python")
JAVASCRIPT = Language(LIB_PATH, "javascript")


class TestTreeSitter(unittest.TestCase):
    def test_basic_parsing(self):
        parser = Parser()

        # First parse some python code
        parser.set_language(PYTHON)
        tree = parser.parse("def foo():\n  bar()")

        root_node = tree.root_node
        self.assertEqual(
            root_node.sexp(),
            "(module (function_definition (identifier) (parameters) (expression_statement (call (identifier) (argument_list)))))"
        )

        assert root_node.type == 'module'
        assert root_node.start_point == (0, 0)
        self.assertEqual(root_node.type, "module")
        self.assertEqual(root_node.start_byte, 0)
        self.assertEqual(root_node.end_byte, 18)
        self.assertEqual(root_node.start_point, (0, 0))
        self.assertEqual(root_node.end_point, (1, 7))

        fn_node = root_node.children[0]
        self.assertEqual(fn_node.type, "function_definition")
        self.assertEqual(fn_node.start_byte, 0)
        self.assertEqual(fn_node.end_byte, 18)
        self.assertEqual(fn_node.start_point, (0, 0))
        self.assertEqual(fn_node.end_point, (1, 7))

        self.assertEqual(fn_node.children[0].type, "def")
        self.assertEqual(fn_node.children[1].type, "identifier")
        self.assertEqual(fn_node.children[2].type, "parameters")
        self.assertEqual(fn_node.children[3].type, ":")
        self.assertEqual(fn_node.children[4].type, "expression_statement")

        # Parse some javascript code
        parser.set_language(JAVASCRIPT)
        tree = parser.parse("function foo() {\n  bar();\n}")

        root_node = tree.root_node
        self.assertEqual(
            root_node.sexp(),
            "(program (function (identifier) (formal_parameters) (statement_block (expression_statement (call_expression (identifier) (arguments))))))"
        )
