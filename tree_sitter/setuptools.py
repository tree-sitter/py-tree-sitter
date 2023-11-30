"""A entry point for setuptools."""
import os
import sys

from setuptools import Distribution

from . import Language

try:
    import tomllib as tomli
except ImportError:
    import tomli


def build(distribution: Distribution) -> None:
    """Build binary library.

    According to tree sitter standard, we assume this project is named tree-sitter-XXX.
    """
    if distribution.metadata.name is None and os.path.isfile("setup.cfg"):
        import configparser

        parser = configparser.ConfigParser()
        parser.read(["setup.cfg"], encoding="utf-8")
        distribution.metadata.name = parser.get("metadata", "name", fallback=None)
    if distribution.metadata.name is None and os.path.isfile("pyproject.toml"):
        with open("pyproject.toml", "rb") as f:
            pyproject = tomli.load(f)
        distribution.metadata.name = pyproject.get("project", {}).get("name")
    if distribution.metadata.name is None:
        return
    language_name = distribution.metadata.name.partition("tree-sitter-")[2]
    ext = {"win32": "dll", "darwin": "dylib"}.get(sys.platform, "so")
    Language.build_library(
        "build/lib/"
        + distribution.metadata.name.replace("-", "_")
        + "/data/lib/"
        + language_name
        + "."
        + ext,
        ["."],
    )
