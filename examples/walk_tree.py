from typing import Generator
from tree_sitter import Language, Parser, Tree, Node
import tree_sitter_python

PY_LANGUAGE = Language(tree_sitter_python.language())

parser = Parser()
parser.set_language(PY_LANGUAGE)

tree = parser.parse(bytes("a = 1", "utf8"))


def traverse_tree(tree: Tree) -> Generator[Node, None, None]:
    cursor = tree.walk()

    visited_children = False
    while True:
        if not visited_children:
            yield cursor.node
            if not cursor.goto_first_child():
                visited_children = True
        elif cursor.goto_next_sibling():
            visited_children = False
        elif not cursor.goto_parent():
            break


node_names = map(lambda node: node.type, traverse_tree(tree))

assert list(node_names) == [
    "module",
    "expression_statement",
    "assignment",
    "identifier",
    "=",
    "integer",
]
