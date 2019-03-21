Py-Tree-sitter
==================

[![Build Status](https://travis-ci.org/tree-sitter/py-tree-sitter.svg?branch=master)](https://travis-ci.org/tree-sitter/py-tree-sitter)

This module provides Python bindings to the [tree-sitter](https://github.com/tree-sitter/tree-sitter) parsing library.

## Installation

This package currently only works with Python 3. There are no library dependencies.

```sh
pip3 install tree_sitter
```

## Usage

#### Setup

First you'll need a Tree-sitter language implementation for each language that you want to parse. You can clone some of the [existing language repos](https://github.com/tree-sitter) or [create your own](http://tree-sitter.github.io/tree-sitter/creating-parsers):

```sh
git clone https://github.com/tree-sitter/tree-sitter-go
git clone https://github.com/tree-sitter/tree-sitter-javascript
git clone https://github.com/tree-sitter/tree-sitter-python
```

Use the `Language.build_library` method to compile these into a library that's usable from Python. This function will return immediately if the library has already been compiled since the last time its source code was modified:

```python
from tree_sitter import Language

Language.build_library(
  # Store the library in the `build` directory
  'build/my-languages.so',

  # Include one or more languages
  [
    'vendor/tree-sitter-go',
    'vendor/tree-sitter-javascript',
    'vendor/tree-sitter-python'
  ]
)
```

Load the languages into your app as `Language` objects:

```python
GO_LANGUAGE = Language('build/my-languages.so', 'go')
JS_LANGUAGE = Language('build/my-languages.so', 'javascript')
PY_LANGUAGE = Language('build/my-languages.so', 'python')
```

#### Basic Parsing

Create a `Parser` and configure it to use one of the languages:

```python
parser = Parser()
parser.set_language(PY_LANGUAGE)
```

Parse some source code:

```python
tree = parser.parse("""
def foo():
    if bar:
        baz()
""")
```

Inspect the resulting `Tree`:

```python
root_node = tree.root_node
assert root_node.type == 'module'
assert root_node.start_point == (1, 0)
assert root_node.end_point == (3, 13)

function_node = root_node.children[0]
assert root_node.type == 'function_definition'

function_name_node = function_node.children[1]
assert function_name_node.type == 'identifier'
assert function_name_node.start_point == (1, 4)
assert function_name_node.end_point == (1, 7)

assert root_node.sexp() == ''
```
