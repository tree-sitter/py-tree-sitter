#include "tree_sitter/api.h"

#include <Python.h>
#include <wctype.h>

// Types

typedef struct {
    PyObject_HEAD
    TSNode node;
    PyObject *children;
    PyObject *tree;
} Node;

typedef struct {
    PyObject_HEAD
    TSTree *tree;
    PyObject *source;
} Tree;

typedef struct {
    PyObject_HEAD
    TSParser *parser;
} Parser;

typedef struct {
    PyObject_HEAD
    TSTreeCursor cursor;
    PyObject *node;
    PyObject *tree;
} TreeCursor;

typedef struct {
    PyObject_HEAD
    uint32_t capture1_value_id;
    uint32_t capture2_value_id;
    int is_positive;
} CaptureEqCapture;

typedef struct {
    PyObject_HEAD
    uint32_t capture_value_id;
    PyObject *string_value;
    int is_positive;
} CaptureEqString;

typedef struct {
    PyObject_HEAD
    uint32_t capture_value_id;
    PyObject *regex;
    int is_positive;
} CaptureMatchString;

typedef struct {
    PyObject_HEAD
    TSQuery *query;
    PyObject *capture_names;
    PyObject *text_predicates;
} Query;

typedef struct {
    PyObject_HEAD
    TSQueryCapture capture;
} QueryCapture;

typedef struct {
    PyObject_HEAD
    TSQueryMatch match;
    PyObject *captures;
    PyObject *pattern_index;
} QueryMatch;

typedef struct {
    PyObject_HEAD
    TSRange range;
} Range;

typedef struct {
    PyObject_HEAD
    TSLookaheadIterator *lookahead_iterator;
} LookaheadIterator;

typedef LookaheadIterator LookaheadNamesIterator;

typedef struct {
    TSTreeCursor default_cursor;
    TSQueryCursor *query_cursor;
    PyObject *re_compile;

    PyTypeObject *tree_type;
    PyTypeObject *tree_cursor_type;
    PyTypeObject *parser_type;
    PyTypeObject *node_type;
    PyTypeObject *query_type;
    PyTypeObject *range_type;
    PyTypeObject *query_capture_type;
    PyTypeObject *query_match_type;
    PyTypeObject *capture_eq_capture_type;
    PyTypeObject *capture_eq_string_type;
    PyTypeObject *capture_match_string_type;
    PyTypeObject *lookahead_iterator_type;
    PyTypeObject *lookahead_names_iterator_type;
} ModuleState;

#if PY_VERSION_HEX < 0x030900f0
static ModuleState *global_state = NULL;
static ModuleState *PyType_GetModuleState(PyTypeObject *obj) { return global_state; }
static PyObject *PyType_FromModuleAndSpec(PyObject *module, PyType_Spec *spec, PyObject *bases) {
    return PyType_FromSpecWithBases(spec, bases);
}
#endif

// Point

static PyObject *point_new(TSPoint point) {
    PyObject *row = PyLong_FromSize_t((size_t)point.row);
    PyObject *column = PyLong_FromSize_t((size_t)point.column);
    if (!row || !column) {
        Py_XDECREF(row);
        Py_XDECREF(column);
        return NULL;
    }

    PyObject *obj = PyTuple_Pack(2, row, column);
    Py_XDECREF(row);
    Py_XDECREF(column);
    return obj;
}

// Node

static PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);
static PyObject *tree_cursor_new_internal(ModuleState *state, TSNode node, PyObject *tree);
static PyObject *range_new_internal(ModuleState *state, TSRange range);
static PyObject *lookahead_iterator_new_internal(ModuleState *state,
                                                 TSLookaheadIterator *lookahead_iterator);
static PyObject *lookahead_names_iterator_new_internal(ModuleState *state,
                                                       TSLookaheadIterator *lookahead_iterator);

static void node_dealloc(Node *self) {
    Py_XDECREF(self->children);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *node_repr(Node *self) {
    const char *type = ts_node_type(self->node);
    TSPoint start_point = ts_node_start_point(self->node);
    TSPoint end_point = ts_node_end_point(self->node);
    const char *format_string =
        ts_node_is_named(self->node)
            ? "<Node type=%s, start_point=(%u, %u), end_point=(%u, %u)>"
            : "<Node type=\"%s\", start_point=(%u, %u), end_point=(%u, %u)>";
    return PyUnicode_FromFormat(format_string, type, start_point.row, start_point.column,
                                end_point.row, end_point.column);
}

static bool node_is_instance(ModuleState *state, PyObject *self);

static PyObject *node_compare(Node *self, Node *other, int op) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    if (node_is_instance(state, (PyObject *)other)) {
        bool result = ts_node_eq(self->node, other->node);
        switch (op) {
        case Py_EQ:
            return PyBool_FromLong(result);
        case Py_NE:
            return PyBool_FromLong(!result);
        default:
            Py_RETURN_FALSE;
        }
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *node_sexp(Node *self, PyObject *args) {
    char *string = ts_node_string(self->node);
    PyObject *result = PyUnicode_FromString(string);
    free(string);
    return result;
}

static PyObject *node_walk(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return tree_cursor_new_internal(state, self->node, self->tree);
}

static PyObject *node_edit(Node *self, PyObject *args, PyObject *kwargs) {
    unsigned start_byte, start_row, start_column;
    unsigned old_end_byte, old_end_row, old_end_column;
    unsigned new_end_byte, new_end_row, new_end_column;

    char *keywords[] = {
        "start_byte",    "old_end_byte",  "new_end_byte", "start_point",
        "old_end_point", "new_end_point", NULL,
    };

    int ok = PyArg_ParseTupleAndKeywords(
        args, kwargs, "III(II)(II)(II)", keywords, &start_byte, &old_end_byte, &new_end_byte,
        &start_row, &start_column, &old_end_row, &old_end_column, &new_end_row, &new_end_column);

    if (!ok) {
        Py_RETURN_NONE;
    }

    TSInputEdit edit = {
        .start_byte = start_byte,
        .old_end_byte = old_end_byte,
        .new_end_byte = new_end_byte,
        .start_point =
            {
                .row = start_row,
                .column = start_column,
            },
        .old_end_point =
            {
                .row = old_end_row,
                .column = old_end_column,
            },
        .new_end_point =
            {
                .row = new_end_row,
                .column = new_end_column,
            },
    };

    ts_node_edit(&self->node, &edit);

    Py_RETURN_NONE;
}

static PyObject *node_child(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    long index;
    if (!PyArg_ParseTuple(args, "l", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "Index must be positive");
        return NULL;
    }

    TSNode child = ts_node_child(self->node, (uint32_t)index);
    return node_new_internal(state, child, self->tree);
}

static PyObject *node_named_child(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    long index;
    if (!PyArg_ParseTuple(args, "l", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "Index must be positive");
        return NULL;
    }

    TSNode child = ts_node_named_child(self->node, (uint32_t)index);
    return node_new_internal(state, child, self->tree);
}

static PyObject *node_child_by_field_id(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSFieldId field_id;
    if (!PyArg_ParseTuple(args, "H", &field_id)) {
        return NULL;
    }
    TSNode child = ts_node_child_by_field_id(self->node, field_id);
    if (ts_node_is_null(child)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, child, self->tree);
}

static PyObject *node_child_by_field_name(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    char *name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#", &name, &length)) {
        return NULL;
    }
    TSNode child = ts_node_child_by_field_name(self->node, name, length);
    if (ts_node_is_null(child)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, child, self->tree);
}

static PyObject *node_children_by_field_id_internal(Node *self, TSFieldId field_id) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *result = PyList_New(0);

    if (field_id == 0) {
        return result;
    }

    ts_tree_cursor_reset(&state->default_cursor, self->node);
    int ok = ts_tree_cursor_goto_first_child(&state->default_cursor);
    while (ok) {
        if (ts_tree_cursor_current_field_id(&state->default_cursor) == field_id) {
            TSNode tsnode = ts_tree_cursor_current_node(&state->default_cursor);
            PyObject *node = node_new_internal(state, tsnode, self->tree);
            PyList_Append(result, node);
            Py_XDECREF(node);
        }
        ok = ts_tree_cursor_goto_next_sibling(&state->default_cursor);
    }

    return result;
}

static PyObject *node_children_by_field_id(Node *self, PyObject *args) {
    TSFieldId field_id;
    if (!PyArg_ParseTuple(args, "H", &field_id)) {
        return NULL;
    }

    return node_children_by_field_id_internal(self, field_id);
}

static PyObject *node_children_by_field_name(Node *self, PyObject *args) {
    char *name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#", &name, &length)) {
        return NULL;
    }

    const TSLanguage *lang = ts_tree_language(((Tree *)self->tree)->tree);
    TSFieldId field_id = ts_language_field_id_for_name(lang, name, length);
    return node_children_by_field_id_internal(self, field_id);
}

static PyObject *node_field_name_for_child(Node *self, PyObject *args) {
    uint32_t index;
    if (!PyArg_ParseTuple(args, "I", &index)) {
        return NULL;
    }

    const char *field_name = ts_node_field_name_for_child(self->node, index);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromString(field_name);
}

static PyObject *node_descendant_for_byte_range(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "II", &start_byte, &end_byte)) {
        return NULL;
    }
    TSNode descendant = ts_node_descendant_for_byte_range(self->node, start_byte, end_byte);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

static PyObject *node_named_descendant_for_byte_range(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "II", &start_byte, &end_byte)) {
        return NULL;
    }
    TSNode descendant = ts_node_named_descendant_for_byte_range(self->node, start_byte, end_byte);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

static PyObject *node_descendant_for_point_range(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));

    if (!PyTuple_Check(args) || PyTuple_Size(args) != 2) {
        PyErr_SetString(PyExc_TypeError, "Expected two tuples as arguments");
        return NULL;
    }

    PyObject *start_point = PyTuple_GetItem(args, 0);
    PyObject *end_point = PyTuple_GetItem(args, 1);

    if (!PyTuple_Check(start_point) || !PyTuple_Check(end_point)) {
        PyErr_SetString(PyExc_TypeError, "Both start_point and end_point must be tuples");
        return NULL;
    }

    TSPoint start, end;
    if (!PyArg_ParseTuple(start_point, "ii", &start.row, &start.column)) {
        return NULL;
    }
    if (!PyArg_ParseTuple(end_point, "ii", &end.row, &end.column)) {
        return NULL;
    }

    TSNode descendant = ts_node_descendant_for_point_range(self->node, start, end);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

static PyObject *node_named_descendant_for_point_range(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));

    if (!PyTuple_Check(args) || PyTuple_Size(args) != 2) {
        PyErr_SetString(PyExc_TypeError, "Expected two tuples as arguments");
        return NULL;
    }

    PyObject *start_point = PyTuple_GetItem(args, 0);
    PyObject *end_point = PyTuple_GetItem(args, 1);

    if (!PyTuple_Check(start_point) || !PyTuple_Check(end_point)) {
        PyErr_SetString(PyExc_TypeError, "Both start_point and end_point must be tuples");
        return NULL;
    }

    TSPoint start, end;
    if (!PyArg_ParseTuple(start_point, "ii", &start.row, &start.column)) {
        return NULL;
    }
    if (!PyArg_ParseTuple(end_point, "ii", &end.row, &end.column)) {
        return NULL;
    }

    TSNode descendant = ts_node_named_descendant_for_point_range(self->node, start, end);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

