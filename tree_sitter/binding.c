#include "Python.h"
#include "tree_sitter/api.h"

// Types

typedef struct {
  PyObject_HEAD
  TSNode node;
  PyObject *children;
} Node;

typedef struct {
  PyObject_HEAD
  TSTree *tree;
} Tree;

typedef struct {
  PyObject_HEAD
  TSParser *parser;
} Parser;

typedef struct {
  PyObject_HEAD
  TSTreeCursor cursor;
  PyObject *node;
} TreeCursor;

static TSTreeCursor default_cursor = {0};

// Point

static PyObject *point_new(TSPoint point) {
  PyObject *row = PyLong_FromSize_t((size_t)point.row);
  PyObject *column = PyLong_FromSize_t((size_t)point.column);
  if (!row || !column) {
    Py_XDECREF(row);
    Py_XDECREF(column);
    return NULL;
  }
  return PyTuple_Pack(2, row, column);
}

// Node

static PyObject *node_new_internal(TSNode node);
static PyObject *tree_cursor_new_internal(TSNode node);

static void node_dealloc(Node *self) {
  Py_XDECREF(self->children);
  Py_TYPE(self)->tp_free(self);
}

static PyObject *node_repr(Node *self) {
  const char *type = ts_node_type(self->node);
  TSPoint start_point = ts_node_start_point(self->node);
  TSPoint end_point = ts_node_end_point(self->node);
  const char *format_string = ts_node_is_named(self->node)
    ? "<Node kind=%s, start_point=(%u, %u), end_point=(%u, %u)>"
    : "<Node kind=\"%s\", start_point=(%u, %u), end_point=(%u, %u)>";
  return PyUnicode_FromFormat(
    format_string,
    type,
    start_point.row,
    start_point.column,
    end_point.row,
    end_point.column
  );
}

static PyObject *node_sexp(Node *self, PyObject *args) {
  char *string = ts_node_string(self->node);
  PyObject *result = PyUnicode_FromString(string);
  free(string);
  return result;
}

static PyObject *node_walk(Node *self, PyObject *args) {
  return tree_cursor_new_internal(self->node);
}

static PyObject *node_chield_by_field_id(Node *self, PyObject *args) {
  TSFieldId field_id;
  if (!PyArg_ParseTuple(args, "H", &field_id)) {
    return NULL;
  }
  TSNode child = ts_node_child_by_field_id(self->node, field_id);
  if (ts_node_is_null(child)) {
    Py_RETURN_NONE;
  }
  return node_new_internal(child);
}

static PyObject *node_chield_by_field_name(Node *self, PyObject *args) {
  char *name;
  int length;
  if (!PyArg_ParseTuple(args, "s#", &name, &length)) {
    return NULL;
  }
  TSNode child = ts_node_child_by_field_name(self->node, name, length);
  if (ts_node_is_null(child)) {
    Py_RETURN_NONE;
  }
  return node_new_internal(child);
}

static PyObject *node_get_type(Node *self, void *payload) {
  return PyUnicode_FromString(ts_node_type(self->node));
}

static PyObject *node_get_is_named(Node *self, void *payload) {
  return PyBool_FromLong(ts_node_is_named(self->node));
}

static PyObject *node_get_has_changes(Node *self, void *payload) {
  return PyBool_FromLong(ts_node_has_changes(self->node));
}

static PyObject *node_get_has_error(Node *self, void *payload) {
  return PyBool_FromLong(ts_node_has_error(self->node));
}

static PyObject *node_get_start_byte(Node *self, void *payload) {
  return PyLong_FromSize_t((size_t)ts_node_start_byte(self->node));
}

static PyObject *node_get_end_byte(Node *self, void *payload) {
  return PyLong_FromSize_t((size_t)ts_node_end_byte(self->node));
}

static PyObject *node_get_start_point(Node *self, void *payload) {
  return point_new(ts_node_start_point(self->node));
}

static PyObject *node_get_end_point(Node *self, void *payload) {
  return point_new(ts_node_end_point(self->node));
}

static PyObject *node_get_children(Node *self, void *payload) {
  if (self->children) {
    Py_INCREF(self->children);
    return self->children;
  }

  long length = (long)ts_node_child_count(self->node);
  PyObject *result = PyList_New(length);
  if (length > 0) {
    ts_tree_cursor_reset(&default_cursor, self->node);
    ts_tree_cursor_goto_first_child(&default_cursor);
    int i = 0;
    do {
      TSNode child = ts_tree_cursor_current_node(&default_cursor);
      PyList_SetItem(result, i, node_new_internal(child));
      i++;
    } while (ts_tree_cursor_goto_next_sibling(&default_cursor));
  }
  Py_INCREF(result);
  self->children = result;
  return result;
}

