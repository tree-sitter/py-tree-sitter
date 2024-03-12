from collections.abc import ByteString, Callable, Iterator, Sequence
from typing import Annotated, Any, Final, NamedTuple, final, overload

_Ptr = Annotated[int, "TSLanguage *"]

_ParseCB = Callable[[int, Point], bytes]

_UINT32_MAX = 0xFFFFFFFF


class Point(NamedTuple):
    row: int
    column: int


@final
class Language:
    def __init__(self, ptr: _Ptr, /) -> None: ...

    # TODO(?): add when ABI 15 is available
    # @property
    # def name(self) -> str: ...

    @property
    def version(self) -> int: ...

    @property
    def node_kind_count(self) -> int: ...

    @property
    def parse_state_count(self) -> int: ...

    @property
    def field_count(self) -> int: ...

    def node_kind_for_id(self, id: int, /) -> str | None: ...

    def id_for_node_kind(self, kind: str, named: bool, /) -> int | None: ...

    def node_kind_is_named(self, id: int, /) -> bool: ...

    def node_kind_is_visible(self, id: int, /) -> bool: ...

    def field_name_for_id(self, field_id: int, /) -> str | None: ...

    def field_id_for_name(self, name: str, /) -> int | None: ...

    def next_state(self, state: int, id: int, /) -> int: ...

    def lookahead_iterator(self, state: int, /) -> LookaheadIterator | None: ...

    def query(self, source: str, /) -> Query: ...

    def __repr__(self) -> str: ...

    def __eq__(self, other: Any, /) -> bool: ...

    def __ne__(self, other: Any, /) -> bool: ...

    def __int__(self) -> int: ...

    def __index__(self) -> int: ...

    def __hash__(self) -> int: ...


@final
class Node:
    @property
    def id(self) -> int: ...

    @property
    def kind_id(self) -> int: ...

    @property
    def grammar_id(self) -> int: ...

    @property
    def grammar_name(self) -> str: ...

    @property
    def type(self) -> str: ...

    @property
    def is_named(self) -> bool: ...

    @property
    def is_extra(self) -> bool: ...

    @property
    def has_changes(self) -> bool: ...

    @property
    def has_error(self) -> bool: ...

    @property
    def is_error(self) -> bool: ...

    @property
    def parse_state(self) -> int: ...

    @property
    def next_parse_state(self) -> int: ...

    @property
    def is_missing(self) -> bool: ...

    @property
    def start_byte(self) -> int: ...

    @property
    def end_byte(self) -> int: ...

    @property
    def byte_range(self) -> tuple[int, int]: ...

    @property
    def range(self) -> Range: ...

    @property
    def start_point(self) -> Point: ...

    @property
    def end_point(self) -> Point: ...

    @property
    def children(self) -> list[Node]: ...

    @property
    def child_count(self) -> int: ...

    @property
    def named_children(self) -> list[Node]: ...

    @property
    def named_child_count(self) -> int: ...

    @property
    def parent(self) -> Node | None: ...

    @property
    def next_sibling(self) -> Node | None: ...

    @property
    def prev_sibling(self) -> Node | None: ...

    @property
    def next_named_sibling(self) -> Node | None: ...

    @property
    def prev_named_sibling(self) -> Node | None: ...

    @property
    def descendant_count(self) -> int: ...

    @property
    def text(self) -> bytes | None: ...

    def walk(self) -> TreeCursor: ...

    def edit(
        self,
        start_byte: int,
        old_end_byte: int,
        new_end_byte: int,
        start_point: Point,
        old_end_point: Point,
        new_end_point: Point,
    ) -> None: ...

    def child(self, index: int, /) -> Node | None: ...

    def named_child(self, index: int, /) -> Node | None: ...

    def child_by_field_id(self, id: int, /) -> Node | None: ...

    def child_by_field_name(self, name: str, /) -> Node | None: ...

    def children_by_field_id(self, id: int, /) -> list[Node]: ...

    def children_by_field_name(self, name: str, /) -> list[Node]: ...

    def field_name_for_child(self, child_index: int, /) -> str | None: ...

    def descendant_for_byte_range(
        self,
        start_byte: int,
        end_byte: int,
        /,
    ) -> Node | None: ...

    def named_descendant_for_byte_range(
        self,
        start_byte: int,
        end_byte: int,
        /,
    ) -> Node | None: ...

    def descendant_for_point_range(
        self,
        start_point: Point,
        end_point: Point,
        /,
    ) -> Node | None: ...

    def named_descendant_for_point_range(
        self,
        start_point: Point,
        end_point: Point,
        /,
    ) -> Node | None: ...

    # NOTE: deprecated
    def sexp(self) -> str: ...

    def __repr__(self) -> str: ...

    def __str__(self) -> str: ...

    def __eq__(self, other: Any, /) -> bool: ...

    def __ne__(self, other: Any, /) -> bool: ...

    def __hash__(self) -> int: ...


