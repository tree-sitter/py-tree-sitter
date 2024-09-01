from importlib.metadata import version as v
from pathlib import PurePath
from re import compile as regex
from sys import path

path.insert(0, str(PurePath(__file__).parents[2] / "tree_sitter"))

project = "py-tree-sitter"
author = "Max Brunsfeld"
copyright = "2019, MIT license"
release = v("tree_sitter")

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinx.ext.githubpages",
]
source_suffix = ".rst"
master_doc = "index"
language = "en"
needs_sphinx = "7.3"
templates_path = ["_templates"]

intersphinx_mapping = {
    "python": ("https://docs.python.org/3.9/", None),
}

autoclass_content = "class"
autodoc_member_order = "alphabetical"
autosummary_generate = False

napoleon_numpy_docstring = True
napoleon_google_docstring = False
napoleon_use_ivar = False
napoleon_use_param = True
napoleon_use_rtype = False
napoleon_use_admonition_for_notes = True

html_theme = "sphinx_book_theme"
html_theme_options = {
    "repository_url": "https://github.com/tree-sitter/py-tree-sitter",
    "pygment_light_style": "default",
    "pygment_dark_style": "github-dark",
    "navigation_with_keys": False,
    "use_repository_button": True,
    "use_download_button": False,
    "use_fullscreen_button": False,
    "show_toc_level": 2,
}
html_static_path = ["_static"]
html_logo = "_static/logo.png"
html_favicon = "_static/favicon.png"


special_doc = regex("\S*self[^.]+")


def process_signature(_app, _what, name, _obj, _options, _signature, return_annotation):
    if name == "tree_sitter.Language":
        return "(ptr)", return_annotation
    if name == "tree_sitter.Query":
        return "(language, source, *, timeout_micros=None)", return_annotation
    if name == "tree_sitter.Parser":
        return "(language, *, included_ranges=None, timeout_micros=None)", return_annotation
    if name == "tree_sitter.Range":
        return "(start_point, end_point, start_byte, end_byte)", return_annotation
    if name == "tree_sitter.QueryPredicate":
        return None, return_annotation


def process_docstring(_app, what, name, _obj, _options, lines):
    if what == "data":
        lines.clear()
    elif what == "method":
        if name.endswith("__index__"):
            lines[0] = "Converts ``self`` to an integer for use as an index."
        elif name.endswith("__") and lines and "self" in lines[0]:
            lines[0] = f"Implements ``{special_doc.search(lines[0]).group(0)}``."


def process_bases(_app, name, _obj, _options, bases):
    if name == "tree_sitter.Point":
        bases[-1] = ":class:`~typing.NamedTuple`"
    if name == "tree_sitter.LookaheadIterator":
        bases[-1] = ":class:`~collections.abc.Iterator`"


def setup(app):
    app.connect("autodoc-process-signature", process_signature)
    app.connect("autodoc-process-docstring", process_docstring)
    app.connect("autodoc-process-bases", process_bases)
