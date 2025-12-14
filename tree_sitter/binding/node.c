#include "types.h"

PyObject *point_new_internal(ModuleState *state, TSPoint point);

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree) {
    Node *self = PyObject_New(Node, state->node_type);
    if (self == NULL) {
        return NULL;
    }
    self->node = node;
    self->tree = Py_NewRef(tree);
    self->children = NULL;
    return PyObject_Init((PyObject *)self, state->node_type);
}

void node_dealloc(Node *self) {
    Py_XDECREF(self->children);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free(self);
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

PyObject *node_str(Node *self) {
    char *string = ts_node_string(self->node);
    PyObject *result = PyUnicode_FromString(string);
    PyMem_Free(string);
    return result;
}

PyObject *node_compare(Node *self, PyObject *other, int op) {
    if ((op != Py_EQ && op != Py_NE) || !IS_INSTANCE(other, node_type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    bool result = ts_node_eq(self->node, ((Node *)other)->node);
    return PyBool_FromLong(result ^ (op == Py_NE));
}

PyObject *node_walk(Node *self, PyObject *Py_UNUSED(args)) {
    ModuleState *state = GET_MODULE_STATE(self);
    TreeCursor *tree_cursor = PyObject_New(TreeCursor, state->tree_cursor_type);
    if (tree_cursor == NULL) {
        return NULL;
    }

    tree_cursor->tree = Py_NewRef(self->tree);
    tree_cursor->node = NULL;
    tree_cursor->cursor = ts_tree_cursor_new(self->node);
    return PyObject_Init((PyObject *)tree_cursor, state->tree_cursor_type);
}

PyObject *node_edit(Node *self, PyObject *args, PyObject *kwargs) {
    uint32_t start_byte, start_row, start_column;
    uint32_t old_end_byte, old_end_row, old_end_column;
    uint32_t new_end_byte, new_end_row, new_end_column;
    char *keywords[] = {
        "start_byte",    "old_end_byte",  "new_end_byte", "start_point",
        "old_end_point", "new_end_point", NULL,
    };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "III(II)(II)(II):edit", keywords, &start_byte,
                                     &old_end_byte, &new_end_byte, &start_row, &start_column,
                                     &old_end_row, &old_end_column, &new_end_row,
                                     &new_end_column)) {
        return NULL;
    }

    TSInputEdit edit = {
        .start_byte = start_byte,
        .old_end_byte = old_end_byte,
        .new_end_byte = new_end_byte,
        .start_point = {start_row, start_column},
        .old_end_point = {old_end_row, old_end_column},
        .new_end_point = {new_end_row, new_end_column},
    };

    ts_node_edit(&self->node, &edit);

    Py_RETURN_NONE;
}

PyObject *node_child(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    long index;
    if (!PyArg_ParseTuple(args, "l:child", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "child index must be positive");
        return NULL;
    }

    if ((uint32_t)index >= ts_node_child_count(self->node)) {
        PyErr_SetString(PyExc_IndexError, "child index out of range");
        return NULL;
    }

    TSNode child = ts_node_child(self->node, (uint32_t)index);
    return node_new_internal(state, child, self->tree);
}

PyObject *node_named_child(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    long index;
    if (!PyArg_ParseTuple(args, "l:named_child", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "child index must be positive");
        return NULL;
    }
    if ((uint32_t)index >= ts_node_named_child_count(self->node)) {
        PyErr_SetString(PyExc_IndexError, "child index out of range");
        return NULL;
    }

    TSNode child = ts_node_named_child(self->node, (uint32_t)index);
    return node_new_internal(state, child, self->tree);
}

PyObject *node_first_child_for_byte(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    uint32_t byte;
    if (!PyArg_ParseTuple(args, "I:first_child_for_byte", &byte)) {
        return NULL;
    }
    TSNode child = ts_node_first_child_for_byte(self->node, byte);
    return node_new_internal(state, child, self->tree);
}

PyObject *node_first_named_child_for_byte(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    uint32_t byte;
    if (!PyArg_ParseTuple(args, "I:first_named_child_for_byte", &byte)) {
        return NULL;
    }
    TSNode child = ts_node_first_named_child_for_byte(self->node, byte);
    return node_new_internal(state, child, self->tree);
}

PyObject *node_child_by_field_id(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    TSFieldId field_id;
    if (!PyArg_ParseTuple(args, "H:child_by_field_id", &field_id)) {
        return NULL;
    }

    TSNode child = ts_node_child_by_field_id(self->node, field_id);
    if (ts_node_is_null(child)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, child, self->tree);
}

PyObject *node_child_by_field_name(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:child_by_field_name", &name, &length)) {
        return NULL;
    }

    TSNode child = ts_node_child_by_field_name(self->node, name, length);
    if (ts_node_is_null(child)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, child, self->tree);
}

