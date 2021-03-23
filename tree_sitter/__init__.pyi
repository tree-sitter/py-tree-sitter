from typing import Optional, List, Iterator


class Language:
    def __init__(self, path: str, name: str): ...

    def field_id_for_name(self, name: str) -> int: ...
    def query(self, source) -> Query: ...


class Parser:
    def set_language(self, lang: Language) -> None: ...
    def parse(self, src: bytes) -> Tree: ...


class Node:
    type: str
    is_named: bool
    is_missing: bool
    has_changes: bool
    has_error: bool
    start_byte: int
    end_byte: int
    start_point: Point
    end_point: Point
    children: List[Node]
    child_count: int
    named_child_count: int
    next_sibling: Optional[Node]
    prev_sibling: Optional[Node]
    next_named_sibling: Optional[Node]
    prev_named_sibling: Optional[Node]
    parent: Optional[Node]

    def walk(self) -> TreeCursor: ...
    def sexp(self) -> str: ...
    def child_by_field_id(self, id: int) -> Optional[Node]: ...
    def child_by_field_name(self, name: str) -> Optional[Node]: ...
    def children_by_field_id(self, id: int) -> Iterator[Node]: ...
    def children_by_field_name(self, name: str) -> Iterator[Node]: ...


class Tree:
    root_node: Node
    walk(self): TreeCursor


class TreeCursor:
    node: Node

    def current_field_name(self) -> str: ...
    def goto_parent(self) -> bool: ...
    def goto_first_child(self) -> bool: ...
    def goto_next_sibling(self) -> bool: ...


class Query:
    def captures(self, node: Node) -> List[Tuple[Node, str]]