static PyObject *node_get_id(Node *self, void *payload) {
    return PyLong_FromVoidPtr((void *)self->node.id);
}

static PyObject *node_get_kind_id(Node *self, void *payload) {
    TSSymbol kind_id = ts_node_symbol(self->node);
    return PyLong_FromLong(kind_id);
}

static PyObject *node_get_grammar_id(Node *self, void *payload) {
    TSSymbol grammar_id = ts_node_grammar_symbol(self->node);
    return PyLong_FromLong(grammar_id);
}

static PyObject *node_get_type(Node *self, void *payload) {
    return PyUnicode_FromString(ts_node_type(self->node));
}

static PyObject *node_get_grammar_name(Node *self, void *payload) {
    return PyUnicode_FromString(ts_node_grammar_type(self->node));
}

static PyObject *node_get_is_named(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_named(self->node));
}

static PyObject *node_get_is_extra(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_extra(self->node));
}

static PyObject *node_get_has_changes(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_has_changes(self->node));
}

static PyObject *node_get_has_error(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_has_error(self->node));
}

static PyObject *node_get_is_error(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_error(self->node));
}

static PyObject *node_get_parse_state(Node *self, void *payload) {
    return PyLong_FromLong(ts_node_parse_state(self->node));
}

static PyObject *node_get_next_parse_state(Node *self, void *payload) {
    return PyLong_FromLong(ts_node_next_parse_state(self->node));
}

static PyObject *node_get_is_missing(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_missing(self->node));
}

static PyObject *node_get_start_byte(Node *self, void *payload) {
    return PyLong_FromSize_t((size_t)ts_node_start_byte(self->node));
}

static PyObject *node_get_end_byte(Node *self, void *payload) {
    return PyLong_FromSize_t((size_t)ts_node_end_byte(self->node));
}

static PyObject *node_get_byte_range(Node *self, void *payload) {
    PyObject *start_byte = PyLong_FromSize_t((size_t)ts_node_start_byte(self->node));
    if (start_byte == NULL) {
        return NULL;
    }
    PyObject *end_byte = PyLong_FromSize_t((size_t)ts_node_end_byte(self->node));
    if (end_byte == NULL) {
        Py_DECREF(start_byte);
        return NULL;
    }
    PyObject *result = PyTuple_Pack(2, start_byte, end_byte);
    Py_DECREF(start_byte);
    Py_DECREF(end_byte);
    return result;
}

static PyObject *node_get_range(Node *self, void *payload) {
    uint32_t start_byte = ts_node_start_byte(self->node);
    uint32_t end_byte = ts_node_end_byte(self->node);
    TSPoint start_point = ts_node_start_point(self->node);
    TSPoint end_point = ts_node_end_point(self->node);
    TSRange range = {
        .start_byte = start_byte,
        .end_byte = end_byte,
        .start_point = start_point,
        .end_point = end_point,
    };
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return range_new_internal(state, range);
}

static PyObject *node_get_start_point(Node *self, void *payload) {
    return point_new(ts_node_start_point(self->node));
}

static PyObject *node_get_end_point(Node *self, void *payload) {
    return point_new(ts_node_end_point(self->node));
}

static PyObject *node_get_children(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    if (self->children) {
        Py_INCREF(self->children);
        return self->children;
    }

    long length = (long)ts_node_child_count(self->node);
    PyObject *result = PyList_New(length);
    if (result == NULL) {
        return NULL;
    }
    if (length > 0) {
        ts_tree_cursor_reset(&state->default_cursor, self->node);
        ts_tree_cursor_goto_first_child(&state->default_cursor);
        int i = 0;
        do {
            TSNode child = ts_tree_cursor_current_node(&state->default_cursor);
            if (PyList_SetItem(result, i, node_new_internal(state, child, self->tree))) {
                Py_DECREF(result);
                return NULL;
            }
            i++;
        } while (ts_tree_cursor_goto_next_sibling(&state->default_cursor));
    }
    Py_INCREF(result);
    self->children = result;
    return result;
}

static PyObject *node_get_named_children(Node *self, void *payload) {
    PyObject *children = node_get_children(self, payload);
    if (children == NULL) {
        return NULL;
    }
    // children is retained by self->children
    Py_DECREF(children);

    long named_count = (long)ts_node_named_child_count(self->node);
    PyObject *result = PyList_New(named_count);
    if (result == NULL) {
        return NULL;
    }

    long length = (long)ts_node_child_count(self->node);
    int j = 0;
    for (int i = 0; i < length; i++) {
        PyObject *child = PyList_GetItem(self->children, i);
        if (ts_node_is_named(((Node *)child)->node)) {
            Py_INCREF(child);
            if (PyList_SetItem(result, j++, child)) {
                Py_DECREF(result);
                return NULL;
            }
        }
    }
    return result;
}

static PyObject *node_get_child_count(Node *self, void *payload) {
    long length = (long)ts_node_child_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

static PyObject *node_get_named_child_count(Node *self, void *payload) {
    long length = (long)ts_node_named_child_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

static PyObject *node_get_parent(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode parent = ts_node_parent(self->node);
    if (ts_node_is_null(parent)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, parent, self->tree);
}

static PyObject *node_get_next_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_sibling = ts_node_next_sibling(self->node);
    if (ts_node_is_null(next_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_sibling, self->tree);
}

static PyObject *node_get_prev_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_sibling = ts_node_prev_sibling(self->node);
    if (ts_node_is_null(prev_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_sibling, self->tree);
}

static PyObject *node_get_next_named_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_named_sibling = ts_node_next_named_sibling(self->node);
    if (ts_node_is_null(next_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_named_sibling, self->tree);
}

static PyObject *node_get_prev_named_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_named_sibling = ts_node_prev_named_sibling(self->node);
    if (ts_node_is_null(prev_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_named_sibling, self->tree);
}

static PyObject *node_get_descendant_count(Node *self, void *payload) {
    long length = (long)ts_node_descendant_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

static PyObject *node_get_text(Node *self, void *payload) {
    Tree *tree = (Tree *)self->tree;
    if (tree == NULL) {
        PyErr_SetString(PyExc_ValueError, "No tree");
        return NULL;
    }
    if (tree->source == Py_None || tree->source == NULL) {
        Py_RETURN_NONE;
    }

    PyObject *start_byte = PyLong_FromSize_t((size_t)ts_node_start_byte(self->node));
    if (start_byte == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to determine start byte");
        return NULL;
    }
    PyObject *end_byte = PyLong_FromSize_t((size_t)ts_node_end_byte(self->node));
    if (end_byte == NULL) {
        Py_DECREF(start_byte);
        PyErr_SetString(PyExc_RuntimeError, "Failed to determine end byte");
        return NULL;
    }
    PyObject *slice = PySlice_New(start_byte, end_byte, NULL);
    Py_DECREF(start_byte);
    Py_DECREF(end_byte);
    if (slice == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "PySlice_New failed");
        return NULL;
    }
    PyObject *node_mv = PyMemoryView_FromObject(tree->source);
    if (node_mv == NULL) {
        Py_DECREF(slice);
        PyErr_SetString(PyExc_RuntimeError, "PyMemoryView_FromObject failed");
        return NULL;
    }
    PyObject *node_slice = PyObject_GetItem(node_mv, slice);
    Py_DECREF(slice);
    Py_DECREF(node_mv);
    if (node_slice == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "PyObject_GetItem failed");
        return NULL;
    }
    PyObject *result = PyBytes_FromObject(node_slice);
    Py_DECREF(node_slice);
    return result;
}

static Py_hash_t node_hash(Node *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));

    // __eq__ and __hash__ must be compatible. As __eq__ is defined by
    // ts_node_eq, which in turn checks the tree pointer and the node
    // id, we can use those values to compute the hash.
    Py_hash_t tree_hash = _Py_HashPointer(self->node.tree);
    Py_hash_t id_hash = (Py_hash_t)(self->node.id);

    return tree_hash ^ id_hash;
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
        .ml_name = "edit",
        .ml_meth = (PyCFunction)node_edit,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc =
            "edit(start_byte, old_end_byte, new_end_byte, start_point, old_end_point, new_end_point)\n--\n\n\
			   Edit this node to keep it in-sync with source code that has been edited.",
    },
    {
        .ml_name = "sexp",
        .ml_meth = (PyCFunction)node_sexp,
        .ml_flags = METH_NOARGS,
        .ml_doc = "sexp()\n--\n\n\
               Get an S-expression representing the node.",
    },
    {
        .ml_name = "child",
        .ml_meth = (PyCFunction)node_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = "child(index)\n--\n\n\
			   Get child at the given index.",
    },
    {
        .ml_name = "named_child",
        .ml_meth = (PyCFunction)node_named_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = "named_child(index)\n--\n\n\
			   Get named child by index.",
    },
    {
        .ml_name = "child_by_field_id",
        .ml_meth = (PyCFunction)node_child_by_field_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = "child_by_field_id(id)\n--\n\n\
               Get child for the given field id.",
    },
    {
        .ml_name = "child_by_field_name",
        .ml_meth = (PyCFunction)node_child_by_field_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "child_by_field_name(name)\n--\n\n\
               Get child for the given field name.",
    },
    {
        .ml_name = "children_by_field_id",
        .ml_meth = (PyCFunction)node_children_by_field_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = "children_by_field_id(id)\n--\n\n\
               Get a list of child nodes for the given field id.",
    },
    {
        .ml_name = "children_by_field_name",
        .ml_meth = (PyCFunction)node_children_by_field_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "children_by_field_name(name)\n--\n\n\
               Get a list of child nodes for the given field name.",
    },
    {.ml_name = "field_name_for_child",
     .ml_meth = (PyCFunction)node_field_name_for_child,
     .ml_flags = METH_VARARGS,
     .ml_doc = "field_name_for_child(index)\n-\n\n\
               Get the field name of a child node by the index of child."},
    {
        .ml_name = "descendant_for_byte_range",
        .ml_meth = (PyCFunction)node_descendant_for_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = "descendant_for_byte_range(start_byte, end_byte)\n--\n\n\
			   Get the smallest node within this node that spans the given byte range.",
    },
    {
        .ml_name = "named_descendant_for_byte_range",
        .ml_meth = (PyCFunction)node_named_descendant_for_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = "named_descendant_for_byte_range(start_byte, end_byte)\n--\n\n\
			   Get the smallest named node within this node that spans the given byte range.",
    },
    {
        .ml_name = "descendant_for_point_range",
        .ml_meth = (PyCFunction)node_descendant_for_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = "descendant_for_point_range(start_point, end_point)\n--\n\n\
			   Get the smallest node within this node that spans the given point range.",
    },
    {
        .ml_name = "named_descendant_for_point_range",
        .ml_meth = (PyCFunction)node_named_descendant_for_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = "named_descendant_for_point_range(start_point, end_point)\n--\n\n\
			   Get the smallest named node within this node that spans the given point range.",
    },
    {NULL},
};