static PyObject *node_children_by_field_id_internal(Node *self, TSFieldId field_id) {
    ModuleState *state = GET_MODULE_STATE(self);
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
    if (!PyArg_ParseTuple(args, "H:child_by_field_id", &field_id)) {
        return NULL;
    }

    return node_children_by_field_id_internal(self, field_id);
}

PyObject *node_children_by_field_name(Node *self, PyObject *args) {
    char *name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:child_by_field_name", &name, &length)) {
        return NULL;
    }

    const TSLanguage *lang = ts_tree_language(((Tree *)self->tree)->tree);
    TSFieldId field_id = ts_language_field_id_for_name(lang, name, length);
    return node_children_by_field_id_internal(self, field_id);
}

PyObject *node_field_name_for_child(Node *self, PyObject *args) {
    long index;
    if (!PyArg_ParseTuple(args, "l:field_name_for_child", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "child index must be positive");
        return NULL;
    }
    if ((uint32_t)index >= ts_node_child_count(self->node)) {
        PyErr_SetString(PyExc_IndexError, "child index out of range");
        return NULL;
    }

    const char *field_name = ts_node_field_name_for_child(self->node, index);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

PyObject *node_field_name_for_named_child(Node *self, PyObject *args) {
    long index;
    if (!PyArg_ParseTuple(args, "l:field_name_for_named_child", &index)) {
        return NULL;
    }
    if (index < 0) {
        PyErr_SetString(PyExc_ValueError, "child index must be positive");
        return NULL;
    }
    if ((uint32_t)index >= ts_node_child_count(self->node)) {
        PyErr_SetString(PyExc_IndexError, "child index out of range");
        return NULL;
    }

    const char *field_name = ts_node_field_name_for_named_child(self->node, index);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

PyObject *node_descendant_for_byte_range(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "II:descendant_for_byte_range", &start_byte, &end_byte)) {
        return NULL;
    }
    TSNode descendant = ts_node_descendant_for_byte_range(self->node, start_byte, end_byte);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

PyObject *node_named_descendant_for_byte_range(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "II:named_descendant_for_byte_range", &start_byte, &end_byte)) {
        return NULL;
    }
    TSNode descendant = ts_node_named_descendant_for_byte_range(self->node, start_byte, end_byte);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

