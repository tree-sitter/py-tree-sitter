from platform import system

from setuptools import Extension, setup  # type: ignore

setup(
    packages=["tree_sitter"],
    include_package_data=False,
    package_data={
        "tree_sitter": ["py.typed", "*.pyi"],
    },
    ext_modules=[
        Extension(
            name="tree_sitter._binding",
            sources=[
                "tree_sitter/core/lib/src/lib.c",
                "tree_sitter/binding/language.c",
                "tree_sitter/binding/lookahead_iterator.c",
                "tree_sitter/binding/lookahead_names_iterator.c",
                "tree_sitter/binding/node.c",
                "tree_sitter/binding/parser.c",
                "tree_sitter/binding/query.c",
                "tree_sitter/binding/range.c",
                "tree_sitter/binding/tree.c",
                "tree_sitter/binding/tree_cursor.c",
                "tree_sitter/binding/module.c",
            ],
            include_dirs=[
                "tree_sitter/binding",
                "tree_sitter/core/lib/include",
                "tree_sitter/core/lib/src",
            ],
            define_macros=[
                ("PY_SSIZE_T_CLEAN", None),
                ("TREE_SITTER_HIDE_SYMBOLS", None),
            ],
            undef_macros=[
                "TREE_SITTER_FEATURE_WASM",
            ],
            extra_compile_args=[
                "-std=c11",
                "-fvisibility=hidden",
                "-Wno-cast-function-type",
                "-Werror=implicit-function-declaration",
            ] if system() != "Windows" else [
                "/std:c11",
                "/wd4244",
            ],
        )
    ],
)