static PyGetSetDef node_accessors[] = {
    {"id", (getter)node_get_id, NULL, "The node's numeric id", NULL},
    {"kind_id", (getter)node_get_kind_id, NULL, "The node's type as a numerical id", NULL},
    {"grammar_id", (getter)node_get_grammar_id, NULL, "The node's grammar type as a numerical id",
     NULL},
    {"grammar_name", (getter)node_get_grammar_name, NULL, "The node's grammar name as a string",
     NULL},
    {"type", (getter)node_get_type, NULL, "The node's type", NULL},
    {"is_named", (getter)node_get_is_named, NULL, "Is this a named node", NULL},
    {"is_extra", (getter)node_get_is_extra, NULL, "Is this an extra node", NULL},
    {"has_changes", (getter)node_get_has_changes, NULL,
     "Does this node have text changes since it was parsed", NULL},
    {"has_error", (getter)node_get_has_error, NULL, "Does this node contain any errors", NULL},
    {"is_error", (getter)node_get_is_error, NULL, "Is this node an error", NULL},
    {"parse_state", (getter)node_get_parse_state, NULL, "The node's parse state", NULL},
    {"next_parse_state", (getter)node_get_next_parse_state, NULL,
     "The parse state after this node's", NULL},
    {"is_missing", (getter)node_get_is_missing, NULL, "Is this a node inserted by the parser",
     NULL},
    {"start_byte", (getter)node_get_start_byte, NULL, "The node's start byte", NULL},
    {"end_byte", (getter)node_get_end_byte, NULL, "The node's end byte", NULL},
    {"byte_range", (getter)node_get_byte_range, NULL, "The node's byte range", NULL},
    {"range", (getter)node_get_range, NULL, "The node's range", NULL},
    {"start_point", (getter)node_get_start_point, NULL, "The node's start point", NULL},
    {"end_point", (getter)node_get_end_point, NULL, "The node's end point", NULL},
    {"children", (getter)node_get_children, NULL, "The node's children", NULL},
    {"child_count", (getter)node_get_child_count, NULL, "The number of children for a node", NULL},
    {"named_children", (getter)node_get_named_children, NULL, "The node's named children", NULL},
    {"named_child_count", (getter)node_get_named_child_count, NULL,
     "The number of named children for a node", NULL},
    {"parent", (getter)node_get_parent, NULL, "The node's parent", NULL},
    {"next_sibling", (getter)node_get_next_sibling, NULL, "The node's next sibling", NULL},
    {"prev_sibling", (getter)node_get_prev_sibling, NULL, "The node's previous sibling", NULL},
    {"next_named_sibling", (getter)node_get_next_named_sibling, NULL,
     "The node's next named sibling", NULL},
    {"prev_named_sibling", (getter)node_get_prev_named_sibling, NULL,
     "The node's previous named sibling", NULL},
    {"descendant_count", (getter)node_get_descendant_count, NULL,
     "The number of descendants for a node, including itself", NULL},
    {"text", (getter)node_get_text, NULL, "The node's text, if tree has not been edited", NULL},
    {NULL},
};

static PyType_Slot node_type_slots[] = {
    {Py_tp_doc, "A syntax node"},   {Py_tp_dealloc, node_dealloc},
    {Py_tp_repr, node_repr},        {Py_tp_richcompare, node_compare},
    {Py_tp_hash, node_hash},        {Py_tp_methods, node_methods},
    {Py_tp_getset, node_accessors}, {0, NULL},
};

static PyType_Spec node_type_spec = {
    .name = "tree_sitter.Node",
    .basicsize = sizeof(Node),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = node_type_slots,
};

static PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree) {
    Node *self = (Node *)state->node_type->tp_alloc(state->node_type, 0);
    if (self != NULL) {
        self->node = node;
        Py_INCREF(tree);
        self->tree = tree;
        self->children = NULL;
    }
    return (PyObject *)self;
}

static bool node_is_instance(ModuleState *state, PyObject *self) {
    return PyObject_IsInstance(self, (PyObject *)state->node_type);
}

// Tree

static void tree_dealloc(Tree *self) {
    ts_tree_delete(self->tree);
    Py_XDECREF(self->source);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *tree_get_root_node(Tree *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return node_new_internal(state, ts_tree_root_node(self->tree), (PyObject *)self);
}

static PyObject *tree_get_text(Tree *self, void *payload) {
    PyObject *source = self->source;
    if (source == NULL) {
        Py_RETURN_NONE;
    }
    Py_INCREF(source);
    return source;
}

static PyObject *tree_root_node_with_offset(Tree *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));

    unsigned offset_bytes;
    TSPoint offset_extent;

    if (!PyArg_ParseTuple(args, "I(ii)", &offset_bytes, &offset_extent.row,
                          &offset_extent.column)) {
        return NULL;
    }

    TSNode node = ts_tree_root_node_with_offset(self->tree, offset_bytes, offset_extent);
    return node_new_internal(state, node, (PyObject *)self);
}

static PyObject *tree_walk(Tree *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return tree_cursor_new_internal(state, ts_tree_root_node(self->tree), (PyObject *)self);
}

static PyObject *tree_edit(Tree *self, PyObject *args, PyObject *kwargs) {
    unsigned start_byte, start_row, start_column;
    unsigned old_end_byte, old_end_row, old_end_column;
    unsigned new_end_byte, new_end_row, new_end_column;

    char *keywords[] = {
        "start_byte",    "old_end_byte",  "new_end_byte", "start_point",
        "old_end_point", "new_end_point", NULL,
    };

    int ok = PyArg_ParseTupleAndKeywords(
        args, kwargs, "III(II)(II)(II)", keywords, &start_byte, &old_end_byte, &new_end_byte,
        &start_row, &start_column, &old_end_row, &old_end_column, &new_end_row, &new_end_column);

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
        Py_XDECREF(self->source);
        self->source = Py_None;
        Py_INCREF(self->source);
    }
    Py_RETURN_NONE;
}

static PyObject *tree_changed_ranges(Tree *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    Tree *new_tree = NULL;
    char *keywords[] = {"new_tree", NULL};
    int ok = PyArg_ParseTupleAndKeywords(args, kwargs, "O", keywords, (PyObject **)&new_tree);
    if (!ok) {
        return NULL;
    }

    if (!PyObject_IsInstance((PyObject *)new_tree, (PyObject *)state->tree_type)) {
        PyErr_SetString(PyExc_TypeError, "First argument to get_changed_ranges must be a Tree");
        return NULL;
    }

    uint32_t length = 0;
    TSRange *ranges = ts_tree_get_changed_ranges(self->tree, new_tree->tree, &length);

    PyObject *result = PyList_New(length);
    if (!result) {
        return NULL;
    }
    for (unsigned i = 0; i < length; i++) {
        PyObject *range = range_new_internal(state, ranges[i]);
        PyList_SetItem(result, i, range);
    }

    free(ranges);
    return result;
}

static PyObject *tree_get_included_ranges(Tree *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    uint32_t length = 0;
    TSRange *ranges = ts_tree_included_ranges(self->tree, &length);

    PyObject *result = PyList_New(length);
    if (!result) {
        return NULL;
    }
    for (unsigned i = 0; i < length; i++) {
        PyObject *range = range_new_internal(state, ranges[i]);
        PyList_SetItem(result, i, range);
    }

    free(ranges);
    return result;
}

static PyMethodDef tree_methods[] = {
    {
        .ml_name = "root_node_with_offset",
        .ml_meth = (PyCFunction)tree_root_node_with_offset,
        .ml_flags = METH_VARARGS,
        .ml_doc = "root_node_with_offset(offset_bytes, offset_extent)\n--\n\n\
			   Get the root node of the syntax tree, but with its position shifted forward by the given offset.",
    },
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
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = "edit(start_byte, old_end_byte, new_end_byte,\
               start_point, old_end_point, new_end_point)\n--\n\n\
               Edit the syntax tree.",
    },
    {
        .ml_name = "changed_ranges",
        .ml_meth = (PyCFunction)tree_changed_ranges,
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = "changed_ranges(new_tree)\n--\n\n\
               Compare old edited tree to new tree and return changed ranges.",
    },
    {NULL},
};

static PyGetSetDef tree_accessors[] = {
    {"root_node", (getter)tree_get_root_node, NULL, "The root node of this tree.", NULL},
    {"text", (getter)tree_get_text, NULL, "The source text for this tree, if unedited.", NULL},
    {"included_ranges", (getter)tree_get_included_ranges, NULL,
     "Get the included ranges that were used to parse the syntax tree.", NULL},
    {NULL},
};

static PyType_Slot tree_type_slots[] = {
    {Py_tp_doc, "A syntax tree"},
    {Py_tp_dealloc, tree_dealloc},
    {Py_tp_methods, tree_methods},
    {Py_tp_getset, tree_accessors},
    {0, NULL},
};

static PyType_Spec tree_type_spec = {
    .name = "tree_sitter.Tree",
    .basicsize = sizeof(Tree),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = tree_type_slots,
};

static PyObject *tree_new_internal(ModuleState *state, TSTree *tree, PyObject *source,
                                   int keep_text) {
    Tree *self = (Tree *)state->tree_type->tp_alloc(state->tree_type, 0);
    if (self != NULL) {
        self->tree = tree;
    }

    if (keep_text) {
        self->source = source;
    } else {
        self->source = Py_None;
    }
    Py_INCREF(self->source);
    return (PyObject *)self;
}

// TreeCursor