PyObject *node_descendant_for_point_range(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    TSPoint start, end;
    if (!PyArg_ParseTuple(args, "(II)(II):descendant_for_point_range", &start.row, &start.column,
                          &end.row, &end.column)) {
        return NULL;
    }

    TSNode descendant = ts_node_descendant_for_point_range(self->node, start, end);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

PyObject *node_named_descendant_for_point_range(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    TSPoint start, end;
    if (!PyArg_ParseTuple(args, "(II)(II):descendant_for_point_range", &start.row, &start.column,
                          &end.row, &end.column)) {
        return NULL;
    }

    TSNode descendant = ts_node_named_descendant_for_point_range(self->node, start, end);
    if (ts_node_is_null(descendant)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, descendant, self->tree);
}

PyObject *node_child_with_descendant(Node *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *descendant;
    if (!PyArg_ParseTuple(args, "O!:child_with_descendant", state->node_type, &descendant)) {
        return NULL;
    }

    TSNode child = ts_node_child_with_descendant(self->node, ((Node *)descendant)->node);
    if (ts_node_is_null(child)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, child, self->tree);
}

PyObject *node_get_id(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromVoidPtr((void *)self->node.id);
}

PyObject *node_get_kind_id(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromLong(ts_node_symbol(self->node));
}

PyObject *node_get_grammar_id(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromLong(ts_node_grammar_symbol(self->node));
}

PyObject *node_get_type(Node *self, void *Py_UNUSED(payload)) {
    return PyUnicode_FromString(ts_node_type(self->node));
}

PyObject *node_get_grammar_name(Node *self, void *Py_UNUSED(payload)) {
    return PyUnicode_FromString(ts_node_grammar_type(self->node));
}

PyObject *node_get_is_named(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_is_named(self->node));
}

PyObject *node_get_is_extra(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_is_extra(self->node));
}

PyObject *node_get_has_changes(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_has_changes(self->node));
}

PyObject *node_get_has_error(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_has_error(self->node));
}

PyObject *node_get_is_error(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_is_error(self->node));
}

PyObject *node_get_parse_state(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromLong(ts_node_parse_state(self->node));
}

PyObject *node_get_next_parse_state(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromLong(ts_node_next_parse_state(self->node));
}

PyObject *node_get_is_missing(Node *self, void *Py_UNUSED(payload)) {
    return PyBool_FromLong(ts_node_is_missing(self->node));
}

PyObject *node_get_start_byte(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_node_start_byte(self->node));
}

PyObject *node_get_end_byte(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_node_end_byte(self->node));
}

PyObject *node_get_byte_range(Node *self, void *Py_UNUSED(payload)) {
    PyObject *start_byte = PyLong_FromUnsignedLong(ts_node_start_byte(self->node));
    if (start_byte == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to determine start byte");
        return NULL;
    }
    PyObject *end_byte = PyLong_FromUnsignedLong(ts_node_end_byte(self->node));
    if (end_byte == NULL) {
        Py_DECREF(start_byte);
        PyErr_SetString(PyExc_RuntimeError, "Failed to determine end byte");
        return NULL;
    }
    PyObject *result = PyTuple_Pack(2, start_byte, end_byte);
    Py_DECREF(start_byte);
    Py_DECREF(end_byte);
    return result;
}

PyObject *node_get_range(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = GET_MODULE_STATE(self);
    Range *range = PyObject_New(Range, state->range_type);
    if (range == NULL) {
        return NULL;
    }
    range->range = (TSRange){
        .start_byte = ts_node_start_byte(self->node),
        .end_byte = ts_node_end_byte(self->node),
        .start_point = ts_node_start_point(self->node),
        .end_point = ts_node_end_point(self->node),
    };
    return PyObject_Init((PyObject *)range, state->range_type);
}

PyObject *node_get_start_point(Node *self, void *Py_UNUSED(payload)) {
    TSPoint point = ts_node_start_point(self->node);
    return point_new_internal(GET_MODULE_STATE(self), point);
}

PyObject *node_get_end_point(Node *self, void *Py_UNUSED(payload)) {
    TSPoint point = ts_node_end_point(self->node);
    return point_new_internal(GET_MODULE_STATE(self), point);
}

PyObject *node_get_children(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = GET_MODULE_STATE(self);
    if (self->children) {
        return Py_NewRef(self->children);
    }

    uint32_t length = ts_node_child_count(self->node);
    PyObject *result = PyList_New(length);
    if (result == NULL) {
        return NULL;
    }
    if (length > 0) {
        ts_tree_cursor_reset(&state->default_cursor, self->node);
        ts_tree_cursor_goto_first_child(&state->default_cursor);
        uint32_t i = 0;
        do {
            TSNode child = ts_tree_cursor_current_node(&state->default_cursor);
            PyObject *node = node_new_internal(state, child, self->tree);
            if (PyList_SetItem(result, i++, node) < 0) {
                Py_DECREF(result);
                return NULL;
            }
        } while (ts_tree_cursor_goto_next_sibling(&state->default_cursor));
    }
    self->children = Py_NewRef(result);
    return self->children;
}

