from re import error as RegexError
from unittest import TestCase

import tree_sitter_python
import tree_sitter_javascript

from tree_sitter import Language, Parser, Query, QueryCursor, QueryError


def collect_matches(matches):
    return [(m[0], format_captures(m[1])) for m in matches]


def format_captures(captures):
    return [(name, format_capture(capture)) for name, capture in captures.items()]


def format_capture(capture):
    return [n.text.decode("utf-8") for n in capture]


class TestQuery(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.javascript = Language(tree_sitter_javascript.language())
        cls.python = Language(tree_sitter_python.language())

    def assert_query_matches(self, language, query, source, expected):
        parser = Parser(language)
        tree = parser.parse(source)
        cursor = QueryCursor(Query(language, query))
        matches = cursor.matches(tree.root_node)
        self.assertListEqual(collect_matches(matches), expected)

    def test_errors(self):
        with self.assertRaises(QueryError):
            Query(self.python, "(list (foo))")
        with self.assertRaises(QueryError):
            Query(self.python, "(function_definition buzz: (identifier))")
        with self.assertRaises(QueryError):
            Query(self.python, "((function_definition) (#eq? @garbage foo))")
        with self.assertRaises(QueryError):
            Query(self.python, "(list))")

    def test_matches_with_simple_pattern(self):
        self.assert_query_matches(
            self.javascript,
            "(function_declaration name: (identifier) @fn-name)",
            b"function one() { two(); function three() {} }",
            [(0, [("fn-name", ["one"])]), (0, [("fn-name", ["three"])])],
        )

    def test_matches_with_multiple_on_same_root(self):
        self.assert_query_matches(
            self.javascript,
            """
            (class_declaration
                name: (identifier) @the-class-name
                (class_body
                    (method_definition
                        name: (property_identifier) @the-method-name)))
            """,
            b"""
            class Person {
                // the constructor
                constructor(name) { this.name = name; }

                // the getter
                getFullName() { return this.name; }
            }
            """,
            [
                (0, [("the-class-name", ["Person"]), ("the-method-name", ["constructor"])]),
                (0, [("the-class-name", ["Person"]), ("the-method-name", ["getFullName"])]),
            ],
        )

    def test_matches_with_multiple_patterns_different_roots(self):
        self.assert_query_matches(
            self.javascript,
            """
            (function_declaration name: (identifier) @fn-def)
            (call_expression function: (identifier) @fn-ref)
            """,
            b"""
            function f1() {
                f2(f3());
            }
            """,
            [
                (0, [("fn-def", ["f1"])]),
                (1, [("fn-ref", ["f2"])]),
                (1, [("fn-ref", ["f3"])]),
            ],
        )

    def test_matches_with_nesting_and_no_fields(self):
        self.assert_query_matches(
            self.javascript,
            "(array (array (identifier) @x1 (identifier) @x2))",
            b"""
            [[a]];
            [[c, d], [e, f, g, h]];
            [[h], [i]];
            """,
            [
                (0, [("x1", ["c"]), ("x2", ["d"])]),
                (0, [("x1", ["e"]), ("x2", ["f"])]),
                (0, [("x1", ["e"]), ("x2", ["g"])]),
                (0, [("x1", ["f"]), ("x2", ["g"])]),
                (0, [("x1", ["e"]), ("x2", ["h"])]),
                (0, [("x1", ["f"]), ("x2", ["h"])]),
                (0, [("x1", ["g"]), ("x2", ["h"])]),
            ],
        )

    def test_matches_with_list_capture(self):
        self.assert_query_matches(
            self.javascript,
            """
            (function_declaration
                name: (identifier) @fn-name
                body: (statement_block (_)* @fn-statements))
            """,
            b"""function one() {
                    x = 1;
                    y = 2;
                    z = 3;
                }
                function two() {
                    x = 1;
                }
            """,
            [
                (
                    0,
                    [
                        ("fn-name", ["one"]),
                        ("fn-statements", ["x = 1;", "y = 2;", "z = 3;"]),
                    ],
                ),
                (0, [("fn-name", ["two"]), ("fn-statements", ["x = 1;"])]),
            ],
        )

    def test_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = Query(
            self.python,
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """,
        )

        cursor = QueryCursor(query)
        captures = list(cursor.captures(tree.root_node).items())

        self.assertEqual(captures[0][0], "func-def")
        self.assertEqual(captures[0][1][0].start_point, (0, 4))
        self.assertEqual(captures[0][1][0].end_point, (0, 7))
        self.assertEqual(captures[0][1][1].start_point, (2, 4))
        self.assertEqual(captures[0][1][1].end_point, (2, 7))

        self.assertEqual(captures[1][0], "func-call")
        self.assertEqual(captures[1][1][0].start_point, (1, 2))
        self.assertEqual(captures[1][1][0].end_point, (1, 5))
        self.assertEqual(captures[1][1][1].start_point, (3, 2))
        self.assertEqual(captures[1][1][1].end_point, (3, 6))

    def test_text_predicates(self):
        parser = Parser(self.javascript)
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
        query1 = Query(
            self.javascript,
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#eq? @function-name fun1))
            """
        )
        cursor = QueryCursor(query1)
        captures1 = list(cursor.captures(root_node).items())
        self.assertEqual(1, len(captures1))
        self.assertEqual(captures1[0][0], "function-name")
        self.assertEqual(captures1[0][1][0].text, b"fun1")

        # functions with name not equal to 'fun1' -> test for #not-eq? @capture string
        query2 = Query(
            self.javascript,
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#not-eq? @function-name fun1))
            """
        )
        cursor = QueryCursor(query2)
        captures2 = list(cursor.captures(root_node).items())
        self.assertEqual(1, len(captures2))
        self.assertEqual(captures2[0][0], "function-name")
        self.assertEqual(captures2[0][1][0].text, b"fun2")

    def test_text_predicates_errors(self):
        with self.assertRaises(QueryError):
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#eq? @function-name @function-name fun1))
                """
            )

        with self.assertRaises(QueryError):
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#eq? fun1 @function-name))
                """
            )

        with self.assertRaises(QueryError):
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? @function-name @function-name fun1))
                """
            )

        with self.assertRaises(QueryError):
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? fun1 @function-name))
                """
            )

        with self.assertRaises(QueryError):
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? @function-name @function-name))
                """
            )

        with self.assertRaises(QueryError) as ctx:
            Query(
                self.javascript,
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? @function-name "?"))
                """
            )
        self.assertEqual(
            str(ctx.exception), "Invalid predicate in pattern at row 1: regular expression error"
        )
        self.assertIsInstance(ctx.exception.__cause__, RegexError)

    def test_point_range_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = Query(
            self.python,
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )
        cursor = QueryCursor(query)
        cursor.set_point_range((1, 0), (2, 0))

        captures = list(cursor.captures(tree.root_node).items())

        self.assertEqual(captures[0][0], "func-call")
        self.assertEqual(captures[0][1][0].start_point, (1, 2))
        self.assertEqual(captures[0][1][0].end_point, (1, 5))

    def test_byte_range_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = Query(
            self.python,
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )
        cursor = QueryCursor(query)
        cursor.set_byte_range(10, 20)

        captures = list(cursor.captures(tree.root_node).items())
        self.assertEqual(captures[0][0], "func-call")
        self.assertEqual(captures[0][1][0].start_point, (1, 2))
        self.assertEqual(captures[0][1][0].end_point, (1, 5))