static void tree_cursor_dealloc(TreeCursor *self) {
    ts_tree_cursor_delete(&self->cursor);
    Py_XDECREF(self->node);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *tree_cursor_get_node(TreeCursor *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    if (!self->node) {
        self->node =
            node_new_internal(state, ts_tree_cursor_current_node(&self->cursor), self->tree);
    }

    Py_INCREF(self->node);
    return self->node;
}

static PyObject *tree_cursor_current_field_id(TreeCursor *self, PyObject *args) {
    uint32_t field_id = ts_tree_cursor_current_field_id(&self->cursor);
    if (field_id == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(field_id);
}

static PyObject *tree_cursor_current_field_name(TreeCursor *self, PyObject *args) {
    const char *field_name = ts_tree_cursor_current_field_name(&self->cursor);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

static PyObject *tree_cursor_current_depth(TreeCursor *self, PyObject *args) {
    uint32_t depth = ts_tree_cursor_current_depth(&self->cursor);
    return PyLong_FromUnsignedLong(depth);
}

static PyObject *tree_cursor_current_descendant_index(TreeCursor *self, PyObject *args) {
    uint32_t index = ts_tree_cursor_current_descendant_index(&self->cursor);
    return PyLong_FromUnsignedLong(index);
}

static PyObject *tree_cursor_goto_first_child(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_first_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_last_child(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_last_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_parent(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_parent(&self->cursor);
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

static PyObject *tree_cursor_goto_previous_sibling(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_previous_sibling(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_descendant(TreeCursor *self, PyObject *args) {
    uint32_t index;
    if (!PyArg_ParseTuple(args, "I", &index)) {
        return NULL;
    }
    ts_tree_cursor_goto_descendant(&self->cursor, index);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

static PyObject *tree_cursor_goto_first_child_for_byte(TreeCursor *self, PyObject *args) {
    uint32_t byte;
    if (!PyArg_ParseTuple(args, "I", &byte)) {
        return NULL;
    }
    bool result = ts_tree_cursor_goto_first_child_for_byte(&self->cursor, byte);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

static PyObject *tree_cursor_goto_first_child_for_point(TreeCursor *self, PyObject *args) {
    uint32_t row, column;
    if (!PyArg_ParseTuple(args, "II", &row, &column)) {
        return NULL;
    }
    bool result = ts_tree_cursor_goto_first_child_for_point(&self->cursor, (TSPoint){row, column});
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

static PyObject *tree_cursor_reset(TreeCursor *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *node_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &node_obj)) {
        return NULL;
    }
    if (!PyObject_IsInstance(node_obj, (PyObject *)state->node_type)) {
        PyErr_SetString(PyExc_TypeError, "First argument to reset must be a Node");
        return NULL;
    }
    Node *node = (Node *)node_obj;
    ts_tree_cursor_reset(&self->cursor, node->node);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

// Reset to another cursor
static PyObject *tree_cursor_reset_to(TreeCursor *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *cursor_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &cursor_obj)) {
        return NULL;
    }
    if (!PyObject_IsInstance(cursor_obj, (PyObject *)state->tree_cursor_type)) {
        PyErr_SetString(PyExc_TypeError, "First argument to reset_to must be a TreeCursor");
        return NULL;
    }
    TreeCursor *cursor = (TreeCursor *)cursor_obj;
    ts_tree_cursor_reset_to(&self->cursor, &cursor->cursor);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

static PyObject *tree_cursor_copy(PyObject *self);

static PyMethodDef tree_cursor_methods[] = {
    {
        .ml_name = "descendant_index",
        .ml_meth = (PyCFunction)tree_cursor_current_descendant_index,
        .ml_flags = METH_NOARGS,
        .ml_doc = "current_descendant_index()\n--\n\n\
			   Get the index of the cursor's current node out of all of the descendants of the original node.",
    },
    {
        .ml_name = "goto_first_child",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child,
        .ml_flags = METH_NOARGS,
        .ml_doc = "goto_first_child()\n--\n\n\
               Go to the first child.\n\n\
               If the current node has children, move to the first child and\n\
               return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_last_child",
        .ml_meth = (PyCFunction)tree_cursor_goto_last_child,
        .ml_flags = METH_NOARGS,
        .ml_doc = "goto_last_child()\n--\n\n\
			   Go to the last child.\n\n\
			   If the current node has children, move to the last child and\n\
			   return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_parent",
        .ml_meth = (PyCFunction)tree_cursor_goto_parent,
        .ml_flags = METH_NOARGS,
        .ml_doc = "goto_parent()\n--\n\n\
               Go to the parent.\n\n\
               If the current node is not the root, move to its parent and\n\
               return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_next_sibling",
        .ml_meth = (PyCFunction)tree_cursor_goto_next_sibling,
        .ml_flags = METH_NOARGS,
        .ml_doc = "goto_next_sibling()\n--\n\n\
               Go to the next sibling.\n\n\
               If the current node has a next sibling, move to the next sibling\n\
               and return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_previous_sibling",
        .ml_meth = (PyCFunction)tree_cursor_goto_previous_sibling,
        .ml_flags = METH_NOARGS,
        .ml_doc = "goto_previous_sibling()\n--\n\n\
			   Go to the previous sibling.\n\n\
			   If the current node has a previous sibling, move to the previous sibling\n\
			   and return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_descendant",
        .ml_meth = (PyCFunction)tree_cursor_goto_descendant,
        .ml_flags = METH_VARARGS,
        .ml_doc = "goto_descendant(index)\n--\n\n\
			   Go to the descendant at the given index.\n\n\
			   If the current node has a descendant at the given index, move to the\n\
			   descendant and return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_first_child_for_byte",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child_for_byte,
        .ml_flags = METH_VARARGS,
        .ml_doc = "goto_first_child_for_byte(byte)\n--\n\n\
			   Go to the first child that extends beyond the given byte.\n\n\
			   If the current node has a child that includes the given byte, move to the\n\
			   child and return True. Otherwise, return False.",
    },
    {
        .ml_name = "goto_first_child_for_point",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child_for_point,
        .ml_flags = METH_VARARGS,
        .ml_doc = "goto_first_child_for_point(row, column)\n--\n\n\
			   Go to the first child that extends beyond the given point.\n\n\
			   If the current node has a child that includes the given point, move to the\n\
			   child and return True. Otherwise, return False.",
    },
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)tree_cursor_reset,
        .ml_flags = METH_VARARGS,
        .ml_doc = "reset(node)\n--\n\n\
			   Re-initialize a tree cursor to start at a different node.",
    },
    {
        .ml_name = "reset_to",
        .ml_meth = (PyCFunction)tree_cursor_reset_to,
        .ml_flags = METH_VARARGS,
        .ml_doc = "reset_to(cursor)\n--\n\n\
			   Re-initialize the cursor to the same position as the given cursor.\n\n\
			   Unlike `reset`, this will not lose parent information and allows reusing already created cursors\n`",
    },
    {
        .ml_name = "copy",
        .ml_meth = (PyCFunction)tree_cursor_copy,
        .ml_flags = METH_NOARGS,
        .ml_doc = "copy()\n--\n\n\
               Create an independent copy of the cursor.\n",
    },
    {NULL},
};

static PyGetSetDef tree_cursor_accessors[] = {
    {"node", (getter)tree_cursor_get_node, NULL, "The current node.", NULL},
    {
        "field_id",
        (getter)tree_cursor_current_field_id,
        NULL,
        "current_field_id()\n--\n\n\
			   Get the field id of the tree cursor's current node.\n\n\
			   If the current node has the field id, return int. Otherwise, return None.",
        NULL,
    },
    {
        "field_name",
        (getter)tree_cursor_current_field_name,
        NULL,
        "current_field_name()\n--\n\n\
               Get the field name of the tree cursor's current node.\n\n\
               If the current node has the field name, return str. Otherwise, return None.",
        NULL,
    },
    {
        "depth",
        (getter)tree_cursor_current_depth,
        NULL,
        "current_depth()\n--\n\n\
			   Get the depth of the cursor's current node relative to the original node.",
        NULL,
    },
    {NULL},
};

static PyType_Slot tree_cursor_type_slots[] = {
    {Py_tp_doc, "A syntax tree cursor"},
    {Py_tp_dealloc, tree_cursor_dealloc},
    {Py_tp_methods, tree_cursor_methods},
    {Py_tp_getset, tree_cursor_accessors},
    {0, NULL},
};

static PyType_Spec tree_cursor_type_spec = {
    .name = "tree_sitter.TreeCursor",
    .basicsize = sizeof(TreeCursor),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = tree_cursor_type_slots,
};

static PyObject *tree_cursor_new_internal(ModuleState *state, TSNode node, PyObject *tree) {
    TreeCursor *self = (TreeCursor *)state->tree_cursor_type->tp_alloc(state->tree_cursor_type, 0);
    if (self != NULL) {
        self->cursor = ts_tree_cursor_new(node);
        Py_INCREF(tree);
        self->tree = tree;
    }
    return (PyObject *)self;
}

static PyObject *tree_cursor_copy(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TreeCursor *origin = (TreeCursor *)self;
    PyObject *tree = origin->tree;
    TreeCursor *copied =
        (TreeCursor *)state->tree_cursor_type->tp_alloc(state->tree_cursor_type, 0);
    if (copied != NULL) {
        copied->cursor = ts_tree_cursor_copy(&origin->cursor);
        Py_INCREF(tree);
        copied->tree = tree;
    }
    return (PyObject *)copied;
}

// Parser

static PyObject *parser_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    Parser *self = (Parser *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->parser = ts_parser_new();
    }
    return (PyObject *)self;
}

static void parser_dealloc(Parser *self) {
    ts_parser_delete(self->parser);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

typedef struct {
    PyObject *read_cb;
    PyObject *previous_return_value;
} ReadWrapperPayload;

static const char *parser_read_wrapper(void *payload, uint32_t byte_offset, TSPoint position,
                                       uint32_t *bytes_read) {
    ReadWrapperPayload *wrapper_payload = payload;
    PyObject *read_cb = wrapper_payload->read_cb;

    // We assume that the parser only needs the return value until the next time
    // this function is called or when ts_parser_parse() returns. We store the
    // return value from the callable in wrapper_payload->previous_return_value so
    // that its reference count will be decremented either during the next call to
    // this wrapper or after ts_parser_parse() has returned.
    Py_XDECREF(wrapper_payload->previous_return_value);
    wrapper_payload->previous_return_value = NULL;

    // Form arguments to callable.
    PyObject *byte_offset_obj = PyLong_FromSize_t((size_t)byte_offset);
    PyObject *position_obj = point_new(position);
    if (!position_obj || !byte_offset_obj) {
        *bytes_read = 0;
        return NULL;
    }

    PyObject *args = PyTuple_Pack(2, byte_offset_obj, position_obj);
    Py_XDECREF(byte_offset_obj);
    Py_XDECREF(position_obj);

    // Call callable.
    PyObject *rv = PyObject_Call(read_cb, args, NULL);
    Py_XDECREF(args);

    // If error or None returned, we've done parsing.
    if (!rv || (rv == Py_None)) {
        Py_XDECREF(rv);
        *bytes_read = 0;
        return NULL;
    }

    // If something other than None is returned, it must be a bytes object.
    if (!PyBytes_Check(rv)) {
        Py_XDECREF(rv);
        PyErr_SetString(PyExc_TypeError, "Read callable must return None or byte buffer type");
        *bytes_read = 0;
        return NULL;
    }

    // Store return value in payload so its reference count can be decremented and
    // return string representation of bytes.
    wrapper_payload->previous_return_value = rv;
    *bytes_read = PyBytes_Size(rv);
    return PyBytes_AsString(rv);
}

static PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *source_or_callback = NULL;
    PyObject *old_tree_arg = NULL;
    int keep_text = 1;
    static char *keywords[] = {"", "old_tree", "keep_text", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Op:parse", keywords, &source_or_callback,
                                     &old_tree_arg, &keep_text)) {
        return NULL;
    }

    const TSTree *old_tree = NULL;
    if (old_tree_arg) {
        if (!PyObject_IsInstance(old_tree_arg, (PyObject *)state->tree_type)) {
            PyErr_SetString(PyExc_TypeError, "Second argument to parse must be a Tree");
            return NULL;
        }
        old_tree = ((Tree *)old_tree_arg)->tree;
    }

    TSTree *new_tree = NULL;
    Py_buffer source_view;
    if (!PyObject_GetBuffer(source_or_callback, &source_view, PyBUF_SIMPLE)) {
        // parse a buffer
        const char *source_bytes = (const char *)source_view.buf;
        size_t length = source_view.len;
        new_tree = ts_parser_parse_string(self->parser, old_tree, source_bytes, length);
        PyBuffer_Release(&source_view);
    } else if (PyCallable_Check(source_or_callback)) {
        PyErr_Clear(); // clear the GetBuffer error
        // parse a callable
        ReadWrapperPayload payload = {
            .read_cb = source_or_callback,
            .previous_return_value = NULL,
        };
        TSInput input = {
            .payload = &payload,
            .read = parser_read_wrapper,
            .encoding = TSInputEncodingUTF8,
        };
        new_tree = ts_parser_parse(self->parser, old_tree, input);
        Py_XDECREF(payload.previous_return_value);

        // don't allow tree_new_internal to keep the source text
        source_or_callback = Py_None;
        keep_text = 0;
    } else {
        PyErr_SetString(PyExc_TypeError, "First argument byte buffer type or callable");
        return NULL;
    }

    if (!new_tree) {
        PyErr_SetString(PyExc_ValueError, "Parsing failed");
        return NULL;
    }

    return tree_new_internal(state, new_tree, source_or_callback, keep_text);
}

static PyObject *parser_reset(Parser *self, void *payload) {
    ts_parser_reset(self->parser);
    Py_RETURN_NONE;
}

static PyObject *parser_get_timeout_micros(Parser *self, void *payload) {
    return PyLong_FromUnsignedLong(ts_parser_timeout_micros(self->parser));
}

static PyObject *parser_set_timeout_micros(Parser *self, PyObject *arg) {
    long timeout;
    if (!PyArg_Parse(arg, "l", &timeout)) {
        return NULL;
    }

    if (timeout < 0) {
        PyErr_SetString(PyExc_ValueError, "Timeout must be a positive integer");
        return NULL;
    }

    ts_parser_set_timeout_micros(self->parser, timeout);
    Py_RETURN_NONE;
}

static PyObject *parser_set_included_ranges(Parser *self, PyObject *arg) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *ranges = NULL;
    if (!PyArg_Parse(arg, "O", &ranges)) {
        return NULL;
    }

    if (!PyList_Check(ranges)) {
        PyErr_SetString(PyExc_TypeError, "Included ranges must be a list");
        return NULL;
    }

    uint32_t length = PyList_Size(ranges);
    TSRange *c_ranges = malloc(sizeof(TSRange) * length);
    if (!c_ranges) {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    for (unsigned i = 0; i < length; i++) {
        PyObject *range = PyList_GetItem(ranges, i);
        if (!PyObject_IsInstance(range, (PyObject *)state->range_type)) {
            PyErr_SetString(PyExc_TypeError, "Included range must be a Range");
            free(c_ranges);
            return NULL;
        }
        c_ranges[i] = ((Range *)range)->range;
    }

    bool res = ts_parser_set_included_ranges(self->parser, c_ranges, length);
    if (!res) {
        PyErr_SetString(PyExc_ValueError,
                        "Included ranges must not overlap or end before it starts");
        free(c_ranges);
        return NULL;
    }

    free(c_ranges);
    Py_RETURN_NONE;
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

    TSLanguage *language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    Py_XDECREF(language_id);
    if (!language) {
        PyErr_SetString(PyExc_ValueError, "Language ID must not be null");
        return NULL;
    }

    unsigned version = ts_language_version(language);
    if (version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
        TREE_SITTER_LANGUAGE_VERSION < version) {
        return PyErr_Format(
            PyExc_ValueError, "Incompatible Language version %u. Must be between %u and %u",
            version, TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION, TREE_SITTER_LANGUAGE_VERSION);
    }

    ts_parser_set_language(self->parser, language);
    Py_RETURN_NONE;
}

static PyGetSetDef parser_accessors[] = {
    {"timeout_micros", (getter)parser_get_timeout_micros, NULL,
     "The timeout for parsing, in microseconds.", NULL},
    {NULL},
};

static PyMethodDef parser_methods[] = {
    {
        .ml_name = "parse",
        .ml_meth = (PyCFunction)parser_parse,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = "parse(bytes, old_tree=None, keep_text=True)\n--\n\n\
               Parse source code, creating a syntax tree.",
    },
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)parser_reset,
        .ml_flags = METH_NOARGS,
        .ml_doc = "reset()\n--\n\n\
			   Instruct the parser to start the next parse from the beginning.",
    },
    {
        .ml_name = "set_timeout_micros",
        .ml_meth = (PyCFunction)parser_set_timeout_micros,
        .ml_flags = METH_O,
        .ml_doc = "set_timeout_micros(timeout_micros)\n--\n\n\
			  Set the maximum duration in microseconds that parsing should be allowed to\
              take before halting.",
    },
    {
        .ml_name = "set_included_ranges",
        .ml_meth = (PyCFunction)parser_set_included_ranges,
        .ml_flags = METH_O,
        .ml_doc = "set_included_ranges(ranges)\n--\n\n\
			   Set the ranges of text that the parser should include when parsing.",
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

static PyType_Slot parser_type_slots[] = {
    {Py_tp_doc, "A parser"},
    {Py_tp_new, parser_new},
    {Py_tp_dealloc, parser_dealloc},
    {Py_tp_methods, parser_methods},
    {0, NULL},
};

static PyType_Spec parser_type_spec = {
    .name = "tree_sitter.Parser",
    .basicsize = sizeof(Parser),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = parser_type_slots,
};

// Query Capture

static void capture_dealloc(QueryCapture *self) { Py_TYPE(self)->tp_free(self); }

static PyType_Slot query_capture_type_slots[] = {
    {Py_tp_doc, "A query capture"},
    {Py_tp_dealloc, capture_dealloc},
    {0, NULL},
};

static PyType_Spec query_capture_type_spec = {
    .name = "tree_sitter.Capture",
    .basicsize = sizeof(QueryCapture),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = query_capture_type_slots,
};

static PyObject *query_capture_new_internal(ModuleState *state, TSQueryCapture capture) {
    QueryCapture *self =
        (QueryCapture *)state->query_capture_type->tp_alloc(state->query_capture_type, 0);
    if (self != NULL) {
        self->capture = capture;
    }
    return (PyObject *)self;
}

static void match_dealloc(QueryMatch *self) { Py_TYPE(self)->tp_free(self); }

static PyType_Slot query_match_type_slots[] = {
    {Py_tp_doc, "A query match"},
    {Py_tp_dealloc, match_dealloc},
    {0, NULL},
};

static PyType_Spec query_match_type_spec = {
    .name = "tree_sitter.QueryMatch",
    .basicsize = sizeof(QueryMatch),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = query_match_type_slots,
};

static PyObject *query_match_new_internal(ModuleState *state, TSQueryMatch match) {
    QueryMatch *self = (QueryMatch *)state->query_match_type->tp_alloc(state->query_match_type, 0);
    if (self != NULL) {
        self->match = match;
        self->captures = PyList_New(0);
        self->pattern_index = 0;
    }
    return (PyObject *)self;
}

// Text Predicates

static void capture_eq_capture_dealloc(CaptureEqCapture *self) { Py_TYPE(self)->tp_free(self); }

static void capture_eq_string_dealloc(CaptureEqString *self) {
    Py_XDECREF(self->string_value);
    Py_TYPE(self)->tp_free(self);
}

static void capture_match_string_dealloc(CaptureMatchString *self) {
    Py_XDECREF(self->regex);
    Py_TYPE(self)->tp_free(self);
}

// CaptureEqCapture
static PyType_Slot capture_eq_capture_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #eq? @capture1 @capture2"},
    {Py_tp_dealloc, capture_eq_capture_dealloc},
    {0, NULL},
};

static PyType_Spec capture_eq_capture_type_spec = {
    .name = "tree_sitter.CaptureEqCapture",
    .basicsize = sizeof(CaptureEqCapture),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = capture_eq_capture_type_slots,
};

// CaptureEqString
static PyType_Slot capture_eq_string_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #eq? @capture string"},
    {Py_tp_dealloc, capture_eq_string_dealloc},
    {0, NULL},
};

static PyType_Spec capture_eq_string_type_spec = {
    .name = "tree_sitter.CaptureEqString",
    .basicsize = sizeof(CaptureEqString),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = capture_eq_string_type_slots,
};

// CaptureMatchString
static PyType_Slot capture_match_string_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #match? @capture regex"},
    {Py_tp_dealloc, capture_match_string_dealloc},
    {0, NULL},
};

static PyType_Spec capture_match_string_type_spec = {
    .name = "tree_sitter.CaptureMatchString",
    .basicsize = sizeof(CaptureMatchString),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = capture_match_string_type_slots,
};

static PyObject *capture_eq_capture_new_internal(ModuleState *state, uint32_t capture1_value_id,
                                                 uint32_t capture2_value_id, int is_positive) {
    CaptureEqCapture *self = (CaptureEqCapture *)state->capture_eq_capture_type->tp_alloc(
        state->capture_eq_capture_type, 0);
    if (self != NULL) {
        self->capture1_value_id = capture1_value_id;
        self->capture2_value_id = capture2_value_id;
        self->is_positive = is_positive;
    }
    return (PyObject *)self;
}

static PyObject *capture_eq_string_new_internal(ModuleState *state, uint32_t capture_value_id,
                                                const char *string_value, int is_positive) {
    CaptureEqString *self = (CaptureEqString *)state->capture_eq_string_type->tp_alloc(
        state->capture_eq_string_type, 0);
    if (self != NULL) {
        self->capture_value_id = capture_value_id;
        self->string_value = PyBytes_FromString(string_value);
        self->is_positive = is_positive;
    }
    return (PyObject *)self;
}

static PyObject *capture_match_string_new_internal(ModuleState *state, uint32_t capture_value_id,
                                                   const char *string_value, int is_positive) {
    CaptureMatchString *self = (CaptureMatchString *)state->capture_match_string_type->tp_alloc(
        state->capture_match_string_type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->capture_value_id = capture_value_id;
    self->regex = PyObject_CallFunction(state->re_compile, "s", string_value);
    self->is_positive = is_positive;
    return (PyObject *)self;
}

static bool capture_eq_capture_is_instance(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return PyObject_IsInstance(self, (PyObject *)state->capture_eq_capture_type);
}

static bool capture_eq_string_is_instance(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return PyObject_IsInstance(self, (PyObject *)state->capture_eq_string_type);
}

static bool capture_match_string_is_instance(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return PyObject_IsInstance(self, (PyObject *)state->capture_match_string_type);
}

// Query

static Node *node_for_capture_index(ModuleState *state, uint32_t index, TSQueryMatch match,
                                    Tree *tree) {
    for (unsigned i = 0; i < match.capture_count; i++) {
        TSQueryCapture capture = match.captures[i];
        if (capture.index == index) {
            Node *capture_node = (Node *)node_new_internal(state, capture.node, (PyObject *)tree);
            return capture_node;
        }
    }
    return NULL;
}

static bool satisfies_text_predicates(Query *query, TSQueryMatch match, Tree *tree) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(query));
    PyObject *pattern_text_predicates = PyList_GetItem(query->text_predicates, match.pattern_index);
    // if there is no source, ignore the text predicates
    if (tree->source == Py_None || tree->source == NULL) {
        return true;
    }

    Node *node1 = NULL;
    Node *node2 = NULL;
    PyObject *node1_text = NULL;
    PyObject *node2_text = NULL;
    // check if all text_predicates are satisfied
    for (Py_ssize_t j = 0; j < PyList_Size(pattern_text_predicates); j++) {
        PyObject *text_predicate = PyList_GetItem(pattern_text_predicates, j);
        int is_satisfied;
        if (capture_eq_capture_is_instance(text_predicate)) {
            uint32_t capture1_value_id = ((CaptureEqCapture *)text_predicate)->capture1_value_id;
            uint32_t capture2_value_id = ((CaptureEqCapture *)text_predicate)->capture2_value_id;
            node1 = node_for_capture_index(state, capture1_value_id, match, tree);
            node2 = node_for_capture_index(state, capture2_value_id, match, tree);
            if (node1 == NULL || node2 == NULL) {
                is_satisfied = true;
                if (node1 != NULL) {
                    Py_XDECREF(node1);
                }
                if (node2 != NULL) {
                    Py_XDECREF(node2);
                }
            } else {
                node1_text = node_get_text(node1, NULL);
                node2_text = node_get_text(node2, NULL);
                if (node1_text == NULL || node2_text == NULL) {
                    goto error;
                }
                is_satisfied = PyObject_RichCompareBool(node1_text, node2_text, Py_EQ) ==
                               ((CaptureEqCapture *)text_predicate)->is_positive;
                Py_XDECREF(node1);
                Py_XDECREF(node2);
                Py_XDECREF(node1_text);
                Py_XDECREF(node2_text);
            }
            if (!is_satisfied) {
                return false;
            }
        } else if (capture_eq_string_is_instance(text_predicate)) {
            uint32_t capture_value_id = ((CaptureEqString *)text_predicate)->capture_value_id;
            node1 = node_for_capture_index(state, capture_value_id, match, tree);
            if (node1 == NULL) {
                is_satisfied = true;
            } else {
                node1_text = node_get_text(node1, NULL);
                if (node1_text == NULL) {
                    goto error;
                }
                PyObject *string_value = ((CaptureEqString *)text_predicate)->string_value;
                is_satisfied = PyObject_RichCompareBool(node1_text, string_value, Py_EQ) ==
                               ((CaptureEqString *)text_predicate)->is_positive;
            }
            Py_XDECREF(node1);
            Py_XDECREF(node1_text);
            if (!is_satisfied) {
                return false;
            }
        } else if (capture_match_string_is_instance(text_predicate)) {
            uint32_t capture_value_id = ((CaptureMatchString *)text_predicate)->capture_value_id;
            node1 = node_for_capture_index(state, capture_value_id, match, tree);
            if (node1 == NULL) {
                is_satisfied = true;
            } else {
                node1_text = node_get_text(node1, NULL);
                if (node1_text == NULL) {
                    goto error;
                }
                PyObject *search_result =
                    PyObject_CallMethod(((CaptureMatchString *)text_predicate)->regex, "search",
                                        "s", PyBytes_AsString(node1_text));
                Py_XDECREF(node1_text);
                is_satisfied = (search_result != NULL && search_result != Py_None) ==
                               ((CaptureMatchString *)text_predicate)->is_positive;
                if (search_result != NULL) {
                    Py_DECREF(search_result);
                }
            }
            Py_XDECREF(node1);
            if (!is_satisfied) {
                return false;
            }
        }
    }
    return true;

error:
    Py_XDECREF(node1);
    Py_XDECREF(node2);
    Py_XDECREF(node1_text);
    Py_XDECREF(node2_text);
    return false;
}

static PyObject *query_matches(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    char *keywords[] = {
        "node", "start_point", "end_point", "start_byte", "end_byte", NULL,
    };

    Node *node = NULL;
    TSPoint start_point = {.row = 0, .column = 0};
    TSPoint end_point = {.row = UINT32_MAX, .column = UINT32_MAX};
    unsigned start_byte = 0, end_byte = UINT32_MAX;

    int ok = PyArg_ParseTupleAndKeywords(args, kwargs, "O|(II)(II)II", keywords, (PyObject **)&node,
                                         &start_point.row, &start_point.column, &end_point.row,
                                         &end_point.column, &start_byte, &end_byte);
    if (!ok) {
        return NULL;
    }

    if (!PyObject_IsInstance((PyObject *)node, (PyObject *)state->node_type)) {
        PyErr_SetString(PyExc_TypeError, "First argument to captures must be a Node");
        return NULL;
    }

    ts_query_cursor_set_byte_range(state->query_cursor, start_byte, end_byte);
    ts_query_cursor_set_point_range(state->query_cursor, start_point, end_point);
    ts_query_cursor_exec(state->query_cursor, self->query, node->node);

    QueryMatch *match = NULL;
    PyObject *result = PyList_New(0);
    if (result == NULL) {
        goto error;
    }

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(state->query_cursor, &_match)) {
        match = (QueryMatch *)query_match_new_internal(state, _match);
        if (match == NULL) {
            goto error;
        }
        PyObject *captures_for_match = PyList_New(0);
        if (captures_for_match == NULL) {
            goto error;
        }
        bool is_satisfied = satisfies_text_predicates(self, _match, (Tree *)node->tree);
        for (unsigned i = 0; i < _match.capture_count; i++) {
            QueryCapture *capture =
                (QueryCapture *)query_capture_new_internal(state, _match.captures[i]);
            if (capture == NULL) {
                Py_XDECREF(captures_for_match);
                goto error;
            }
            if (is_satisfied) {
                PyObject *capture_name =
                    PyList_GetItem(self->capture_names, capture->capture.index);
                PyObject *capture_node =
                    node_new_internal(state, capture->capture.node, node->tree);
                PyObject *item = PyTuple_Pack(2, capture_node, capture_name);
                if (item == NULL) {
                    Py_XDECREF(captures_for_match);
                    Py_XDECREF(capture_node);
                    goto error;
                }
                Py_XDECREF(capture_node);
                PyList_Append(captures_for_match, item);
                Py_XDECREF(item);
            }
            Py_XDECREF(capture);
        }
        PyObject *pattern_index = PyLong_FromLong(_match.pattern_index);
        PyObject *tuple_match = PyTuple_Pack(2, pattern_index, captures_for_match);
        PyList_Append(result, tuple_match);
        Py_XDECREF(tuple_match);
        Py_XDECREF(pattern_index);
        Py_XDECREF(captures_for_match);
        Py_XDECREF(match);
    }
    return result;

error:
    Py_XDECREF(result);
    Py_XDECREF(match);
    return NULL;
}