PyObject *node_get_named_children(Node *self, void *payload) {
    PyObject *children = node_get_children(self, payload);
    if (children == NULL) {
        return NULL;
    }
    // children is retained by self->children
    Py_DECREF(children);

    uint32_t named_count = ts_node_named_child_count(self->node);
    PyObject *result = PyList_New(named_count);
    if (result == NULL) {
        return NULL;
    }

    uint32_t length = ts_node_child_count(self->node);
    for (uint32_t i = 0, j = 0; i < length; ++i) {
        PyObject *child = PyList_GetItem(self->children, i);
        if (ts_node_is_named(((Node *)child)->node)) {
            if (PyList_SetItem(result, j++, Py_NewRef(child)) < 0) {
                Py_DECREF(result);
                return NULL;
            }
        }
    }
    return result;
}

PyObject *node_get_child_count(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_node_child_count(self->node));
}

PyObject *node_get_named_child_count(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_node_named_child_count(self->node));
}

PyObject *node_get_parent(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = GET_MODULE_STATE(self);
    TSNode parent = ts_node_parent(self->node);
    if (ts_node_is_null(parent)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, parent, self->tree);
}

PyObject *node_get_next_sibling(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_sibling = ts_node_next_sibling(self->node);
    if (ts_node_is_null(next_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_sibling, self->tree);
}

PyObject *node_get_prev_sibling(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_sibling = ts_node_prev_sibling(self->node);
    if (ts_node_is_null(prev_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_sibling, self->tree);
}

PyObject *node_get_next_named_sibling(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode next_named_sibling = ts_node_next_named_sibling(self->node);
    if (ts_node_is_null(next_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, next_named_sibling, self->tree);
}

PyObject *node_get_prev_named_sibling(Node *self, void *Py_UNUSED(payload)) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    TSNode prev_named_sibling = ts_node_prev_named_sibling(self->node);
    if (ts_node_is_null(prev_named_sibling)) {
        Py_RETURN_NONE;
    }
    return node_new_internal(state, prev_named_sibling, self->tree);
}

PyObject *node_get_descendant_count(Node *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_node_descendant_count(self->node));
}

PyObject *node_get_text(Node *self, void *Py_UNUSED(payload)) {
    Tree *tree = (Tree *)self->tree;
    if (tree == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "This Node is not associated with a Tree");
        return NULL;
    }
    if (tree->source == Py_None || tree->source == NULL) {
        Py_RETURN_NONE;
    }

    PyObject *result = NULL;
    uint32_t start_offset = ts_node_start_byte(self->node),
             end_offset = ts_node_end_byte(self->node);

    // Case 1: source is a byte buffer
    if (!PyCallable_Check(tree->source)) {
        PyObject *start_byte = PyLong_FromUnsignedLong(start_offset),
                 *end_byte = PyLong_FromUnsignedLong(end_offset);
        PyObject *slice = PySlice_New(start_byte, end_byte, NULL);
        Py_XDECREF(start_byte);
        Py_XDECREF(end_byte);
        if (slice == NULL) {
            return NULL;
        }

        PyObject *node_mv = PyMemoryView_FromObject(tree->source);
        if (node_mv == NULL) {
            Py_DECREF(slice);
            return NULL;
        }

        PyObject *node_slice = PyObject_GetItem(node_mv, slice);
        Py_DECREF(slice);
        Py_DECREF(node_mv);
        if (node_slice == NULL) {
            return NULL;
        }

        result = PyBytes_FromObject(node_slice);
        Py_DECREF(node_slice);
    } else {
        // Case 2: source is a callable
        PyObject *collected_bytes = PyByteArray_FromStringAndSize(NULL, 0);
        if (collected_bytes == NULL) {
            return NULL;
        }
        TSPoint start_point = ts_node_start_point(self->node);
        TSPoint current_point = start_point;

        for (size_t current_offset = start_offset; current_offset < end_offset;) {
            PyObject *byte_offset_obj = PyLong_FromSize_t(current_offset);
            if (!byte_offset_obj) {
                Py_DECREF(collected_bytes);
                return NULL;
            }
            PyObject *position_obj = point_new_internal(GET_MODULE_STATE(self), current_point);
            if (!position_obj) {
                Py_DECREF(byte_offset_obj);
                Py_DECREF(collected_bytes);
                return NULL;
            }

            PyObject *args = PyTuple_Pack(2, byte_offset_obj, position_obj);
            Py_XDECREF(byte_offset_obj);
            Py_XDECREF(position_obj);

            PyObject *rv = PyObject_Call(tree->source, args, NULL);
            Py_XDECREF(args);

            PyObject *rv_bytearray = PyByteArray_FromObject(rv);
            if (rv_bytearray == NULL) {
                Py_DECREF(collected_bytes);
                Py_XDECREF(rv);
                return NULL;
            }

            PyObject *new_collected_bytes = PyByteArray_Concat(collected_bytes, rv_bytearray);
            Py_DECREF(rv_bytearray);
            Py_DECREF(collected_bytes);
            if (new_collected_bytes == NULL) {
                Py_XDECREF(rv);
                return NULL;
            }
            collected_bytes = new_collected_bytes;

            size_t bytes_read = (size_t)PyBytes_Size(rv);
            const char *rv_str = PyBytes_AsString(rv);
            for (size_t i = 0; i < bytes_read; ++i) {
                if (rv_str[i] == '\n') {
                    ++current_point.row;
                    current_point.column = 0;
                } else {
                    ++current_point.column;
                }
            }
            current_offset += bytes_read;
        }

        PyObject *start_byte = PyLong_FromSize_t(0);
        PyObject *end_byte = PyLong_FromSize_t(end_offset - start_offset);
        PyObject *slice = PySlice_New(start_byte, end_byte, NULL);
        Py_XDECREF(start_byte);
        Py_XDECREF(end_byte);
        if (slice == NULL) {
            Py_DECREF(collected_bytes);
            return NULL;
        }
        result = PyObject_GetItem(collected_bytes, slice);
        Py_DECREF(slice);
        Py_DECREF(collected_bytes);
    }
    PyObject *bytes_result = PyBytes_FromObject(result);
    Py_DECREF(result);
    return bytes_result;
}