@final
class Tree:
    @property
    def root_node(self) -> Node: ...

    @property
    def included_ranges(self) -> list[Range]: ...

    @property
    def text(self) -> bytes | None: ...

    def root_node_with_offset(
        self,
        offset_bytes: int,
        offset_extent: Point,
        /,
    ) -> Node | None: ...

    def edit(
        self,
        start_byte: int,
        old_end_byte: int,
        new_end_byte: int,
        start_point: Point,
        old_end_point: Point,
        new_end_point: Point,
    ) -> None: ...

    def walk(self) -> TreeCursor: ...

    def changed_ranges(self, old_tree: Tree) -> list[Range]: ...


@final
class TreeCursor:
    @property
    def node(self) -> Node: ...

    @property
    def field_id(self) -> int | None: ...

    @property
    def field_name(self) -> str | None: ...

    @property
    def depth(self) -> int: ...

    @property
    def descendant_index(self) -> int: ...

    def copy(self) -> TreeCursor: ...

    def reset(self, node: Node, /) -> None: ...

    def reset_to(self, cursor: TreeCursor, /) -> None: ...

    def goto_first_child(self) -> bool: ...

    def goto_last_child(self) -> bool: ...

    def goto_parent(self) -> bool: ...

    def goto_next_sibling(self) -> bool: ...

    def goto_previous_sibling(self) -> bool: ...

    def goto_descendant(self, index: int, /) -> None: ...

    def goto_first_child_for_byte(self, byte: int, /) -> bool: ...

    @overload
    def goto_first_child_for_point(self, point: Point, /) -> bool: ...

    # NOTE: deprecated
    @overload
    def goto_first_child_for_point(self, row: int, column: int, /) -> bool: ...

    def __copy__(self) -> TreeCursor: ...


@final
class Parser:
    def __init__(
        self,
        language: Language | None = None,
        *,
        included_ranges: Sequence[Range] | None = None,
        timeout_micros: int | None = None
    ) -> None: ...

    @property
    def language(self) -> Language | None: ...

    @language.setter
    def language(self, language: Language) -> None: ...

    @property
    def included_ranges(self) -> list[Range]: ...

    @included_ranges.setter
    def included_ranges(self, ranges: Sequence[Range]) -> None: ...

    @property
    def timeout_micros(self) -> int: ...

    @timeout_micros.setter
    def timeout_micros(self, timeout: int) -> None: ...

    # TODO(0.24): implement logger

    def parse(
        self,
        source: ByteString | _ParseCB | None,
        /,
        old_tree: Tree | None = None,
        # NOTE: deprecated
        keep_text: bool = True,
    ) -> Tree: ...

    def reset(self) -> None: ...

    # NOTE: deprecated
    def set_language(self, language: Language, /) -> None: ...

    # NOTE: deprecated
    def set_included_ranges(self, ranges: Sequence[Range], /) -> None: ...

    # NOTE: deprecated
    def set_timeout_micros(self, timeout: int, /) -> None: ...


@final
class Query:
    def __init__(self, language: Language, source: str) -> None: ...

    # TODO(0.23): implement more Query methods

    # TODO(0.23): return `dict[str, Node]`
    def captures(
        self,
        node: Node,
        *,
        start_point: Point = Point(0, 0),
        end_point: Point = Point(_UINT32_MAX, _UINT32_MAX),
        start_byte: int = 0,
        end_byte: int = _UINT32_MAX,
    ) -> list[tuple[Node, str]]: ...

    def matches(
        self,
        node: Node,
        *,
        start_point: Point = Point(0, 0),
        end_point: Point = Point(_UINT32_MAX, _UINT32_MAX),
        start_byte: int = 0,
        end_byte: int = _UINT32_MAX,
    ) -> list[tuple[int, dict[str, Node | list[Node]]]]: ...


@final
class LookaheadIterator(Iterator[int]):
    @property
    def language(self) -> Language: ...

    @property
    def current_symbol(self) -> int: ...

    @property
    def current_symbol_name(self) -> str: ...

    # NOTE: deprecated
    def reset(self, language: _Ptr, state: int, /) -> None: ...

    # TODO(0.24): rename to reset
    def reset_state(self, state: int, language: Language | None = None) -> None: ...

    def iter_names(self) -> LookaheadNamesIterator: ...

    def __next__(self) -> int: ...


@final
class LookaheadNamesIterator(Iterator[str]):
    def __next__(self) -> str: ...


@final
class Range:
    def __init__(
        self,
        start_point: Point,
        end_point: Point,
        start_byte: int,
        end_byte: int,
    ) -> None: ...

    @property
    def start_point(self): Point

    @property
    def end_point(self): Point

    @property
    def start_byte(self): int

    @property
    def end_byte(self): int

    def __eq__(self, other: Any, /) -> bool: ...

    def __ne__(self, other: Any, /) -> bool: ...

    def __repr__(self) -> str: ...

    def __hash__(self) -> int: ...


LANGUAGE_VERSION: Final[int]

MIN_COMPATIBLE_LANGUAGE_VERSION: Final[int]