static PyObject *query_captures(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    char *keywords[] = {
        "node", "start_point", "end_point", "start_byte", "end_byte", NULL,
    };

    Node *node = NULL;
    TSPoint start_point = {.row = 0, .column = 0};
    TSPoint end_point = {.row = UINT32_MAX, .column = UINT32_MAX};
    unsigned start_byte = 0, end_byte = UINT32_MAX;

    int ok = PyArg_ParseTupleAndKeywords(args, kwargs, "O|(II)(II)II", keywords, (PyObject **)&node,
                                         &start_point.row, &start_point.column, &end_point.row,
                                         &end_point.column, &start_byte, &end_byte);
    if (!ok) {
        return NULL;
    }

    if (!PyObject_IsInstance((PyObject *)node, (PyObject *)state->node_type)) {
        PyErr_SetString(PyExc_TypeError, "First argument to captures must be a Node");
        return NULL;
    }

    ts_query_cursor_set_byte_range(state->query_cursor, start_byte, end_byte);
    ts_query_cursor_set_point_range(state->query_cursor, start_point, end_point);
    ts_query_cursor_exec(state->query_cursor, self->query, node->node);

    QueryCapture *capture = NULL;
    PyObject *result = PyList_New(0);
    if (result == NULL) {
        goto error;
    }

    uint32_t capture_index;
    TSQueryMatch match;
    while (ts_query_cursor_next_capture(state->query_cursor, &match, &capture_index)) {
        capture = (QueryCapture *)query_capture_new_internal(state, match.captures[capture_index]);
        if (capture == NULL) {
            goto error;
        }
        if (satisfies_text_predicates(self, match, (Tree *)node->tree)) {
            PyObject *capture_name = PyList_GetItem(self->capture_names, capture->capture.index);
            PyObject *capture_node = node_new_internal(state, capture->capture.node, node->tree);
            PyObject *item = PyTuple_Pack(2, capture_node, capture_name);
            if (item == NULL) {
                goto error;
            }
            Py_XDECREF(capture_node);
            PyList_Append(result, item);
            Py_XDECREF(item);
        }
        Py_XDECREF(capture);
    }
    return result;

error:
    Py_XDECREF(result);
    Py_XDECREF(capture);
    return NULL;
}