Py_hash_t node_hash(Node *self) {
    // __eq__ and __hash__ must be compatible. As __eq__ is defined by
    // ts_node_eq, which in turn checks the tree pointer and the node
    // id, we can use those values to compute the hash.
    Py_hash_t id = (Py_hash_t)self->node.id, tree = (Py_hash_t)self->node.tree;
    return id == tree ? id : id ^ tree;
}

PyDoc_STRVAR(node_walk_doc, "walk(self, /)\n--\n\n"
                            "Create a new :class:`TreeCursor` starting from this node.");
PyDoc_STRVAR(node_edit_doc,
             "edit(self, /, start_byte, old_end_byte, new_end_byte, start_point, "
             "old_end_point, new_end_point)\n--\n\n"
             "Edit this node to keep it in-sync with source code that has been edited." DOC_NOTE
             "This method is only rarely needed. When you edit a syntax tree via "
             ":meth:`Tree.edit`, all of the nodes that you retrieve from the tree afterwards "
             "will already reflect the edit. You only need to use this when you have a specific "
             ":class:`Node` instance that you want to keep and continue to use after an edit.");
PyDoc_STRVAR(node_child_doc,
             "child(self, index, /)\n--\n\n"
             "Get this node's child at the given index, where ``0`` represents the first "
             "child." DOC_CAUTION "This method is fairly fast, but its cost is technically "
             "``log(i)``, so if you might be iterating over a long list of children, "
             "you should use :attr:`children` or :meth:`walk` instead.");
