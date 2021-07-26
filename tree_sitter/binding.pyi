from typing import Optional, List, Tuple, Any

import tree_sitter

# ----------------------------------- Node ----------------------------------- #

class Node:
    """A syntax node"""

    def walk(self) -> TreeCursor:
        """Get a tree cursor for walking the tree starting at this node."""
        ...
    
    def sexp(self) -> str:
        """Get an S-expression representing the node."""
        ...
    
    def child_by_field_id(self, id: int) -> Optional[Node]:
        """Get child for the given field id."""
        ...

    def child_by_field_name(self, name: str) -> Optional[Node]:
        """Get child for the given field name."""
        ...

    @property
    def type(self) -> str:
        """The node's type"""
        ...
    @property
    def is_named(self) -> bool:
        """Is this a named node"""
        ...
    @property
    def is_missing(self) -> bool:
        """Is this a node inserted by the parser"""
        ...
    @property
    def has_changes(self) -> bool:
        """Does this node have text changes since it was parsed"""
        ...
    @property
    def has_error(self) -> bool:
        """Does this node contain any errors"""
        ...
    @property
    def start_byte(self) -> int:
        """The node's start byte"""
        ...
    @property
    def end_byte(self) -> int:
        """The node's end byte"""
        ...
    @property
    def start_point(self) -> Tuple[int,int]:
        """The node's start point"""
        ...
    @property
    def end_point(self) -> Tuple[int,int]:
        """The node's end point"""
        ...
    @property
    def children(self) -> List[Node]:
        """The node's children"""
        ...
    @property
    def child_count(self) -> int:
        """The number of children for a node"""
        ...
    @property
    def named_child_count(self) -> int:
        """The number of named children for a node"""
        ...
    @property
    def next_sibling(self) -> Optional[Node]:
        """The node's next sibling"""
        ...
    @property
    def prev_sibling(self) -> Optional[Node]:
        """The node's previous sibling"""
        ...
    @property
    def next_named_sibling(self) -> Optional[Node]:
        """The node's next named sibling"""
        ...
    @property
    def prev_named_sibling(self) -> Optional[Node]:
        """The node's previous named sibling"""
        ...
    @property
    def parent(self) -> Optional[Node]:
        """The node's parent"""
        ...

class Tree:
    """A Syntax Tree"""

    def walk(self) -> TreeCursor:
        """Get a tree cursor for walking this tree."""
        ...

    def edit(self, start_byte: int, old_end_byte: int, new_end_byte: int, start_point: Tuple[int,int], old_end_point: Tuple[int,int], new_end_point: Tuple[int,int]) -> None:
        """Edit the syntax tree."""
        ...

    @property
    def root_node(self) -> Node:
        """The root node of this tree."""
        ...

class TreeCursor:
    """A syntax tree cursor."""
    
    def current_field_name(self) -> Optional[str]:
        """Get the field name of the tree cursor's current node.
        
        If the current node has the field name, return str. Otherwise, return None.
        """
        ...

    def goto_parent(self) -> bool:
        """Go to parent.

        If the current node is not the root, move to its parent and
        return True. Otherwise, return False.
        """
        ...

    def goto_first_child(self) -> bool:
        """Go to first child.

        If the current node has children, move to the first child and
        return True. Otherwise, return False.
        """
        ...

    def goto_next_sibling(self) -> bool:
        """Go to next sibling.
               
        If the current node has a next sibling, move to the next sibling
        and return True. Otherwise, return False.
        """
        ...

    @property
    def node(self) -> Node:
        """The current node."""
        ...

class Parser:
    """A Parser"""

    def parse(self, source_code: bytes, old_tree: Tree = None) -> Tree:
        """Parse source code, creating a syntax tree."""
        ...

    def set_language(self, language: tree_sitter.Language) -> None:
        """Set the parser language."""
        ...

class Query:
    """A set of patterns to search for in a syntax tree."""

    def matches(self, node: Node):
        """Get a list of all of the matches within the given node."""
        ...

    def captures(self, node: Node, start_point: Tuple[int,int] = None, end_point: Tuple[int,int] = None) -> List[Tuple[Node, str]]:
        """Get a list of all of the captures within the given node."""
        ...


def _language_field_id_for_name(language_id: Any, name: str) -> int:
    """(internal)"""
    ...

def _language_query(language_id: Any, source: str) -> Query:
    """(internal)"""
    ...
