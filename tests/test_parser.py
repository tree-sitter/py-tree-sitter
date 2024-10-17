from unittest import TestCase

from tree_sitter import Language, LogType, Parser, Range, Tree

import tree_sitter_html
import tree_sitter_javascript
import tree_sitter_json
import tree_sitter_python
import tree_sitter_rust


def simple_range(start, end):
    return Range((0, start), (0, end), start, end)


class TestParser(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = Language(tree_sitter_html.language())
        cls.python = Language(tree_sitter_python.language())
        cls.javascript = Language(tree_sitter_javascript.language())
        cls.json = Language(tree_sitter_json.language())
        cls.rust = Language(tree_sitter_rust.language())
        cls.max_range = Range((0, 0), (0xFFFFFFFF, 0xFFFFFFFF), 0, 0xFFFFFFFF)
        cls.min_range = Range((0, 0), (0, 1), 0, 1)
        cls.timeout = 1000

    def test_init_no_args(self):
        parser = Parser()
        self.assertIsNone(parser.language)
        self.assertListEqual(parser.included_ranges, [self.max_range])
        self.assertEqual(parser.timeout_micros, 0)

    def test_init_args(self):
        parser = Parser(
            language=self.python, included_ranges=[self.min_range], timeout_micros=self.timeout
        )
        self.assertEqual(parser.language, self.python)
        self.assertListEqual(parser.included_ranges, [self.min_range])
        self.assertEqual(parser.timeout_micros, self.timeout)

    def test_setters(self):
        parser = Parser()

        with self.subTest(setter="language"):
            parser.language = self.python
            self.assertEqual(parser.language, self.python)

        with self.subTest(setter="included_ranges"):
            parser.included_ranges = [self.min_range]
            self.assertListEqual(parser.included_ranges, [self.min_range])
            with self.assertRaises(ValueError):
                parser.included_ranges = [
                    Range(
                        start_byte=23,
                        end_byte=29,
                        start_point=(0, 23),
                        end_point=(0, 29),
                    ),
                    Range(
                        start_byte=0,
                        end_byte=5,
                        start_point=(0, 0),
                        end_point=(0, 5),
                    ),
                    Range(
                        start_byte=50,
                        end_byte=60,
                        start_point=(0, 50),
                        end_point=(0, 60),
                    ),
                ]
            with self.assertRaises(ValueError):
                parser.included_ranges = [
                    Range(
                        start_byte=10,
                        end_byte=5,
                        start_point=(0, 10),
                        end_point=(0, 5),
                    )
                ]

        with self.subTest(setter="timeout_micros"):
            parser.timeout_micros = self.timeout
            self.assertEqual(parser.timeout_micros, self.timeout)

        with self.subTest(setter="logger"):
            def logger(log_type, message):
                print(log_type.name, message)

            parser.logger = logger
            self.assertEqual(parser.logger, logger)

    def test_deleters(self):
        parser = Parser()

        with self.subTest(deleter="language"):
            del parser.language
            self.assertIsNone(parser.language)

        with self.subTest(deleter="included_ranges"):
            del parser.included_ranges
            self.assertListEqual(parser.included_ranges, [self.max_range])

        with self.subTest(deleter="timeout_micros"):
            del parser.timeout_micros
            self.assertEqual(parser.timeout_micros, 0)

        with self.subTest(deleter="logger"):
            del parser.logger
            self.assertEqual(parser.logger, None)

    def test_parse_buffer(self):
        parser = Parser(self.javascript)
        with self.subTest(type="bytes"):
            self.assertIsInstance(parser.parse(b"test"), Tree)
        with self.subTest(type="memoryview"):
            self.assertIsInstance(parser.parse(memoryview(b"test")), Tree)
        with self.subTest(type="bytearray"):
            self.assertIsInstance(parser.parse(bytearray(b"test")), Tree)

    def test_parse_callback(self):
        parser = Parser(self.python)
        source_lines = ["def foo():\n", "  bar()"]

        def read_callback(_, point):
            row, column = point
            if row >= len(source_lines):
                return None
            if column >= len(source_lines[row]):
                return None
            return source_lines[row][column:].encode("utf8")

        tree = parser.parse(read_callback)
        self.assertEqual(
            str(tree.root_node),
            "(module (function_definition"
            + " name: (identifier)"
            + " parameters: (parameters)"
            + " body: (block (expression_statement (call"
            + " function: (identifier)"
            + " arguments: (argument_list))))))",
        )

    def test_parse_utf16_encoding(self):
        source_code = bytes("'😎' && '🐍'", "utf16")
        parser = Parser(self.javascript)

        def read(byte_position, _):
            return source_code[byte_position : byte_position + 2]

        tree = parser.parse(read, encoding="utf16")
        root_node = tree.root_node
        snake_node = root_node.children[0].children[0].children[2]
        snake = source_code[snake_node.start_byte + 2 : snake_node.end_byte - 2]

        self.assertEqual(snake_node.type, "string")
        self.assertEqual(snake.decode("utf16"), "🐍")
        self.assertIs(tree.language, self.javascript)

    def test_parse_invalid_encoding(self):
        parser = Parser(self.python)
        with self.assertRaises(ValueError):
            parser.parse(b"foo", encoding="ascii")

    def test_parse_with_one_included_range(self):
        source_code = b"<span>hi</span><script>console.log('sup');</script>"
        parser = Parser(self.html)
        html_tree = parser.parse(source_code)
        script_content_node = html_tree.root_node.child(1).child(1)
        self.assertIsNotNone(script_content_node)
        self.assertEqual(script_content_node.type, "raw_text")

        parser.included_ranges = [script_content_node.range]
        parser.language = self.javascript
        js_tree = parser.parse(source_code)
        self.assertEqual(
            str(js_tree.root_node),
            "(program (expression_statement (call_expression"
            + " function: (member_expression object: (identifier) property: (property_identifier))"
            + " arguments: (arguments (string (string_fragment))))))",
        )
        self.assertEqual(js_tree.root_node.start_point, (0, source_code.index(b"console")))
        self.assertEqual(js_tree.included_ranges, [script_content_node.range])

    def test_parse_with_multiple_included_ranges(self):
        source_code = b"html `<div>Hello, ${name.toUpperCase()}, it's <b>${now()}</b>.</div>`"

        parser = Parser(self.javascript)
        js_tree = parser.parse(source_code)
        template_string_node = js_tree.root_node.descendant_for_byte_range(
            source_code.index(b"`<"), source_code.index(b">`")
        )
        self.assertIsNotNone(template_string_node)

        self.assertEqual(template_string_node.type, "template_string")

        open_quote_node = template_string_node.child(0)
        self.assertIsNotNone(open_quote_node)
        interpolation_node1 = template_string_node.child(2)
        self.assertIsNotNone(interpolation_node1)
        interpolation_node2 = template_string_node.child(4)
        self.assertIsNotNone(interpolation_node2)
        close_quote_node = template_string_node.child(6)
        self.assertIsNotNone(close_quote_node)

        html_ranges = [
            Range(
                start_byte=open_quote_node.end_byte,
                start_point=open_quote_node.end_point,
                end_byte=interpolation_node1.start_byte,
                end_point=interpolation_node1.start_point,
            ),
            Range(
                start_byte=interpolation_node1.end_byte,
                start_point=interpolation_node1.end_point,
                end_byte=interpolation_node2.start_byte,
                end_point=interpolation_node2.start_point,
            ),
            Range(
                start_byte=interpolation_node2.end_byte,
                start_point=interpolation_node2.end_point,
                end_byte=close_quote_node.start_byte,
                end_point=close_quote_node.start_point,
            ),
        ]
        parser.included_ranges = html_ranges
        parser.language = self.html
        html_tree = parser.parse(source_code)

        self.assertEqual(
            str(html_tree.root_node),
            "(document (element"
            + " (start_tag (tag_name))"
            + " (text)"
            + " (element (start_tag (tag_name)) (end_tag (tag_name)))"
            + " (text)"
            + " (end_tag (tag_name))))",
        )
        self.assertEqual(html_tree.included_ranges, html_ranges)

        div_element_node = html_tree.root_node.child(0)
        self.assertIsNotNone(div_element_node)
        hello_text_node = div_element_node.child(1)
        self.assertIsNotNone(hello_text_node)
        b_element_node = div_element_node.child(2)
        self.assertIsNotNone(b_element_node)
        b_start_tag_node = b_element_node.child(0)
        self.assertIsNotNone(b_start_tag_node)
        b_end_tag_node = b_element_node.child(1)
        self.assertIsNotNone(b_end_tag_node)

        self.assertEqual(hello_text_node.type, "text")
        self.assertEqual(hello_text_node.start_byte, source_code.index(b"Hello"))
        self.assertEqual(hello_text_node.end_byte, source_code.index(b" <b>"))

        self.assertEqual(b_start_tag_node.type, "start_tag")
        self.assertEqual(b_start_tag_node.start_byte, source_code.index(b"<b>"))
        self.assertEqual(b_start_tag_node.end_byte, source_code.index(b"${now()}"))

        self.assertEqual(b_end_tag_node.type, "end_tag")
        self.assertEqual(b_end_tag_node.start_byte, source_code.index(b"</b>"))
        self.assertEqual(b_end_tag_node.end_byte, source_code.index(b".</div>"))

    def test_parse_with_included_range_containing_mismatched_positions(self):
        source_code = b"<div>test</div>{_ignore_this_part_}"
        end_byte = source_code.index(b"{_ignore_this_part_")

        range_to_parse = Range(
            start_byte=0,
            start_point=(10, 12),
            end_byte=end_byte,
            end_point=(10, 12 + end_byte),
        )

        parser = Parser(self.html, included_ranges=[range_to_parse])
        html_tree = parser.parse(source_code)

        self.assertEqual(
            str(html_tree.root_node),
            "(document (element (start_tag (tag_name)) (text) (end_tag (tag_name))))",
        )

    def test_parse_with_included_range_boundaries(self):
        source_code = b"a <%= b() %> c <% d() %>"
        range1_start_byte = source_code.index(b" b() ")
        range1_end_byte = range1_start_byte + len(b" b() ")
        range2_start_byte = source_code.index(b" d() ")
        range2_end_byte = range2_start_byte + len(b" d() ")

        parser = Parser(
            self.javascript,
            included_ranges=[
                Range(
                    start_byte=range1_start_byte,
                    end_byte=range1_end_byte,
                    start_point=(0, range1_start_byte),
                    end_point=(0, range1_end_byte),
                ),
                Range(
                    start_byte=range2_start_byte,
                    end_byte=range2_end_byte,
                    start_point=(0, range2_start_byte),
                    end_point=(0, range2_end_byte),
                ),
            ],
        )

        tree = parser.parse(source_code)
        root = tree.root_node
        statement1 = root.child(0)
        self.assertIsNotNone(statement1)
        statement2 = root.child(1)
        self.assertIsNotNone(statement2)

        self.assertEqual(
            str(root),
            "(program"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments)))"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments))))",
        )

        self.assertEqual(statement1.start_byte, source_code.index(b"b()"))
        self.assertEqual(statement1.end_byte, source_code.find(b" %> c"))
        self.assertEqual(statement2.start_byte, source_code.find(b"d()"))
        self.assertEqual(statement2.end_byte, len(source_code) - len(" %>"))

    def test_parse_with_a_newly_excluded_range(self):
        source_code = b"<div><span><%= something %></span></div>"

        # Parse HTML including the template directive, which will cause an error
        parser = Parser(self.html)
        first_tree = parser.parse(source_code)

        prefix = b"a very very long line of plain text. "
        first_tree.edit(
            start_byte=0,
            old_end_byte=0,
            new_end_byte=len(prefix),
            start_point=(0, 0),
            old_end_point=(0, 0),
            new_end_point=(0, len(prefix)),
        )
        source_code = prefix + source_code

        # Parse the HTML again, this time *excluding* the template directive
        # (which has moved since the previous parse).
        directive_start = source_code.index(b"<%=")
        directive_end = source_code.index(b"</span>")
        source_code_end = len(source_code)
        parser.included_ranges = [
            Range(
                start_byte=0,
                end_byte=directive_start,
                start_point=(0, 0),
                end_point=(0, directive_start),
            ),
            Range(
                start_byte=directive_end,
                end_byte=source_code_end,
                start_point=(0, directive_end),
                end_point=(0, source_code_end),
            ),
        ]

        tree = parser.parse(source_code, first_tree)

        self.assertEqual(
            str(tree.root_node),
            "(document (text) (element"
            + " (start_tag (tag_name))"
            + " (element (start_tag (tag_name)) (end_tag (tag_name)))"
            + " (end_tag (tag_name))))",
        )

        self.assertEqual(
            tree.changed_ranges(first_tree),
            [
                # The first range that has changed syntax is the range of the newly-inserted text.
                Range(
                    start_byte=0,
                    end_byte=len(prefix),
                    start_point=(0, 0),
                    end_point=(0, len(prefix)),
                ),
                # Even though no edits were applied to the outer `div` element,
                # its contents have changed syntax because a range of text that
                # was previously included is now excluded.
                Range(
                    start_byte=directive_start,
                    end_byte=directive_end,
                    start_point=(0, directive_start),
                    end_point=(0, directive_end),
                ),
            ],
        )

    def test_parsing_with_a_newly_included_range(self):
        source_code = b"<div><%= foo() %></div><span><%= bar() %></span><%= baz() %>"
        range1_start = source_code.index(b" foo")
        range2_start = source_code.index(b" bar")
        range3_start = source_code.index(b" baz")
        range1_end = range1_start + 7
        range2_end = range2_start + 7
        range3_end = range3_start + 7

        # Parse only the first code directive as JavaScript
        parser = Parser(self.javascript)
        parser.included_ranges = [simple_range(range1_start, range1_end)]
        tree = parser.parse(source_code)
        self.assertEqual(
            str(tree.root_node),
            "(program"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments))))",
        )

        # Parse both the first and third code directives as JavaScript, using the old tree as a
        # reference.
        parser.included_ranges = [
            simple_range(range1_start, range1_end),
            simple_range(range3_start, range3_end),
        ]
        tree2 = parser.parse(source_code)
        self.assertEqual(
            str(tree2.root_node),
            "(program"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments)))"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments))))",
        )
        self.assertEqual(tree2.changed_ranges(tree), [simple_range(range1_end, range3_end)])

        # Parse all three code directives as JavaScript, using the old tree as a
        # reference.
        parser.included_ranges = [
            simple_range(range1_start, range1_end),
            simple_range(range2_start, range2_end),
            simple_range(range3_start, range3_end),
        ]
        tree3 = parser.parse(source_code)
        self.assertEqual(
            str(tree3.root_node),
            "(program"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments)))"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments)))"
            + " (expression_statement (call_expression"
            + " function: (identifier) arguments: (arguments))))",
        )
        self.assertEqual(
            tree3.changed_ranges(tree2),
            [simple_range(range2_start + 1, range2_end - 1)],
        )

    def test_logging(self):
        from logging import getLogger

        def logger(log_type: LogType, message: str):
            match log_type:
                case LogType.PARSE:
                    parse_logger.info(message)
                case LogType.LEX:
                    lex_logger.info(message)

        parse_logger = getLogger("tree_sitter.PARSE")
        lex_logger = getLogger("tree_sitter.LEX")
        parser = Parser(self.python, logger=logger)
        with self.assertLogs("tree_sitter") as logs:
            parser.parse(b"foo")

        self.assertEqual(logs.records[0].name, "tree_sitter.PARSE")
        self.assertEqual(logs.records[0].message, "new_parse")
        self.assertEqual(logs.records[3].name, "tree_sitter.LEX")
        self.assertEqual(logs.records[3].message, "consume character:'f'")

    def test_dot_graphs(self):
        from tempfile import TemporaryFile

        new_parse = ["graph {\n", 'label="new_parse"\n', "}\n"]
        parser = Parser(self.python)
        with TemporaryFile("w+") as f:
            parser.print_dot_graphs(f)
            parser.parse(b"foo")
            f.seek(0)
            lines = [f.readline(), f.readline(), f.readline()]
            self.assertListEqual(lines, new_parse)