PyDoc_STRVAR(node_named_child_doc,
             "named_child(self, index, /)\n--\n\n"
             "Get this node's *named* child at the given index, where ``0`` represents the first "
             "child." DOC_CAUTION "This method is fairly fast, but its cost is technically "
             "``log(i)``, so if you might be iterating over a long list of children, "
             "you should use :attr:`children` or :meth:`walk` instead.");
PyDoc_STRVAR(node_first_child_for_byte_doc,
             "first_child_for_byte(self, byte, /)\n--\n\n"
             "Get the node's first child that contains or starts after the given byte offset.");
PyDoc_STRVAR(node_first_named_child_for_byte_doc,
             "first_named_child_for_byte(self, byte, /)\n--\n\n"
             "Get the node's first *named* child that contains "
             "or starts after the given byte offset.");
PyDoc_STRVAR(node_child_by_field_id_doc,
             "child_by_field_id(self, id, /)\n--\n\n"
             "Get the first child with the given numerical field id." DOC_HINT
             "You can convert a field name to an id using "
             ":meth:`Language.field_id_for_name`." DOC_SEE_ALSO ":meth:`child_by_field_name`");
PyDoc_STRVAR(node_children_by_field_id_doc,
             "children_by_field_id(self, id, /)\n--\n\n"
             "Get a list of children with the given numerical field id." DOC_SEE_ALSO
             ":meth:`children_by_field_name`");
PyDoc_STRVAR(node_child_by_field_name_doc, "child_by_field_name(self, name, /)\n--\n\n"
                                           "Get the first child with the given field name.");
PyDoc_STRVAR(node_children_by_field_name_doc, "children_by_field_name(self, name, /)\n--\n\n"
                                              "Get a list of children with the given field name.");
PyDoc_STRVAR(node_field_name_for_child_doc,
             "field_name_for_child(self, child_index, /)\n--\n\n"
             "Get the field name of this node's child at the given index.");
PyDoc_STRVAR(node_field_name_for_named_child_doc,
             "field_name_for_child(self, child_index, /)\n--\n\n"
             "Get the field name of this node's *named* child at the given index.");
PyDoc_STRVAR(node_descendant_for_byte_range_doc,
             "descendant_for_byte_range(self, start_byte, end_byte, /)\n--\n\n"
             "Get the smallest node within this node that spans the given byte range.");
PyDoc_STRVAR(node_named_descendant_for_byte_range_doc,
             "named_descendant_for_byte_range(self, start_byte, end_byte, /)\n--\n\n"
             "Get the smallest *named* node within this node that spans the given byte range.");
PyDoc_STRVAR(node_descendant_for_point_range_doc,
             "descendant_for_point_range(self, start_point, end_point, /)\n--\n\n"
             "Get the smallest node within this node that spans the given point range.");
PyDoc_STRVAR(node_named_descendant_for_point_range_doc,
             "named_descendant_for_point_range(self, start_point, end_point, /)\n--\n\n"
             "Get the smallest *named* node within this node that spans the given point range.");
PyDoc_STRVAR(node_child_with_descendant_doc, "child_with_descendant(self, descendant, /)\n--\n\n"
                                             "Get the node that contains the given descendant.");

