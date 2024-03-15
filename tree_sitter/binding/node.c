#include "node.h"
#include "point.h"
#include "range.h"
#include "tree_cursor.h"

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree) {
    Node *self = (Node *)state->node_type->tp_alloc(state->node_type, 0);
    if (self != NULL) {
        self->node = node;
        Py_INCREF(tree);
        self->tree = tree;
        self->children = NULL;
    }
    return (PyObject *)self;
}

void node_dealloc(Node *self) {
    Py_XDECREF(self->children);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free(self);
}

static inline bool node_is_instance(ModuleState *state, PyObject *self) {
    return PyObject_IsInstance(self, (PyObject *)state->node_type);
}

PyObject *node_repr(Node *self) {
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

PyObject *node_compare(Node *self, Node *other, int op) {
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

PyObject *node_sexp(Node *self, PyObject *args) {
    char *string = ts_node_string(self->node);
    PyObject *result = PyUnicode_FromString(string);
    free(string);
    return result;
}

PyObject *node_walk(Node *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return tree_cursor_new_internal(state, self->node, self->tree);
}

PyObject *node_edit(Node *self, PyObject *args, PyObject *kwargs) {
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

PyObject *node_child(Node *self, PyObject *args) {
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

PyObject *node_named_child(Node *self, PyObject *args) {
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

PyObject *node_child_by_field_id(Node *self, PyObject *args) {
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

PyObject *node_child_by_field_name(Node *self, PyObject *args) {
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

PyObject *node_children_by_field_id_internal(Node *self, TSFieldId field_id) {
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

PyObject *node_children_by_field_id(Node *self, PyObject *args) {
    TSFieldId field_id;
    if (!PyArg_ParseTuple(args, "H", &field_id)) {
        return NULL;
    }

    return node_children_by_field_id_internal(self, field_id);
}

PyObject *node_children_by_field_name(Node *self, PyObject *args) {
    char *name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#", &name, &length)) {
        return NULL;
    }

    const TSLanguage *lang = ts_tree_language(((Tree *)self->tree)->tree);
    TSFieldId field_id = ts_language_field_id_for_name(lang, name, length);
    return node_children_by_field_id_internal(self, field_id);
}

PyObject *node_field_name_for_child(Node *self, PyObject *args) {
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

PyObject *node_descendant_for_byte_range(Node *self, PyObject *args) {
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

PyObject *node_named_descendant_for_byte_range(Node *self, PyObject *args) {
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

PyObject *node_descendant_for_point_range(Node *self, PyObject *args) {
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

PyObject *node_named_descendant_for_point_range(Node *self, PyObject *args) {
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

PyObject *node_get_id(Node *self, void *payload) {
    return PyLong_FromVoidPtr((void *)self->node.id);
}

PyObject *node_get_kind_id(Node *self, void *payload) {
    TSSymbol kind_id = ts_node_symbol(self->node);
    return PyLong_FromLong(kind_id);
}

PyObject *node_get_grammar_id(Node *self, void *payload) {
    TSSymbol grammar_id = ts_node_grammar_symbol(self->node);
    return PyLong_FromLong(grammar_id);
}

PyObject *node_get_type(Node *self, void *payload) {
    return PyUnicode_FromString(ts_node_type(self->node));
}

PyObject *node_get_grammar_name(Node *self, void *payload) {
    return PyUnicode_FromString(ts_node_grammar_type(self->node));
}

PyObject *node_get_is_named(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_named(self->node));
}

PyObject *node_get_is_extra(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_extra(self->node));
}

PyObject *node_get_has_changes(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_has_changes(self->node));
}

PyObject *node_get_has_error(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_has_error(self->node));
}

PyObject *node_get_is_error(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_error(self->node));
}

PyObject *node_get_parse_state(Node *self, void *payload) {
    return PyLong_FromLong(ts_node_parse_state(self->node));
}

PyObject *node_get_next_parse_state(Node *self, void *payload) {
    return PyLong_FromLong(ts_node_next_parse_state(self->node));
}

PyObject *node_get_is_missing(Node *self, void *payload) {
    return PyBool_FromLong(ts_node_is_missing(self->node));
}

PyObject *node_get_start_byte(Node *self, void *payload) {
    return PyLong_FromSize_t((size_t)ts_node_start_byte(self->node));
}

PyObject *node_get_end_byte(Node *self, void *payload) {
    return PyLong_FromSize_t((size_t)ts_node_end_byte(self->node));
}

PyObject *node_get_byte_range(Node *self, void *payload) {
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

PyObject *node_get_range(Node *self, void *payload) {
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

PyObject *node_get_start_point(Node *self, void *payload) {
    TSPoint point = ts_node_start_point(self->node);
    return POINT_NEW(GET_MODULE_STATE(Py_TYPE(self)), point);
}

PyObject *node_get_end_point(Node *self, void *payload) {
    TSPoint point = ts_node_end_point(self->node);
    return POINT_NEW(GET_MODULE_STATE(Py_TYPE(self)), point);
}

PyObject *node_get_children(Node *self, void *payload) {
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

PyObject *node_get_named_children(Node *self, void *payload) {
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

PyObject *node_get_child_count(Node *self, void *payload) {
    long length = (long)ts_node_child_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

PyObject *node_get_named_child_count(Node *self, void *payload) {
    long length = (long)ts_node_named_child_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

PyObject *node_get_parent(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode parent = ts_node_parent(self->node);
    if (ts_node_is_null(parent)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, parent, self->tree);
}

PyObject *node_get_next_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_sibling = ts_node_next_sibling(self->node);
    if (ts_node_is_null(next_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_sibling, self->tree);
}

PyObject *node_get_prev_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_sibling = ts_node_prev_sibling(self->node);
    if (ts_node_is_null(prev_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_sibling, self->tree);
}

PyObject *node_get_next_named_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_named_sibling = ts_node_next_named_sibling(self->node);
    if (ts_node_is_null(next_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_named_sibling, self->tree);
}

PyObject *node_get_prev_named_sibling(Node *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_named_sibling = ts_node_prev_named_sibling(self->node);
    if (ts_node_is_null(prev_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_named_sibling, self->tree);
}

PyObject *node_get_descendant_count(Node *self, void *payload) {
    long length = (long)ts_node_descendant_count(self->node);
    PyObject *result = PyLong_FromLong(length);
    return result;
}

PyObject *node_get_text(Node *self, void *payload) {
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

Py_hash_t node_hash(Node *self) {
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

PyType_Spec node_type_spec = {
    .name = "tree_sitter.Node",
    .basicsize = sizeof(Node),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = node_type_slots,
};
