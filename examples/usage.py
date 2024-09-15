from tree_sitter import Language, Parser
import tree_sitter_python

PY_LANGUAGE = Language(tree_sitter_python.language())

parser = Parser(PY_LANGUAGE)

# parsing a string of code
tree = parser.parse(
    bytes(
        """
def foo():
    if bar:
        baz()
""",
        "utf8",
    )
)

# parsing a callable by using the byte offset
src = bytes(
    """
def foo():
    if bar:
        baz()
""",
    "utf8",
)


def read_callable_byte_offset(byte_offset, _):
    return src[byte_offset : byte_offset + 1]


tree = parser.parse(read_callable_byte_offset)


# parsing a callable by using the point
src_lines = ["\n", "def foo():\n", "    if bar:\n", "        baz()\n"]


def read_callable_point(_, point):
    row, column = point
    if row >= len(src_lines) or column >= len(src_lines[row]):
        return None
    return src_lines[row][column:].encode("utf8")


tree = parser.parse(read_callable_point)

# inspecting nodes in the tree
root_node = tree.root_node
assert root_node.type == "module"
assert root_node.start_point == (1, 0)
assert root_node.end_point == (4, 0)

function_node = root_node.child(0)
assert function_node.type == "function_definition"
assert function_node.child_by_field_name("name").type == "identifier"

function_name_node = function_node.child(1)
assert function_name_node.type == "identifier"
assert function_name_node.start_point == (1, 4)
assert function_name_node.end_point == (1, 7)

function_body_node = function_node.child_by_field_name("body")

if_statement_node = function_body_node.child(0)
assert if_statement_node.type == "if_statement"

function_call_node = if_statement_node.child_by_field_name("consequence").child(0).child(0)
assert function_call_node.type == "call"

function_call_name_node = function_call_node.child_by_field_name("function")
assert function_call_name_node.type == "identifier"

function_call_args_node = function_call_node.child_by_field_name("arguments")
assert function_call_args_node.type == "argument_list"


# getting the sexp representation of the tree
assert str(root_node) == (
    "(module "
    "(function_definition "
    "name: (identifier) "
    "parameters: (parameters) "
    "body: (block "
    "(if_statement "
    "condition: (identifier) "
    "consequence: (block "
    "(expression_statement (call "
    "function: (identifier) "
    "arguments: (argument_list))))))))"
)

# walking the tree
cursor = tree.walk()

assert cursor.node.type == "module"

assert cursor.goto_first_child()
assert cursor.node.type == "function_definition"

assert cursor.goto_first_child()
assert cursor.node.type == "def"

# Returns `False` because the `def` node has no children
assert not cursor.goto_first_child()

assert cursor.goto_next_sibling()
assert cursor.node.type == "identifier"

assert cursor.goto_next_sibling()
assert cursor.node.type == "parameters"

assert cursor.goto_parent()
assert cursor.node.type == "function_definition"

# editing the tree
new_src = src[:5] + src[5 : 5 + 2].upper() + src[5 + 2 :]

tree.edit(
    start_byte=5,
    old_end_byte=5,
    new_end_byte=5 + 2,
    start_point=(0, 5),
    old_end_point=(0, 5),
    new_end_point=(0, 5 + 2),
)

new_tree = parser.parse(new_src, tree)

# inspecting the changes
for changed_range in tree.changed_ranges(new_tree):
    print("Changed range:")
    print(f"  Start point {changed_range.start_point}")
    print(f"  Start byte {changed_range.start_byte}")
    print(f"  End point {changed_range.end_point}")
    print(f"  End byte {changed_range.end_byte}")


# querying the tree
query = PY_LANGUAGE.query(
    """
(function_definition
  name: (identifier) @function.def
  body: (block) @function.block)

(call
  function: (identifier) @function.call
  arguments: (argument_list) @function.args)
"""
)

# ...with captures
captures = query.captures(tree.root_node)
assert len(captures) == 4
assert captures["function.def"][0] == function_name_node
assert captures["function.block"][0] == function_body_node
assert captures["function.call"][0] == function_call_name_node
assert captures["function.args"][0] == function_call_args_node

# ...with matches
matches = query.matches(tree.root_node)
assert len(matches) == 2

# first match
assert matches[0][1]["function.def"] == [function_name_node]
assert matches[0][1]["function.block"] == [function_body_node]

# second match
assert matches[1][1]["function.call"] == [function_call_name_node]
assert matches[1][1]["function.args"] == [function_call_args_node]
