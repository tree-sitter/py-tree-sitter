from unittest import TestCase

import tree_sitter_python
import tree_sitter_javascript

from tree_sitter import Language, Parser, Query


def collect_matches(matches):
    return [(m[0], format_captures(m[1])) for m in matches]


def format_captures(captures):
    return [(name, format_capture(capture)) for name, capture in captures.items()]


def format_capture(capture):
    return (
        [n.text.decode("utf-8") for n in capture]
        if isinstance(capture, list)
        else capture.text.decode("utf-8")
    )


class TestQuery(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.javascript = Language(tree_sitter_javascript.language())
        cls.python = Language(tree_sitter_python.language())

    def assert_query_matches(self, language, query, source, expected):
        parser = Parser(language)
        tree = parser.parse(source)
        matches = language.query(query).matches(tree.root_node)
        matches = collect_matches(matches)
        self.assertEqual(matches, expected)

    def test_errors(self):
        with self.assertRaises(NameError, msg="Invalid node type foo"):
            Query(self.python, "(list (foo))")
        with self.assertRaises(NameError, msg="Invalid field name buzz"):
            Query(self.python, "(function_definition buzz: (identifier))")
        with self.assertRaises(NameError, msg="Invalid capture name garbage"):
            Query(self.python, "((function_definition) (#eq? @garbage foo))")
        with self.assertRaises(SyntaxError, msg="Invalid syntax at offset 6"):
            Query(self.python, "(list))")

    def test_matches_with_simple_pattern(self):
        self.assert_query_matches(
            self.javascript,
            "(function_declaration name: (identifier) @fn-name)",
            b"function one() { two(); function three() {} }",
            [(0, [("fn-name", "one")]), (0, [("fn-name", "three")])],
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
                (0, [("the-class-name", "Person"), ("the-method-name", "constructor")]),
                (0, [("the-class-name", "Person"), ("the-method-name", "getFullName")]),
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
                (0, [("fn-def", "f1")]),
                (1, [("fn-ref", "f2")]),
                (1, [("fn-ref", "f3")]),
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
                (0, [("x1", "c"), ("x2", "d")]),
                (0, [("x1", "e"), ("x2", "f")]),
                (0, [("x1", "e"), ("x2", "g")]),
                (0, [("x1", "f"), ("x2", "g")]),
                (0, [("x1", "e"), ("x2", "h")]),
                (0, [("x1", "f"), ("x2", "h")]),
                (0, [("x1", "g"), ("x2", "h")]),
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
                        ("fn-name", "one"),
                        ("fn-statements", ["x = 1;", "y = 2;", "z = 3;"]),
                    ],
                ),
                (0, [("fn-name", "two"), ("fn-statements", ["x = 1;"])]),
            ],
        )

    def test_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = self.python.query(
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
        query1 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#eq? @function-name fun1))
            """
        )
        captures1 = query1.captures(root_node)
        self.assertEqual(1, len(captures1))
        self.assertEqual(b"fun1", captures1[0][0].text)
        self.assertEqual("function-name", captures1[0][1])

        # functions with name not equal to 'fun1' -> test for #not-eq? @capture string
        query2 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#not-eq? @function-name fun1))
        """
        )
        captures2 = query2.captures(root_node)
        self.assertEqual(1, len(captures2))
        self.assertEqual(b"fun2", captures2[0][0].text)
        self.assertEqual("function-name", captures2[0][1])

        # key pairs whose key is equal to its value -> test for #eq? @capture1 @capture2
        query3 = self.javascript.query(
            """
            ((pair
                key: (property_identifier) @key-name
                value: (identifier) @value-name)
                (#eq? @key-name @value-name))
            """
        )
        captures3 = query3.captures(root_node)
        self.assertEqual(2, len(captures3))
        self.assertSetEqual({b"equal"}, set([c[0].text for c in captures3]))
        self.assertSetEqual({"key-name", "value-name"}, set([c[1] for c in captures3]))

        # key pairs whose key is not equal to its value
        # -> test for #not-eq? @capture1 @capture2
        query4 = self.javascript.query(
            """
            ((pair
                key: (property_identifier) @key-name
                value: (identifier) @value-name)
                (#not-eq? @key-name @value-name))
            """
        )
        captures4 = query4.captures(root_node)
        self.assertEqual(2, len(captures4))
        self.assertSetEqual({b"key1", b"value1"}, set([c[0].text for c in captures4]))
        self.assertSetEqual({"key-name", "value-name"}, set([c[1] for c in captures4]))

        # equality that is satisfied by *another* capture
        query5 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters (identifier) @parameter-name))
                (#eq? @function-name arg))
            """
        )
        captures5 = query5.captures(root_node)
        self.assertEqual(0, len(captures5))

        # functions that match the regex .*1 -> test for #match @capture regex
        query6 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#match? @function-name ".*1"))
            """
        )
        captures6 = query6.captures(root_node)
        self.assertEqual(1, len(captures6))
        self.assertEqual(b"fun1", captures6[0][0].text)

        # functions that do not match the regex .*1 -> test for #not-match @capture regex
        query6 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name)
                (#not-match? @function-name ".*1"))
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

    def test_text_predicate_on_optional_capture(self):
        parser = Parser(self.javascript)
        source = b"fun1(1)"
        tree = parser.parse(source)
        root_node = tree.root_node

        # optional capture that is missing in source used in #eq? @capture string
        query1 = self.javascript.query(
            """
            ((call_expression
                function: (identifier) @function-name
                arguments: (arguments (string)? @optional-string-arg)
                (#eq? @optional-string-arg "1")))
            """
        )
        captures1 = query1.captures(root_node)
        self.assertEqual(1, len(captures1))
        self.assertEqual(b"fun1", captures1[0][0].text)
        self.assertEqual("function-name", captures1[0][1])

        # optional capture that is missing in source used in #eq? @capture @capture
        query2 = self.javascript.query(
            """
            ((call_expression
                function: (identifier) @function-name
                arguments: (arguments (string)? @optional-string-arg)
                (#eq? @optional-string-arg @function-name)))
            """
        )
        captures2 = query2.captures(root_node)
        self.assertEqual(1, len(captures2))
        self.assertEqual(b"fun1", captures2[0][0].text)
        self.assertEqual("function-name", captures2[0][1])

        # optional capture that is missing in source used in #match? @capture string
        query3 = self.javascript.query(
            """
            ((call_expression
                function: (identifier) @function-name
                arguments: (arguments (string)? @optional-string-arg)
                (#match? @optional-string-arg "\\d+")))
            """
        )
        captures3 = query3.captures(root_node)
        self.assertEqual(1, len(captures3))
        self.assertEqual(b"fun1", captures3[0][0].text)
        self.assertEqual("function-name", captures3[0][1])

    def test_text_predicates_errors(self):
        with self.assertRaises(RuntimeError):
            self.javascript.query(
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#eq? @function-name @function-name fun1))
                """
            )

        with self.assertRaises(RuntimeError):
            self.javascript.query(
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#eq? fun1 @function-name))
            """
            )

        with self.assertRaises(RuntimeError):
            self.javascript.query(
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? @function-name @function-name fun1))
                """
            )

        with self.assertRaises(RuntimeError):
            self.javascript.query(
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? fun1 @function-name))
                """
            )

        with self.assertRaises(RuntimeError):
            self.javascript.query(
                """
                ((function_declaration
                    name: (identifier) @function-name)
                    (#match? @function-name @function-name))
                """
            )

    def test_multiple_text_predicates(self):
        parser = Parser(self.javascript)
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
        query1 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name))
                (#eq? @function-name fun1))
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
        query2 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name))
                (#eq? @argument-name arg))
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
        query3 = self.javascript.query(
            """
            ((function_declaration
                name: (identifier) @function-name
                parameters: (formal_parameters
                    (identifier) @argument-name))
                (#eq? @function-name fun1)
                (#eq? @argument-name arg))
            """
        )
        captures3 = query3.captures(root_node)
        self.assertEqual(2, len(captures3))
        self.assertEqual(b"fun1", captures3[0][0].text)
        self.assertEqual("function-name", captures3[0][1])
        self.assertEqual(b"arg", captures3[1][0].text)
        self.assertEqual("argument-name", captures3[1][1])

    def test_point_range_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = self.python.query(
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )

        captures = query.captures(tree.root_node, start_point=(1, 0), end_point=(2, 0))

        self.assertEqual(captures[0][0].start_point, (1, 2))
        self.assertEqual(captures[0][0].end_point, (1, 5))
        self.assertEqual(captures[0][1], "func-call")

    def test_byte_range_captures(self):
        parser = Parser(self.python)
        source = b"def foo():\n  bar()\ndef baz():\n  quux()\n"
        tree = parser.parse(source)
        query = self.python.query(
            """
            (function_definition name: (identifier) @func-def)
            (call function: (identifier) @func-call)
            """
        )

        captures = query.captures(tree.root_node, start_byte=10, end_byte=20)
        self.assertEqual(captures[0][0].start_point, (1, 2))
        self.assertEqual(captures[0][0].end_point, (1, 5))
        self.assertEqual(captures[0][1], "func-call")
