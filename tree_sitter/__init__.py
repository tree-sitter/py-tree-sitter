"""Python bindings to the Tree-sitter parsing library."""

from typing import Protocol as _Protocol

from ._binding import (
    Language,
    LogType,
    LookaheadIterator,
    Node,
    Parser,
    Point,
    Query,
    QueryCursor,
    QueryError,
    Range,
    Tree,
    TreeCursor,
    LANGUAGE_VERSION,
    MIN_COMPATIBLE_LANGUAGE_VERSION,
    __version__
)

LogType.__doc__ = "The type of a log message."


class QueryPredicate(_Protocol):
    """A custom query predicate that runs on a pattern."""
    def __call__(self, predicate, args, pattern_index, captures):
        """
        Parameters
        ----------

        predicate : str
            The name of the predicate.
        args : list[tuple[str, typing.Literal['capture', 'string']]]
            The arguments to the predicate.
        pattern_index : int
            The index of the pattern within the query.
        captures : dict[str, list[Node]]
            The captures contained in the pattern.

        Returns
        -------
        ``True`` if the predicate matches, ``False`` otherwise.

        Tip
        ---
        You don't need to create an actual class, just a function with this signature.
        """


__all__ = [
    "Language",
    "LogType",
    "LookaheadIterator",
    "Node",
    "Parser",
    "Point",
    "Query",
    "QueryCursor",
    "QueryError",
    "QueryPredicate",
    "Range",
    "Tree",
    "TreeCursor",
    "LANGUAGE_VERSION",
    "MIN_COMPATIBLE_LANGUAGE_VERSION",
    "__version__"
]
