from os import name as os_name
from platform import machine

from setuptools import Extension, setup  # type: ignore

if os_name != "nt":
    cflags = [
        "-std=c11",
        "-fvisibility=hidden",
        "-Wno-cast-function-type",
        "-Werror=implicit-function-declaration",
    ]
    # FIXME: GCC optimizer bug workaround for #330 & #386
    if machine().startswith("aarch64"):
        cflags.append("--param=early-inlining-insns=9")
else:
    cflags = ["/std:c11", "/wd4244"]

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
                "tree_sitter/binding/node.c",
                "tree_sitter/binding/parser.c",
                "tree_sitter/binding/query.c",
                "tree_sitter/binding/query_cursor.c",
                "tree_sitter/binding/query_predicates.c",
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
                ("_POSIX_C_SOURCE", "200112L"),
                ("_DEFAULT_SOURCE", None),
                ("PY_SSIZE_T_CLEAN", None),
                ("TREE_SITTER_HIDE_SYMBOLS", None),
            ],
            extra_compile_args=cflags,
        )
    ],
)
