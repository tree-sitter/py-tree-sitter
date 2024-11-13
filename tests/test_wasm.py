import importlib.resources
from unittest import TestCase

from tree_sitter import Language, Parser, Tree

try:
    import wasmtime

    class TestWasm(TestCase):
        @classmethod
        def setUpClass(cls):
            javascript_wasm = (
                importlib.resources.files("tests")
                .joinpath("wasm/tree-sitter-javascript.wasm")
                .read_bytes()
            )
            engine = wasmtime.Engine()
            cls.javascript = Language.from_wasm("javascript", engine, javascript_wasm)

        def test_parser(self):
            parser = Parser(self.javascript)
            self.assertIsInstance(parser.parse(b"test"), Tree)

        def test_language_is_wasm(self):
            self.assertEqual(self.javascript.is_wasm, True)

except ImportError:

    class TestWasmDisabled(TestCase):
        def test_parser(self):
            def runtest():
                Language.from_wasm("javascript", None, b"")

            self.assertRaisesRegex(
                RuntimeError, "wasmtime module is not loaded", runtest
            )
