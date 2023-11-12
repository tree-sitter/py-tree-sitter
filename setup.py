"""
Py-Tree-sitter
"""

from os import path
from platform import system

from setuptools import Extension, setup

with open(path.join(path.dirname(__file__), "README.md")) as f:
    LONG_DESCRIPTION = f.read()

setup(
    name="tree_sitter",
    version="0.20.2",
    maintainer="Max Brunsfeld",
    maintainer_email="maxbrunsfeld@gmail.com",
    author="Max Brunsfeld",
    author_email="maxbrunsfeld@gmail.com",
    url="https://github.com/tree-sitter/py-tree-sitter",
    license="MIT",
    platforms=["any"],
    python_requires=">=3.3",
    description="Python bindings for the Tree-Sitter parsing library",
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Topic :: Software Development :: Compilers",
        "Topic :: Text Processing :: Linguistic",
    ],
    install_requires=["setuptools>=60.0.0; python_version>='3.12'"],
    packages=["tree_sitter"],
    package_data={"tree_sitter": ["py.typed", "*.pyi"]},
    ext_modules=[
        Extension(
            "tree_sitter.binding",
            ["tree_sitter/core/lib/src/lib.c", "tree_sitter/binding.c"],
            include_dirs=["tree_sitter/core/lib/include", "tree_sitter/core/lib/src"],
            extra_compile_args=(
                ["-std=c99", "-Wno-unused-variable"] if system() != "Windows" else None
            ),
        )
    ],
    project_urls={"Source": "https://github.com/tree-sitter/py-tree-sitter"},
)