static void query_dealloc(Query *self) {
    if (self->query) {
        ts_query_delete(self->query);
    }
    Py_XDECREF(self->capture_names);
    Py_XDECREF(self->text_predicates);
    Py_TYPE(self)->tp_free(self);
}

static PyMethodDef query_methods[] = {
    {.ml_name = "matches",
     .ml_meth = (PyCFunction)query_matches,
     .ml_flags = METH_KEYWORDS | METH_VARARGS,
     .ml_doc = "matches(node)\n--\n\n\
               Get a list of all of the matches within the given node."},
    {
        .ml_name = "captures",
        .ml_meth = (PyCFunction)query_captures,
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = "captures(node)\n--\n\n\
               Get a list of all of the captures within the given node.",
    },
    {NULL},
};

static PyType_Slot query_type_slots[] = {
    {Py_tp_doc, "A set of patterns to search for in a syntax tree."},
    {Py_tp_dealloc, query_dealloc},
    {Py_tp_methods, query_methods},
    {0, NULL},
};

static PyType_Spec query_type_spec = {
    .name = "tree_sitter.Query",
    .basicsize = sizeof(Query),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = query_type_slots,
};

static PyObject *query_new_internal(ModuleState *state, TSLanguage *language, char *source,
                                    int length) {
    Query *query = (Query *)state->query_type->tp_alloc(state->query_type, 0);
    if (query == NULL) {
        return NULL;
    }

    PyObject *pattern_text_predicates = NULL;
    uint32_t error_offset;
    TSQueryError error_type;
    query->query = ts_query_new(language, source, length, &error_offset, &error_type);
    if (!query->query) {
        char *word_start = &source[error_offset];
        char *word_end = word_start;
        while (word_end < &source[length] &&
               (iswalnum(*word_end) || *word_end == '-' || *word_end == '_' || *word_end == '?' ||
                *word_end == '.')) {
            word_end++;
        }
        char c = *word_end;
        *word_end = 0;
        switch (error_type) {
        case TSQueryErrorNodeType:
            PyErr_Format(PyExc_NameError, "Invalid node type %s", &source[error_offset]);
            break;
        case TSQueryErrorField:
            PyErr_Format(PyExc_NameError, "Invalid field name %s", &source[error_offset]);
            break;
        case TSQueryErrorCapture:
            PyErr_Format(PyExc_NameError, "Invalid capture name %s", &source[error_offset]);
            break;
        default:
            PyErr_Format(PyExc_SyntaxError, "Invalid syntax at offset %u", error_offset);
        }
        *word_end = c;
        goto error;
    }

    unsigned n = ts_query_capture_count(query->query);
    query->capture_names = PyList_New(n);
    Py_INCREF(Py_None);
    for (unsigned i = 0; i < n; i++) {
        unsigned length;
        const char *capture_name = ts_query_capture_name_for_id(query->query, i, &length);
        PyList_SetItem(query->capture_names, i, PyUnicode_FromStringAndSize(capture_name, length));
    }

    unsigned pattern_count = ts_query_pattern_count(query->query);
    query->text_predicates = PyList_New(pattern_count);
    if (query->text_predicates == NULL) {
        goto error;
    }

    for (unsigned i = 0; i < pattern_count; i++) {
        unsigned length;
        const TSQueryPredicateStep *predicate_step =
            ts_query_predicates_for_pattern(query->query, i, &length);
        pattern_text_predicates = PyList_New(0);
        if (pattern_text_predicates == NULL) {
            goto error;
        }
        for (unsigned j = 0; j < length; j++) {
            unsigned predicate_len = 0;
            while ((predicate_step + predicate_len)->type != TSQueryPredicateStepTypeDone) {
                predicate_len++;
            }

            if (predicate_step->type != TSQueryPredicateStepTypeString) {
                PyErr_Format(
                    PyExc_RuntimeError,
                    "Capture predicate must start with a string i=%d/pattern_count=%d "
                    "j=%d/length=%d predicate_step->type=%d TSQueryPredicateStepTypeDone=%d "
                    "TSQueryPredicateStepTypeCapture=%d TSQueryPredicateStepTypeString=%d",
                    i, pattern_count, j, length, predicate_step->type, TSQueryPredicateStepTypeDone,
                    TSQueryPredicateStepTypeCapture, TSQueryPredicateStepTypeString);
                goto error;
            }

            // Build a predicate for each of the supported predicate function names
            unsigned length;
            const char *operator_name =
                ts_query_string_value_for_id(query->query, predicate_step->value_id, &length);
            if (strcmp(operator_name, "eq?") == 0 || strcmp(operator_name, "not-eq?") == 0) {
                if (predicate_len != 3) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "Wrong number of arguments to #eq? or #not-eq? predicate");
                    goto error;
                }
                if (predicate_step[1].type != TSQueryPredicateStepTypeCapture) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "First argument to #eq? or #not-eq? must be a capture name");
                    goto error;
                }
                int is_positive = strcmp(operator_name, "eq?") == 0;
                switch (predicate_step[2].type) {
                case TSQueryPredicateStepTypeCapture:;
                    CaptureEqCapture *capture_eq_capture_predicate =
                        (CaptureEqCapture *)capture_eq_capture_new_internal(
                            state, predicate_step[1].value_id, predicate_step[2].value_id,
                            is_positive);
                    if (capture_eq_capture_predicate == NULL) {
                        goto error;
                    }
                    PyList_Append(pattern_text_predicates,
                                  (PyObject *)capture_eq_capture_predicate);
                    Py_DECREF(capture_eq_capture_predicate);
                    break;
                case TSQueryPredicateStepTypeString:;
                    const char *string_value = ts_query_string_value_for_id(
                        query->query, predicate_step[2].value_id, &length);
                    CaptureEqString *capture_eq_string_predicate =
                        (CaptureEqString *)capture_eq_string_new_internal(
                            state, predicate_step[1].value_id, string_value, is_positive);
                    if (capture_eq_string_predicate == NULL) {
                        goto error;
                    }
                    PyList_Append(pattern_text_predicates, (PyObject *)capture_eq_string_predicate);
                    Py_DECREF(capture_eq_string_predicate);
                    break;
                default:
                    PyErr_SetString(PyExc_RuntimeError, "Second argument to #eq? or #not-eq? must "
                                                        "be a capture name or a string literal");
                    goto error;
                }
            } else if (strcmp(operator_name, "match?") == 0 ||
                       strcmp(operator_name, "not-match?") == 0) {
                if (predicate_len != 3) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "Wrong number of arguments to #match? or #not-match? predicate");
                    goto error;
                }
                if (predicate_step[1].type != TSQueryPredicateStepTypeCapture) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "First argument to #match? or #not-match? must be a capture name");
                    goto error;
                }
                if (predicate_step[2].type != TSQueryPredicateStepTypeString) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "Second argument to #match? or #not-match? must be a regex string");
                    goto error;
                }
                const char *string_value =
                    ts_query_string_value_for_id(query->query, predicate_step[2].value_id, &length);
                int is_positive = strcmp(operator_name, "match?") == 0;
                CaptureMatchString *capture_match_string_predicate =
                    (CaptureMatchString *)capture_match_string_new_internal(
                        state, predicate_step[1].value_id, string_value, is_positive);
                if (capture_match_string_predicate == NULL) {
                    goto error;
                }
                PyList_Append(pattern_text_predicates, (PyObject *)capture_match_string_predicate);
                Py_DECREF(capture_match_string_predicate);
            }
            predicate_step += predicate_len + 1;
            j += predicate_len;
        }
        PyList_SetItem(query->text_predicates, i, pattern_text_predicates);
    }
    return (PyObject *)query;

