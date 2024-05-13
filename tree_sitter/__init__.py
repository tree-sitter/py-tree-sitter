"""Python bindings to the Tree-sitter parsing library."""

from ._binding import (
    Language,
    LookaheadIterator,
    Node,
    Parser,
    Point,
    Query,
    Range,
    Tree,
    TreeCursor,
    LANGUAGE_VERSION,
    MIN_COMPATIBLE_LANGUAGE_VERSION,
)

Point.__doc__ = "A position in a multi-line text document, in terms of rows and columns."
Point.row.__doc__ = "The zero-based row of the document."
Point.column.__doc__ = "The zero-based column of the document."

__all__ = [
    "Language",
    "LookaheadIterator",
    "Node",
    "Parser",
    "Point",
    "Query",
    "Range",
    "Tree",
    "TreeCursor",
    "LANGUAGE_VERSION",
    "MIN_COMPATIBLE_LANGUAGE_VERSION",
]
