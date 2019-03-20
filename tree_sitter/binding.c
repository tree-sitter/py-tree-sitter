#include "Python.h"
#include "tree_sitter/api.h"

// Types

typedef struct {
  PyObject_HEAD
  TSNode node;
} Node;

typedef struct {
  PyObject_HEAD
  TSTree *tree;
} Tree;

typedef struct {
  PyObject_HEAD
  TSParser *parser;
} Parser;

// Node

static PyObject *node_new(
  PyTypeObject *type,
  PyObject *args,
  PyObject *kwds
) {
  PyErr_SetString(PyExc_RuntimeError, "Illegal constructor");
  return NULL;
}

static void node_dealloc(PyObject *self) {
  Py_TYPE(self)->tp_free(self);
}

static PyObject *node_string(Node *self, PyObject *args) {
  char *string = ts_node_string(self->node);
  PyObject *result = PyUnicode_FromString(string);
  free(string);
  return result;
}

static PyObject *node_get_type(Node *self, void *payload) {
  return PyUnicode_FromString(ts_node_type(self->node));
}

static PyMethodDef node_methods[] = {
  {
    .ml_name = "sexp",
    .ml_meth = (PyCFunction)node_string,
    .ml_flags = METH_NOARGS,
    .ml_doc = "Get an S-expression representing the name",
  },
  {NULL},
};

static PyGetSetDef node_accessors[] = {
  {"type", (getter)node_get_type, NULL, "kind", NULL},
  {NULL}
};

static PyTypeObject node_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "tree_sitter.Node",
  .tp_doc = "A syntax node",
  .tp_basicsize = sizeof(Node),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = node_new,
  .tp_dealloc = node_dealloc,
  .tp_methods = node_methods,
  .tp_getset = node_accessors,
};

static PyObject *node_new_internal(TSNode node) {
  Node *self = (Node *)node_type.tp_alloc(&node_type, 0);
  if (self != NULL) self->node = node;
  return (PyObject *)self;
}

// Tree

static PyObject *tree_new(
  PyTypeObject *type,
  PyObject *args,
  PyObject *kwds
) {
  PyErr_SetString(PyExc_RuntimeError, "Illegal constructor");
  return NULL;
}

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
  .tp_new = tree_new,
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

  if (!PyUnicode_Check(source_code)) {
    PyErr_SetString(PyExc_TypeError, "First argument to parse must be a string");
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

  TSTree *new_tree = NULL;

  PyUnicode_READY(source_code);
  size_t length = PyUnicode_GET_LENGTH(source_code);
  int kind = PyUnicode_KIND(source_code);
  if (kind == PyUnicode_1BYTE_KIND) {
    Py_UCS1 *source_bytes = PyUnicode_1BYTE_DATA(source_code);
    new_tree = ts_parser_parse_string(self->parser, old_tree, (char *)source_bytes, length);
  } else if (kind == PyUnicode_2BYTE_KIND) {
    Py_UCS2 *source_bytes = PyUnicode_2BYTE_DATA(source_code);
    new_tree = ts_parser_parse_string_encoding(self->parser, old_tree, (char *)source_bytes, length, TSInputEncodingUTF16);
  } else if (kind == PyUnicode_4BYTE_KIND) {
    PyErr_SetString(PyExc_ValueError, "4 byte strings are not yet supported");
    return NULL;
  } else {
    PyErr_SetString(PyExc_ValueError, "Unknown string kind");
    return NULL;
  }

  if (!new_tree) {
    PyErr_SetString(PyExc_ValueError, "Parsing failed");
    return NULL;
  }

  return (PyObject *)tree_new_internal(new_tree);
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