error:
    query_dealloc(query);
    Py_XDECREF(pattern_text_predicates);
    return NULL;
}

// Range

PyMODINIT_FUNC range_init(Range *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    char *keywords[] = {
        "start_point", "end_point", "start_byte", "end_byte", NULL,
    };

    PyObject *start_point_obj;
    PyObject *end_point_obj;
    unsigned start_byte;
    unsigned end_byte;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!O!II", keywords, &PyTuple_Type,
                                     &start_point_obj, &PyTuple_Type, &end_point_obj, &start_byte,
                                     &end_byte)) {
        PyErr_SetString(PyExc_TypeError, "Invalid arguments to Range()");
        return NULL;
    }

    if (start_point_obj && !PyArg_ParseTuple(start_point_obj, "II", &self->range.start_point.row,
                                             &self->range.start_point.column)) {
        PyErr_SetString(PyExc_TypeError, "Invalid start_point to Range()");
        return NULL;
    }

    if (end_point_obj && !PyArg_ParseTuple(end_point_obj, "II", &self->range.end_point.row,
                                           &self->range.end_point.column)) {
        PyErr_SetString(PyExc_TypeError, "Invalid end_point to Range()");
        return NULL;
    }

    self->range.start_byte = start_byte;
    self->range.end_byte = end_byte;

    return 0;
}

static void range_dealloc(Range *self) { Py_TYPE(self)->tp_free(self); }

static PyObject *range_repr(Range *self) {
    const char *format_string =
        "<Range start_point=(%u, %u), start_byte=%u, end_point=(%u, %u), end_byte=%u>";
    return PyUnicode_FromFormat(format_string, self->range.start_point.row,
                                self->range.start_point.column, self->range.start_byte,
                                self->range.end_point.row, self->range.end_point.column,
                                self->range.end_byte);
}

static bool range_is_instance(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return PyObject_IsInstance(self, (PyObject *)state->range_type);
}

static PyObject *range_compare(Range *self, Range *other, int op) {
    if (range_is_instance((PyObject *)other)) {
        bool result = ((self->range.start_point.row == other->range.start_point.row) &&
                       (self->range.start_point.column == other->range.start_point.column) &&
                       (self->range.start_byte == other->range.start_byte) &&
                       (self->range.end_point.row == other->range.end_point.row) &&
                       (self->range.end_point.column == other->range.end_point.column) &&
                       (self->range.end_byte == other->range.end_byte));
        switch (op) {
        case Py_EQ:
            return PyBool_FromLong(result);
        case Py_NE:
            return PyBool_FromLong(!result);
        default:
            Py_RETURN_FALSE;
        }
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *range_get_start_point(Range *self, void *payload) {
    return point_new(self->range.start_point);
}

static PyObject *range_get_end_point(Range *self, void *payload) {
    return point_new(self->range.end_point);
}

static PyObject *range_get_start_byte(Range *self, void *payload) {
    return PyLong_FromSize_t((size_t)(self->range.start_byte));
}

static PyObject *range_get_end_byte(Range *self, void *payload) {
    return PyLong_FromSize_t((size_t)(self->range.end_byte));
}

static PyGetSetDef range_accessors[] = {
    {"start_point", (getter)range_get_start_point, NULL, "The start point of this range", NULL},
    {"start_byte", (getter)range_get_start_byte, NULL, "The start byte of this range", NULL},
    {"end_point", (getter)range_get_end_point, NULL, "The end point of this range", NULL},
    {"end_byte", (getter)range_get_end_byte, NULL, "The end byte of this range", NULL},
    {NULL},
};

static PyType_Slot range_type_slots[] = {
    {Py_tp_doc, "A range within a document."},
    {Py_tp_init, range_init},
    {Py_tp_dealloc, range_dealloc},
    {Py_tp_repr, range_repr},
    {Py_tp_richcompare, range_compare},
    {Py_tp_getset, range_accessors},
    {0, NULL},
};

static PyType_Spec range_type_spec = {
    .name = "tree_sitter.Range",
    .basicsize = sizeof(Range),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = range_type_slots,
};

static PyObject *range_new_internal(ModuleState *state, TSRange range) {
    Range *self = (Range *)state->range_type->tp_alloc(state->range_type, 0);
    if (self != NULL) {
        self->range = range;
    }
    return (PyObject *)self;
}

// LookaheadIterator

static void lookahead_iterator_dealloc(LookaheadIterator *self) {
    if (self->lookahead_iterator) {
        ts_lookahead_iterator_delete(self->lookahead_iterator);
    }
    Py_TYPE(self)->tp_free(self);
}

static PyObject *lookahead_iterator_repr(LookaheadIterator *self) {
    const char *format_string = "<LookaheadIterator %x>";
    return PyUnicode_FromFormat(format_string, self->lookahead_iterator);
}

static PyObject *lookahead_iterator_get_language(LookaheadIterator *self, void *payload) {
    return PyLong_FromVoidPtr((void *)ts_lookahead_iterator_language(self->lookahead_iterator));
}

static PyObject *lookahead_iterator_get_current_symbol(LookaheadIterator *self, void *payload) {
    return PyLong_FromSize_t(
        (size_t)ts_lookahead_iterator_current_symbol(self->lookahead_iterator));
}

static PyObject *lookahead_iterator_get_current_symbol_name(LookaheadIterator *self,
                                                            void *payload) {
    const char *name = ts_lookahead_iterator_current_symbol_name(self->lookahead_iterator);
    return PyUnicode_FromString(name);
}

static PyObject *lookahead_iterator_reset(LookaheadIterator *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &state_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyBool_FromLong(
        ts_lookahead_iterator_reset(self->lookahead_iterator, language, state_id));
}

static PyObject *lookahead_iterator_reset_state(LookaheadIterator *self, PyObject *args) {
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "H", &state_id)) {
        return NULL;
    }
    return PyBool_FromLong(ts_lookahead_iterator_reset_state(self->lookahead_iterator, state_id));
}