static PyMethodDef node_methods[] = {
  {
    .ml_name = "walk",
    .ml_meth = (PyCFunction)node_walk,
    .ml_flags = METH_NOARGS,
    .ml_doc = "walk()\n--\n\n\
               Get a tree cursor for walking the tree starting at this node.",
  },
  {
    .ml_name = "sexp",
    .ml_meth = (PyCFunction)node_sexp,
    .ml_flags = METH_NOARGS,
    .ml_doc = "sexp()\n--\n\n\
               Get an S-expression representing the node.",
  },
  {
    .ml_name = "child_by_field_id",
    .ml_meth = (PyCFunction)node_chield_by_field_id,
    .ml_flags = METH_VARARGS,
    .ml_doc = "child_by_field_id(id)\n--\n\n\
               Get child for the given field id.",
  },
  {
    .ml_name = "child_by_field_name",
    .ml_meth = (PyCFunction)node_chield_by_field_name,
    .ml_flags = METH_VARARGS,
    .ml_doc = "child_by_field_name(name)\n--\n\n\
               Get child for the given field name.",
  },
  {NULL},
};

static PyGetSetDef node_accessors[] = {
  {"type", (getter)node_get_type, NULL, "The node's type", NULL},
  {"is_named", (getter)node_get_is_named, NULL, "Is this a named node", NULL},
  {"has_changes", (getter)node_get_has_changes, NULL, "Does this node have text changes since it was parsed", NULL},
  {"has_error", (getter)node_get_has_error, NULL, "Does this node contain any errors", NULL},
  {"start_byte", (getter)node_get_start_byte, NULL, "The node's start byte", NULL},
  {"end_byte", (getter)node_get_end_byte, NULL, "The node's end byte", NULL},
  {"start_point", (getter)node_get_start_point, NULL, "The node's start point", NULL},
  {"end_point", (getter)node_get_end_point, NULL, "The node's end point", NULL},
  {"children", (getter)node_get_children, NULL, "The node's children", NULL},
  {NULL}
};

static PyTypeObject node_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "tree_sitter.Node",
  .tp_doc = "A syntax node",
  .tp_basicsize = sizeof(Node),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_dealloc = (destructor)node_dealloc,
  .tp_repr = (reprfunc)node_repr,
  .tp_methods = node_methods,
  .tp_getset = node_accessors,
};

static PyObject *node_new_internal(TSNode node) {
  Node *self = (Node *)node_type.tp_alloc(&node_type, 0);
  if (self != NULL) {
    self->node = node;
    self->children = NULL;
  }
  return (PyObject *)self;
}

// Tree

static void tree_dealloc(Tree *self) {
  ts_tree_delete(self->tree);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *tree_get_root_node(Tree *self, void *payload) {
  return node_new_internal(ts_tree_root_node(self->tree));
}

static PyObject *tree_walk(Tree *self, PyObject *args) {
  return tree_cursor_new_internal(ts_tree_root_node(self->tree));
}

static PyObject *tree_edit(Tree *self, PyObject *args, PyObject *kwargs) {
  unsigned start_byte, start_row, start_column;
  unsigned old_end_byte, old_end_row, old_end_column;
  unsigned new_end_byte, new_end_row, new_end_column;

  char *keywords[] = {
    "start_byte",
    "old_end_byte",
    "new_end_byte",
    "start_point",
    "old_end_point",
    "new_end_point",
    NULL,
  };

  int ok = PyArg_ParseTupleAndKeywords(
    args,
    kwargs,
    "III(II)(II)(II)",
    keywords,
    &start_byte,
    &old_end_byte,
    &new_end_byte,
    &start_row,
    &start_column,
    &old_end_row,
    &old_end_column,
    &new_end_row,
    &new_end_column
  );

  if (ok) {
    TSInputEdit edit = {
      .start_byte = start_byte,
      .old_end_byte = old_end_byte,
      .new_end_byte = new_end_byte,
      .start_point = {start_row, start_column},
      .old_end_point = {old_end_row, old_end_column},
      .new_end_point = {new_end_row, new_end_column},
    };
    ts_tree_edit(self->tree, &edit);
  }
  Py_RETURN_NONE;
}

static PyMethodDef tree_methods[] = {
  {
    .ml_name = "walk",
    .ml_meth = (PyCFunction)tree_walk,
    .ml_flags = METH_NOARGS,
    .ml_doc = "walk()\n--\n\n\
               Get a tree cursor for walking this tree.",
  },
  {
    .ml_name = "edit",
    .ml_meth = (PyCFunction)tree_edit,
    .ml_flags = METH_KEYWORDS|METH_VARARGS,
    .ml_doc = "edit(start_byte, old_end_byte, new_end_byte,\
               start_point, old_end_point, new_end_point)\n--\n\n\
               Edit the syntax tree.",
  },
  {NULL},
};

static PyGetSetDef tree_accessors[] = {
  {"root_node", (getter)tree_get_root_node, NULL, "The root node of this tree.", NULL},
  {NULL}
};

static PyTypeObject tree_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "tree_sitter.Tree",
  .tp_doc = "A Syntax Tree",
  .tp_basicsize = sizeof(Tree),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_dealloc = (destructor)tree_dealloc,
  .tp_methods = tree_methods,
  .tp_getset = tree_accessors,
};