static PyMethodDef node_methods[] = {
    {
        .ml_name = "walk",
        .ml_meth = (PyCFunction)node_walk,
        .ml_flags = METH_NOARGS,
        .ml_doc = node_walk_doc,
    },
    {
        .ml_name = "edit",
        .ml_meth = (PyCFunction)node_edit,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = node_edit_doc,
    },
    {
        .ml_name = "child",
        .ml_meth = (PyCFunction)node_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_child_doc,
    },
    {
        .ml_name = "named_child",
        .ml_meth = (PyCFunction)node_named_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_named_child_doc,
    },
    {
        .ml_name = "first_child_for_byte",
        .ml_meth = (PyCFunction)node_first_child_for_byte,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_first_child_for_byte_doc,
    },
    {
        .ml_name = "first_named_child_for_byte",
        .ml_meth = (PyCFunction)node_first_named_child_for_byte,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_first_named_child_for_byte_doc,
    },
    {
        .ml_name = "child_by_field_id",
        .ml_meth = (PyCFunction)node_child_by_field_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_child_by_field_id_doc,
    },
    {
        .ml_name = "child_by_field_name",
        .ml_meth = (PyCFunction)node_child_by_field_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_child_by_field_name_doc,
    },
    {
        .ml_name = "children_by_field_id",
        .ml_meth = (PyCFunction)node_children_by_field_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_children_by_field_id_doc,
    },
    {
        .ml_name = "children_by_field_name",
        .ml_meth = (PyCFunction)node_children_by_field_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_children_by_field_name_doc,
    },
    {
        .ml_name = "field_name_for_child",
        .ml_meth = (PyCFunction)node_field_name_for_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_field_name_for_child_doc,
    },
    {
        .ml_name = "field_name_for_named_child",
        .ml_meth = (PyCFunction)node_field_name_for_named_child,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_field_name_for_named_child_doc,
    },
    {
        .ml_name = "descendant_for_byte_range",
        .ml_meth = (PyCFunction)node_descendant_for_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_descendant_for_byte_range_doc,
    },
    {
        .ml_name = "named_descendant_for_byte_range",
        .ml_meth = (PyCFunction)node_named_descendant_for_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_named_descendant_for_byte_range_doc,
    },
    {
        .ml_name = "descendant_for_point_range",
        .ml_meth = (PyCFunction)node_descendant_for_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_descendant_for_point_range_doc,
    },
    {
        .ml_name = "named_descendant_for_point_range",
        .ml_meth = (PyCFunction)node_named_descendant_for_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_named_descendant_for_point_range_doc,
    },
    {
        .ml_name = "child_with_descendant",
        .ml_meth = (PyCFunction)node_child_with_descendant,
        .ml_flags = METH_VARARGS,
        .ml_doc = node_child_with_descendant_doc,
    },
    {NULL},
};

