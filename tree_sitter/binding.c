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

static PyObject *node_get_type(Node *self, void *payload) {
  return PyUnicode_FromString(ts_node_type(self->node));
}

static PyObject *node_get_is_named(Node *self, void *payload) {
  return PyBool_FromLong(ts_node_is_named(self->node));
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
    .ml_name = "sexp",
    .ml_meth = (PyCFunction)node_sexp,
    .ml_flags = METH_NOARGS,
    .ml_doc = "Get an S-expression representing the name",
  },
  {NULL},
};

static PyGetSetDef node_accessors[] = {
  {"type", (getter)node_get_type, NULL, "The node's type", NULL},
  {"is_named", (getter)node_get_is_named, NULL, "Is this a named node", NULL},
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

static PyMethodDef tree_methods[] = {
  {NULL},
};

static PyGetSetDef tree_accessors[] = {
  {"root_node", (getter)tree_get_root_node, NULL, "root node", NULL},
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
  return Py_None;
}

static PyMethodDef parser_methods[] = {
  {
    .ml_name = "parse",
    .ml_meth = (PyCFunction)parser_parse,
    .ml_flags = METH_VARARGS,
    .ml_doc = "Parse source code, creating a syntax tree",
  },
  {
    .ml_name = "set_language",
    .ml_meth = (PyCFunction)parser_set_language,
    .ml_flags = METH_O,
    .ml_doc = "Parse source code, creating a syntax tree",
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

static struct PyModuleDef module_definition = {
  .m_base = PyModuleDef_HEAD_INIT,
  .m_name = "tree_sitter",
  .m_doc = NULL,
  .m_size = -1,
};

PyMODINIT_FUNC PyInit_tree_sitter_binding(void) {
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

  return module;
}