static PyObject *tree_new_internal(TSTree *tree) {
  Tree *self = (Tree *)tree_type.tp_alloc(&tree_type, 0);
  if (self != NULL) self->tree = tree;
  return (PyObject *)self;
}

// TreeCursor

static void tree_cursor_dealloc(TreeCursor *self) {
  ts_tree_cursor_delete(&self->cursor);
  Py_XDECREF(self->node);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *tree_cursor_get_node(TreeCursor *self, void *payload) {
  if (!self->node) {
    self->node = node_new_internal(ts_tree_cursor_current_node(&self->cursor));
  }

  Py_INCREF(self->node);
  return self->node;
}

static PyObject *tree_cursor_goto_parent(TreeCursor *self, PyObject *args) {
  bool result = ts_tree_cursor_goto_parent(&self->cursor);
  if (result) {
    Py_XDECREF(self->node);
    self->node = NULL;
  }
  return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_first_child(TreeCursor *self, PyObject *args) {
  bool result = ts_tree_cursor_goto_first_child(&self->cursor);
  if (result) {
    Py_XDECREF(self->node);
    self->node = NULL;
  }
  return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_next_sibling(TreeCursor *self, PyObject *args) {
  bool result = ts_tree_cursor_goto_next_sibling(&self->cursor);
  if (result) {
    Py_XDECREF(self->node);
    self->node = NULL;
  }
  return PyBool_FromLong(result);
}

static PyMethodDef tree_cursor_methods[] = {
  {
    .ml_name = "goto_parent",
    .ml_meth = (PyCFunction)tree_cursor_goto_parent,
    .ml_flags = METH_NOARGS,
    .ml_doc = "goto_parent()\n--\n\n\
               Go to parent.\n\n\
               If the current node is not the root, move to its parent and\n\
               return True. Otherwise, return False.",
  },
  {
    .ml_name = "goto_first_child",
    .ml_meth = (PyCFunction)tree_cursor_goto_first_child,
    .ml_flags = METH_NOARGS,
    .ml_doc = "goto_first_child()\n--\n\n\
               Go to first child.\n\n\
               If the current node has children, move to the first child and\n\
               return True. Otherwise, return False.",
  },
  {
    .ml_name = "goto_next_sibling",
    .ml_meth = (PyCFunction)tree_cursor_goto_next_sibling,
    .ml_flags = METH_NOARGS,
    .ml_doc = "goto_next_sibling()\n--\n\n\
               Go to next sibling.\n\n\
               If the current node has a next sibling, move to the next sibling\n\
               and return True. Otherwise, return False.",
  },
  {NULL},
};

static PyGetSetDef tree_cursor_accessors[] = {
  {"node", (getter)tree_cursor_get_node, NULL, "The current node.", NULL},
  {NULL},
};

static PyTypeObject tree_cursor_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "tree_sitter.TreeCursor",
  .tp_doc = "A syntax tree cursor.",
  .tp_basicsize = sizeof(TreeCursor),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_dealloc = (destructor)tree_cursor_dealloc,
  .tp_methods = tree_cursor_methods,
  .tp_getset = tree_cursor_accessors,
};

static PyObject *tree_cursor_new_internal(TSNode node) {
  TreeCursor *cursor = (TreeCursor *)tree_cursor_type.tp_alloc(&tree_cursor_type, 0);
  if (cursor != NULL) cursor->cursor = ts_tree_cursor_new(node);
  return (PyObject *)cursor;
}

// Parser

static PyObject *parser_new(
  PyTypeObject *type,
  PyObject *args,
  PyObject *kwds
) {
  Parser *self = (Parser *)type->tp_alloc(type, 0);
  if (self != NULL) self->parser = ts_parser_new();
  return (PyObject *)self;
}

static void parser_dealloc(Parser *self) {
  ts_parser_delete(self->parser);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *parser_parse(Parser *self, PyObject *args) {
  PyObject *source_code = NULL;
  PyObject *old_tree_arg = NULL;
  if (!PyArg_UnpackTuple(args, "ref", 1, 2, &source_code, &old_tree_arg)) {
    return NULL;
  }

  if (!PyBytes_Check(source_code)) {
    PyErr_SetString(PyExc_TypeError, "First argument to parse must be bytes");
    return NULL;
  }

  const TSTree *old_tree = NULL;
  if (old_tree_arg) {
    if (!PyObject_IsInstance(old_tree_arg, (PyObject *)&tree_type)) {
      PyErr_SetString(PyExc_TypeError, "Second argument to parse must be a Tree");
      return NULL;
    }

    old_tree = ((Tree *)old_tree_arg)->tree;
  }

  size_t length = PyBytes_Size(source_code);
  char *source_bytes = PyBytes_AsString(source_code);
  TSTree *new_tree = ts_parser_parse_string(self->parser, old_tree, source_bytes, length);

  if (!new_tree) {
    PyErr_SetString(PyExc_ValueError, "Parsing failed");
    return NULL;
  }

  return tree_new_internal(new_tree);
}

static PyObject *parser_set_language(Parser *self, PyObject *arg) {
  PyObject *language_id = PyObject_GetAttrString(arg, "language_id");
  if (!language_id) {
    PyErr_SetString(PyExc_TypeError, "Argument to set_language must be a Language");
    return NULL;
  }

  if (!PyLong_Check(language_id)) {
    PyErr_SetString(PyExc_TypeError, "Language ID must be an integer");
    return NULL;
  }

  TSLanguage *language = (TSLanguage *)PyLong_AsLong(language_id);
  if (!language) {
    PyErr_SetString(PyExc_ValueError, "Language ID must not be null");
    return NULL;
  }

  unsigned version = ts_language_version(language);
  if (version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION || TREE_SITTER_LANGUAGE_VERSION < version) {
    return PyErr_Format(
      PyExc_ValueError,
      "Incompatible Language version %u. Must not be between %u and %u",
      version,
      TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION,
      TREE_SITTER_LANGUAGE_VERSION
    );
  }

  ts_parser_set_language(self->parser, language);
  Py_RETURN_NONE;
}

static PyMethodDef parser_methods[] = {
  {
    .ml_name = "parse",
    .ml_meth = (PyCFunction)parser_parse,
    .ml_flags = METH_VARARGS,
    .ml_doc = "parse(bytes, old_tree=None)\n--\n\n\
               Parse source code, creating a syntax tree.",
  },
  {
    .ml_name = "set_language",
    .ml_meth = (PyCFunction)parser_set_language,
    .ml_flags = METH_O,
    .ml_doc = "set_language(language)\n--\n\n\
               Set the parser language.",
  },
  {NULL},
};

static PyTypeObject parser_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "tree_sitter.Parser",
  .tp_doc = "A Parser",
  .tp_basicsize = sizeof(Parser),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = parser_new,
  .tp_dealloc = (destructor)parser_dealloc,
  .tp_methods = parser_methods,
};

// Module


static PyObject *language_field_id_for_name(Node *self, PyObject *args) {
  TSLanguage *language;
  char *field_name;
  int length;
  if (!PyArg_ParseTuple(args, "ls#", &language, &field_name, &length)) {
    return NULL;
  }

  TSFieldId field_id = ts_language_field_id_for_name(language, field_name, length);
  if (field_id == 0) {
    Py_RETURN_NONE;
  }

  return PyLong_FromSize_t((size_t)field_id);
}

static PyMethodDef module_methods[] = {
  {
    .ml_name = "_language_field_id_for_name",
    .ml_meth = (PyCFunction)language_field_id_for_name,
    .ml_flags = METH_VARARGS,
    .ml_doc = "(internal)",
  },
  {NULL},
};

static struct PyModuleDef module_definition = {
  .m_base = PyModuleDef_HEAD_INIT,
  .m_name = "binding",
  .m_doc = NULL,
  .m_size = -1,
  .m_methods = module_methods,
};

PyMODINIT_FUNC PyInit_binding(void) {
  PyObject *module = PyModule_Create(&module_definition);
  if (module == NULL) return NULL;

  if (PyType_Ready(&parser_type) < 0) return NULL;
  Py_INCREF(&parser_type);
  PyModule_AddObject(module, "Parser", (PyObject *)&parser_type);

  if (PyType_Ready(&tree_type) < 0) return NULL;
  Py_INCREF(&tree_type);
  PyModule_AddObject(module, "Tree", (PyObject *)&tree_type);

  if (PyType_Ready(&node_type) < 0) return NULL;
  Py_INCREF(&node_type);
  PyModule_AddObject(module, "Node", (PyObject *)&node_type);

  if (PyType_Ready(&tree_cursor_type) < 0) return NULL;
  Py_INCREF(&tree_cursor_type);
  PyModule_AddObject(module, "TreeCursor", (PyObject *)&tree_cursor_type);

  return module;
}