static PyGetSetDef node_accessors[] = {
    {"id", (getter)node_get_id, NULL,
     PyDoc_STR("This node's numerical id." DOC_NOTE
               "Within a given syntax tree, no two nodes have the same id. However, if a new tree "
               "is created based on an older tree, and a node from the old tree is reused in the "
               "process, then that node will have the same id in both trees."),
     NULL},
    {"kind_id", (getter)node_get_kind_id, NULL, PyDoc_STR("This node's type as a numerical id."),
     NULL},
    {"grammar_id", (getter)node_get_grammar_id, NULL,
     PyDoc_STR("This node's type as a numerical id as it appears in the grammar ignoring aliases."),
     NULL},
    {"grammar_name", (getter)node_get_grammar_name, NULL,
     PyDoc_STR("This node's symbol name as it appears in the grammar ignoring aliases."), NULL},
    {"type", (getter)node_get_type, NULL, PyDoc_STR("This node's type as a string."), NULL},
    {"is_named", (getter)node_get_is_named, NULL,
     PyDoc_STR("Check if this node is _named_.\n\nNamed nodes correspond to named rules in the "
               "grammar, whereas *anonymous* nodes correspond to string literals in the grammar."),
     NULL},
    {"is_extra", (getter)node_get_is_extra, NULL,
     PyDoc_STR("Check if this node is _extra_.\n\nExtra nodes represent things which are not "
               "required by the grammar but can appear anywhere (e.g. whitespace)."),
     NULL},
    {"has_changes", (getter)node_get_has_changes, NULL,
     PyDoc_STR("Check if this node has been edited."), NULL},
    {"has_error", (getter)node_get_has_error, NULL,
     PyDoc_STR("Check if this node represents a syntax error or contains any syntax errors "
               "anywhere within it."),
     NULL},
    {"is_error", (getter)node_get_is_error, NULL,
     PyDoc_STR("Check if this node represents a syntax error.\n\nSyntax errors represent parts of "
               "the code that could not be incorporated into a valid syntax tree."),
     NULL},
    {"parse_state", (getter)node_get_parse_state, NULL, PyDoc_STR("This node's parse state."),
     NULL},
    {"next_parse_state", (getter)node_get_next_parse_state, NULL,
     PyDoc_STR("The parse state after this node."), NULL},
    {"is_missing", (getter)node_get_is_missing, NULL,
     PyDoc_STR("Check if this node is _missing_.\n\nMissing nodes are inserted by the parser in "
               "order to recover from certain kinds of syntax errors."),
     NULL},
    {"start_byte", (getter)node_get_start_byte, NULL,
     PyDoc_STR("The byte offset where this node starts."), NULL},
    {"end_byte", (getter)node_get_end_byte, NULL,
     PyDoc_STR("The byte offset where this node ends."), NULL},
    {"byte_range", (getter)node_get_byte_range, NULL,
     PyDoc_STR("The byte range of source code that this node represents, in terms of bytes."),
     NULL},
    {"range", (getter)node_get_range, NULL,
     PyDoc_STR("The range of source code that this node represents."), NULL},
    {"start_point", (getter)node_get_start_point, NULL, PyDoc_STR("This node's start point"), NULL},
    {"end_point", (getter)node_get_end_point, NULL, PyDoc_STR("This node's end point."), NULL},
    {"children", (getter)node_get_children, NULL,
     PyDoc_STR("This node's children." DOC_NOTE
               "If you're walking the tree recursively, you may want to use :meth:`walk` instead."),
     NULL},
    {"child_count", (getter)node_get_child_count, NULL,
     PyDoc_STR("This node's number of children."), NULL},
    {"named_children", (getter)node_get_named_children, NULL,
     PyDoc_STR("This node's _named_ children."), NULL},
    {"named_child_count", (getter)node_get_named_child_count, NULL,
     PyDoc_STR("This node's number of _named_ children."), NULL},
    {"parent", (getter)node_get_parent, NULL, PyDoc_STR("This node's immediate parent."), NULL},
    {"next_sibling", (getter)node_get_next_sibling, NULL, PyDoc_STR("This node's next sibling."),
     NULL},
    {"prev_sibling", (getter)node_get_prev_sibling, NULL,
     PyDoc_STR("This node's previous sibling."), NULL},
    {"next_named_sibling", (getter)node_get_next_named_sibling, NULL,
     PyDoc_STR("This node's next named sibling."), NULL},
    {"prev_named_sibling", (getter)node_get_prev_named_sibling, NULL,
     PyDoc_STR("This node's previous named sibling."), NULL},
    {"descendant_count", (getter)node_get_descendant_count, NULL,
     PyDoc_STR("This node's number of descendants, including the node itself."), NULL},
    {"text", (getter)node_get_text, NULL,
     PyDoc_STR("The text of the node, if the tree has not been edited"), NULL},
    {NULL},
};

static PyType_Slot node_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("A single node within a syntax ``Tree``.")},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, node_dealloc},
    {Py_tp_repr, node_repr},
    {Py_tp_str, node_str},
    {Py_tp_richcompare, node_compare},
    {Py_tp_hash, node_hash},
    {Py_tp_methods, node_methods},
    {Py_tp_getset, node_accessors},
    {0, NULL},
};

PyType_Spec node_type_spec = {
    .name = "tree_sitter.Node",
    .basicsize = sizeof(Node),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = node_type_slots,
};