static PyObject *lookahead_iterator_iter(LookaheadIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *lookahead_iterator_next(LookaheadIterator *self) {
    bool res = ts_lookahead_iterator_next(self->lookahead_iterator);
    if (res) {
        return PyLong_FromSize_t(
            (size_t)ts_lookahead_iterator_current_symbol(self->lookahead_iterator));
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

static PyObject *lookahead_iterator_names_iterator(LookaheadIterator *self) {
    return lookahead_names_iterator_new_internal(PyType_GetModuleState(Py_TYPE(self)),
                                                 self->lookahead_iterator);
}

static PyObject *lookahead_iterator(PyObject *self, PyObject *args) {
    ModuleState *state = PyModule_GetState(self);

    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &state_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);

    TSLookaheadIterator *lookahead_iterator = ts_lookahead_iterator_new(language, state_id);

    if (lookahead_iterator == NULL) {
        Py_RETURN_NONE;
    }

    return lookahead_iterator_new_internal(state, lookahead_iterator);
}

static PyObject *lookahead_iterator_new_internal(ModuleState *state,
                                                 TSLookaheadIterator *lookahead_iterator) {
    LookaheadIterator *self = (LookaheadIterator *)state->lookahead_iterator_type->tp_alloc(
        state->lookahead_iterator_type, 0);
    if (self != NULL) {
        self->lookahead_iterator = lookahead_iterator;
    }
    return (PyObject *)self;
}

static PyGetSetDef lookahead_iterator_accessors[] = {
    {"language", (getter)lookahead_iterator_get_language, NULL, "Get the language.", NULL},
    {"current_symbol", (getter)lookahead_iterator_get_current_symbol, NULL,
     "Get the current symbol.", NULL},
    {"current_symbol_name", (getter)lookahead_iterator_get_current_symbol_name, NULL,
     "Get the current symbol name.", NULL},
    {NULL},
};

static PyMethodDef lookahead_iterator_methods[] = {
    {.ml_name = "reset",
     .ml_meth = (PyCFunction)lookahead_iterator_reset,
     .ml_flags = METH_VARARGS,
     .ml_doc = "reset(language, state)\n--\n\n\
			  Reset the lookahead iterator to a new language and parse state.\n\
        	  This returns `True` if the language was set successfully, and `False` otherwise."},
    {.ml_name = "reset_state",
     .ml_meth = (PyCFunction)lookahead_iterator_reset_state,
     .ml_flags = METH_VARARGS,
     .ml_doc = "reset_state(state)\n--\n\n\
			  Reset the lookahead iterator to a new parse state.\n\
			  This returns `True` if the state was set successfully, and `False` otherwise."},
    {
        .ml_name = "iter_names",
        .ml_meth = (PyCFunction)lookahead_iterator_names_iterator,
        .ml_flags = METH_NOARGS,
        .ml_doc = "iter_names()\n--\n\n\
			  Get an iterator of the names of possible syntax nodes that could come next.",
    },
    {NULL},
};

static PyType_Slot lookahead_iterator_type_slots[] = {
    {Py_tp_doc, "An iterator over the possible syntax nodes that could come next."},
    {Py_tp_dealloc, lookahead_iterator_dealloc},
    {Py_tp_repr, lookahead_iterator_repr},
    {Py_tp_getset, lookahead_iterator_accessors},
    {Py_tp_methods, lookahead_iterator_methods},
    {Py_tp_iter, lookahead_iterator_iter},
    {Py_tp_iternext, lookahead_iterator_next},
    {0, NULL},
};

static PyType_Spec lookahead_iterator_type_spec = {
    .name = "tree_sitter.LookaheadIterator",
    .basicsize = sizeof(LookaheadIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = lookahead_iterator_type_slots,
};

// LookaheadNamesIterator

static PyObject *lookahead_names_iterator_new_internal(ModuleState *state,
                                                       TSLookaheadIterator *lookahead_iterator) {
    LookaheadNamesIterator *self =
        (LookaheadNamesIterator *)state->lookahead_names_iterator_type->tp_alloc(
            state->lookahead_names_iterator_type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->lookahead_iterator = lookahead_iterator;
    return (PyObject *)self;
}

static PyObject *lookahead_names_iterator_repr(LookaheadNamesIterator *self) {
    const char *format_string = "<LookaheadNamesIterator %x>";
    return PyUnicode_FromFormat(format_string, self->lookahead_iterator);
}

static void lookahead_names_iterator_dealloc(LookaheadNamesIterator *self) {
    Py_TYPE(self)->tp_free(self);
}

static PyObject *lookahead_names_iterator_iter(LookaheadNamesIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *lookahead_names_iterator_next(LookaheadNamesIterator *self) {
    bool res = ts_lookahead_iterator_next(self->lookahead_iterator);
    if (res) {
        return PyUnicode_FromString(
            ts_lookahead_iterator_current_symbol_name(self->lookahead_iterator));
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

static PyType_Slot lookahead_names_iterator_type_slots[] = {
    {Py_tp_doc, "An iterator over the possible syntax nodes that could come next."},
    {Py_tp_dealloc, lookahead_names_iterator_dealloc},
    {Py_tp_repr, lookahead_names_iterator_repr},
    {Py_tp_iter, lookahead_names_iterator_iter},
    {Py_tp_iternext, lookahead_names_iterator_next},
    {0, NULL},
};

static PyType_Spec lookahead_names_iterator_type_spec = {
    .name = "tree_sitter.LookaheadNamesIterator",
    .basicsize = sizeof(LookaheadNamesIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = lookahead_names_iterator_type_slots,
};

// Module

static PyObject *language_version(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_version(language));
}

static PyObject *language_symbol_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_symbol_count(language));
}

static PyObject *language_state_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_state_count(language));
}

static PyObject *language_symbol_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    const char *name = ts_language_symbol_name(language, symbol);
    if (name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name);
}

static PyObject *language_symbol_for_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    char *kind;
    Py_ssize_t length;
    bool named;
    if (!PyArg_ParseTuple(args, "Os#p", &language_id, &kind, &length, &named)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    TSSymbol symbol = ts_language_symbol_for_name(language, kind, length, named);
    if (symbol == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromSize_t((size_t)symbol);
}

static PyObject *language_symbol_type(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t(ts_language_symbol_type(language, symbol));
}

static PyObject *language_field_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t(ts_language_field_count(language));
}

static PyObject *language_field_name_for_id(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t field_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &field_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    const char *field_name = ts_language_field_name_for_id(language, field_id);

    if (field_name == NULL) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromString(field_name);
}

static PyObject *language_field_id_for_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    char *field_name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "Os#", &language_id, &field_name, &length)) {
        return NULL;
    }

    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    TSFieldId field_id = ts_language_field_id_for_name(language, field_name, length);

    if (field_id == 0) {
        Py_RETURN_NONE;
    }

    return PyLong_FromSize_t((size_t)field_id);
}

static PyObject *language_query(PyObject *self, PyObject *args) {
    ModuleState *state = PyModule_GetState(self);
    TSLanguage *language;
    PyObject *language_id;
    char *source;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "Os#", &language_id, &source, &length)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return query_new_internal(state, language, source, length);
}

static PyObject *next_state(PyObject *self, PyObject *args) {
    ModuleState *state = PyModule_GetState(self);
    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    uint16_t symbol;
    if (!PyArg_ParseTuple(args, "OHH", &language_id, &state_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_next_state(language, state_id, symbol));
}

static void module_free(void *self) {
    ModuleState *state = PyModule_GetState((PyObject *)self);
    ts_query_cursor_delete(state->query_cursor);
    Py_XDECREF(state->tree_type);
    Py_XDECREF(state->tree_cursor_type);
    Py_XDECREF(state->parser_type);
    Py_XDECREF(state->node_type);
    Py_XDECREF(state->query_type);
    Py_XDECREF(state->range_type);
    Py_XDECREF(state->query_capture_type);
    Py_XDECREF(state->capture_eq_capture_type);
    Py_XDECREF(state->capture_eq_string_type);
    Py_XDECREF(state->capture_match_string_type);
    Py_XDECREF(state->lookahead_iterator_type);
    Py_XDECREF(state->re_compile);
}

static PyMethodDef module_methods[] = {
    {
        .ml_name = "_language_version",
        .ml_meth = (PyCFunction)language_version,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_count",
        .ml_meth = (PyCFunction)language_symbol_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_state_count",
        .ml_meth = (PyCFunction)language_state_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_name",
        .ml_meth = (PyCFunction)language_symbol_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_for_name",
        .ml_meth = (PyCFunction)language_symbol_for_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_type",
        .ml_meth = (PyCFunction)language_symbol_type,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_count",
        .ml_meth = (PyCFunction)language_field_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_name_for_id",
        .ml_meth = (PyCFunction)language_field_name_for_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_id_for_name",
        .ml_meth = (PyCFunction)language_field_id_for_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_next_state",
        .ml_meth = (PyCFunction)next_state,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_lookahead_iterator",
        .ml_meth = (PyCFunction)lookahead_iterator,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_query",
        .ml_meth = (PyCFunction)language_query,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {NULL},
};

static struct PyModuleDef module_definition = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "binding",
    .m_doc = NULL,
    .m_size = sizeof(ModuleState),
    .m_free = module_free,
    .m_methods = module_methods,
};

// simulate PyModule_AddObjectRef for pre-Python 3.10
static int AddObjectRef(PyObject *module, const char *name, PyObject *value) {
    if (value == NULL) {
        PyErr_Format(PyExc_SystemError, "PyModule_AddObjectRef() %s == NULL", name);
        return -1;
    }
    int ret = PyModule_AddObject(module, name, value);
    if (ret == 0) {
        Py_INCREF(value);
    }
    return ret;
}

PyMODINIT_FUNC PyInit_binding(void) {
    PyObject *module = PyModule_Create(&module_definition);
    if (module == NULL) {
        return NULL;
    }

    ModuleState *state = PyModule_GetState(module);

    state->tree_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_type_spec, NULL);
    state->tree_cursor_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_cursor_type_spec, NULL);
    state->parser_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &parser_type_spec, NULL);
    state->node_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &node_type_spec, NULL);
    state->query_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_type_spec, NULL);
    state->range_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &range_type_spec, NULL);
    state->query_capture_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_capture_type_spec, NULL);
    state->query_match_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_match_type_spec, NULL);
    state->capture_eq_capture_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_eq_capture_type_spec, NULL);
    state->capture_eq_string_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_eq_string_type_spec, NULL);
    state->capture_match_string_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_match_string_type_spec, NULL);
    state->lookahead_iterator_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &lookahead_iterator_type_spec, NULL);
    state->lookahead_names_iterator_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &lookahead_names_iterator_type_spec, NULL);

    state->query_cursor = ts_query_cursor_new();
    if ((AddObjectRef(module, "Tree", (PyObject *)state->tree_type) < 0) ||
        (AddObjectRef(module, "TreeCursor", (PyObject *)state->tree_cursor_type) < 0) ||
        (AddObjectRef(module, "Parser", (PyObject *)state->parser_type) < 0) ||
        (AddObjectRef(module, "Node", (PyObject *)state->node_type) < 0) ||
        (AddObjectRef(module, "Query", (PyObject *)state->query_type) < 0) ||
        (AddObjectRef(module, "Range", (PyObject *)state->range_type) < 0) ||
        (AddObjectRef(module, "QueryCapture", (PyObject *)state->query_capture_type) < 0) ||
        (AddObjectRef(module, "QueryMatch", (PyObject *)state->query_match_type) < 0) ||
        (AddObjectRef(module, "CaptureEqCapture", (PyObject *)state->capture_eq_capture_type) <
         0) ||
        (AddObjectRef(module, "CaptureEqString", (PyObject *)state->capture_eq_string_type) < 0) ||
        (AddObjectRef(module, "CaptureMatchString", (PyObject *)state->capture_match_string_type) <
         0) ||
        (AddObjectRef(module, "LookaheadIterator", (PyObject *)state->lookahead_iterator_type) <
         0) ||
        (AddObjectRef(module, "LookaheadNamesIterator",
                      (PyObject *)state->lookahead_names_iterator_type) < 0)) {
        goto cleanup;
    }

    PyObject *re_module = PyImport_ImportModule("re");
    if (re_module == NULL) {
        goto cleanup;
    }
    state->re_compile = PyObject_GetAttrString(re_module, (char *)"compile");
    Py_DECREF(re_module);
    if (state->re_compile == NULL) {
        goto cleanup;
    }

#if PY_VERSION_HEX < 0x030900f0
    global_state = state;
#endif
    return module;

cleanup:
    Py_XDECREF(module);
    return NULL;
}
