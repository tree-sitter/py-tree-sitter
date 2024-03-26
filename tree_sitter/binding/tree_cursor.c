#include "tree_cursor.h"

#include "node.h"

PyObject *tree_cursor_new_internal(ModuleState *state, TSNode node, PyObject *tree) {
    TreeCursor *self = (TreeCursor *)state->tree_cursor_type->tp_alloc(state->tree_cursor_type, 0);
    if (self != NULL) {
        self->cursor = ts_tree_cursor_new(node);
        Py_INCREF(tree);
        self->tree = tree;
    }
    return (PyObject *)self;
}

void tree_cursor_dealloc(TreeCursor *self) {
    ts_tree_cursor_delete(&self->cursor);
    Py_XDECREF(self->node);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject *tree_cursor_get_node(TreeCursor *self, void *payload) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    if (!self->node) {
        self->node =
            node_new_internal(state, ts_tree_cursor_current_node(&self->cursor), self->tree);
    }

    Py_INCREF(self->node);
    return self->node;
}

PyObject *tree_cursor_current_field_id(TreeCursor *self, PyObject *args) {
    uint32_t field_id = ts_tree_cursor_current_field_id(&self->cursor);
    if (field_id == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(field_id);
}

PyObject *tree_cursor_current_field_name(TreeCursor *self, PyObject *args) {
    const char *field_name = ts_tree_cursor_current_field_name(&self->cursor);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

PyObject *tree_cursor_current_depth(TreeCursor *self, PyObject *args) {
    uint32_t depth = ts_tree_cursor_current_depth(&self->cursor);
    return PyLong_FromUnsignedLong(depth);
}

PyObject *tree_cursor_current_descendant_index(TreeCursor *self, PyObject *Py_UNUSED(payload)) {
    uint32_t index = ts_tree_cursor_current_descendant_index(&self->cursor);
    return PyLong_FromUnsignedLong(index);
}

PyObject *tree_cursor_goto_first_child(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_first_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_last_child(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_last_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_parent(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_parent(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_next_sibling(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_next_sibling(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_previous_sibling(TreeCursor *self, PyObject *args) {
    bool result = ts_tree_cursor_goto_previous_sibling(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_descendant(TreeCursor *self, PyObject *args) {
    uint32_t index;
    if (!PyArg_ParseTuple(args, "I", &index)) {
        return NULL;
    }
    ts_tree_cursor_goto_descendant(&self->cursor, index);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

PyObject *tree_cursor_goto_first_child_for_byte(TreeCursor *self, PyObject *args) {
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

PyObject *tree_cursor_goto_first_child_for_point(TreeCursor *self, PyObject *args) {
    uint32_t row, column;
    if (!PyArg_ParseTuple(args, "(II):goto_first_child_for_point", &row, &column)) {
        if (PyArg_ParseTuple(args, "II:goto_first_child_for_point", &row, &column)) {
            PyErr_Clear();
            if (REPLACE("TreeCursor.goto_first_child_for_point(row, col)",
                        "TreeCursor.goto_first_child_for_point(point)") < 0) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    bool result = ts_tree_cursor_goto_first_child_for_point(&self->cursor, (TSPoint){row, column});
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_reset(TreeCursor *self, PyObject *args) {
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

PyObject *tree_cursor_reset_to(TreeCursor *self, PyObject *args) {
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

PyObject *tree_cursor_copy(PyObject *self) {
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

static PyMethodDef tree_cursor_methods[] = {
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
    {.ml_name = "__copy__",
     .ml_meth = (PyCFunction)tree_cursor_copy,
     .ml_flags = METH_NOARGS,
     .ml_doc = NULL},
    {NULL},
};

static PyGetSetDef tree_cursor_accessors[] = {
    {"node", (getter)tree_cursor_get_node, NULL, "The current node.", NULL},
    {
        "descendant_index",
        (getter)tree_cursor_current_descendant_index,
        NULL,
        "current_descendant_index()\n--\n\n\
			   Get the index of the cursor's current node out of all of the descendants of the original node.",
        NULL,
    },
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

PyType_Spec tree_cursor_type_spec = {
    .name = "tree_sitter.TreeCursor",
    .basicsize = sizeof(TreeCursor),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = tree_cursor_type_slots,
};
