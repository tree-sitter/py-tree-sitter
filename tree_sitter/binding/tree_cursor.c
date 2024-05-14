#include "types.h"

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

void tree_cursor_dealloc(TreeCursor *self) {
    ts_tree_cursor_delete(&self->cursor);
    Py_XDECREF(self->node);
    Py_XDECREF(self->tree);
    Py_TYPE(self)->tp_free(self);
}

PyObject *tree_cursor_get_node(TreeCursor *self, void *Py_UNUSED(payload)) {
    if (self->node == NULL) {
        TSNode current_node = ts_tree_cursor_current_node(&self->cursor);
        if (ts_node_is_null(current_node)) {
            Py_RETURN_NONE;
        }
        ModuleState *state = GET_MODULE_STATE(self);
        self->node = node_new_internal(state, current_node, self->tree);
    }
    Py_INCREF(self->node);
    return self->node;
}

PyObject *tree_cursor_get_field_id(TreeCursor *self, void *Py_UNUSED(payload)) {
    TSFieldId field_id = ts_tree_cursor_current_field_id(&self->cursor);
    if (field_id == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(field_id);
}

PyObject *tree_cursor_get_field_name(TreeCursor *self, void *Py_UNUSED(payload)) {
    const char *field_name = ts_tree_cursor_current_field_name(&self->cursor);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

PyObject *tree_cursor_get_depth(TreeCursor *self, void *Py_UNUSED(args)) {
    uint32_t depth = ts_tree_cursor_current_depth(&self->cursor);
    return PyLong_FromUnsignedLong(depth);
}

PyObject *tree_cursor_get_descendant_index(TreeCursor *self, void *Py_UNUSED(payload)) {
    uint32_t index = ts_tree_cursor_current_descendant_index(&self->cursor);
    return PyLong_FromUnsignedLong(index);
}

PyObject *tree_cursor_goto_first_child(TreeCursor *self, PyObject *Py_UNUSED(args)) {
    bool result = ts_tree_cursor_goto_first_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_last_child(TreeCursor *self, PyObject *Py_UNUSED(args)) {
    bool result = ts_tree_cursor_goto_last_child(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_parent(TreeCursor *self, PyObject *Py_UNUSED(args)) {
    bool result = ts_tree_cursor_goto_parent(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_next_sibling(TreeCursor *self, PyObject *Py_UNUSED(args)) {
    bool result = ts_tree_cursor_goto_next_sibling(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_previous_sibling(TreeCursor *self, PyObject *Py_UNUSED(args)) {
    bool result = ts_tree_cursor_goto_previous_sibling(&self->cursor);
    if (result) {
        Py_XDECREF(self->node);
        self->node = NULL;
    }
    return PyBool_FromLong(result);
}

PyObject *tree_cursor_goto_descendant(TreeCursor *self, PyObject *args) {
    uint32_t index;
    if (!PyArg_ParseTuple(args, "I:goto_descendant", &index)) {
        return NULL;
    }
    ts_tree_cursor_goto_descendant(&self->cursor, index);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

PyObject *tree_cursor_goto_first_child_for_byte(TreeCursor *self, PyObject *args) {
    uint32_t byte;
    if (!PyArg_ParseTuple(args, "I:goto_first_child_for_byte", &byte)) {
        return NULL;
    }

    int64_t result = ts_tree_cursor_goto_first_child_for_byte(&self->cursor, byte);
    if (result == -1) {
        Py_RETURN_FALSE;
    }
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_TRUE;
}

PyObject *tree_cursor_goto_first_child_for_point(TreeCursor *self, PyObject *args) {
    TSPoint point;
    if (!PyArg_ParseTuple(args, "(II):goto_first_child_for_point", &point.row, &point.column)) {
        return NULL;
    }

    int64_t result = ts_tree_cursor_goto_first_child_for_point(&self->cursor, point);
    if (result == -1) {
        Py_RETURN_FALSE;
    }
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_TRUE;
}

PyObject *tree_cursor_reset(TreeCursor *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *node_obj;
    if (!PyArg_ParseTuple(args, "O!:reset", state->node_type, &node_obj)) {
        return NULL;
    }

    Node *node = (Node *)node_obj;
    ts_tree_cursor_reset(&self->cursor, node->node);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

PyObject *tree_cursor_reset_to(TreeCursor *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *cursor_obj;
    if (!PyArg_ParseTuple(args, "O!:reset_to", state->tree_cursor_type, &cursor_obj)) {
        return NULL;
    }

    TreeCursor *cursor = (TreeCursor *)cursor_obj;
    ts_tree_cursor_reset_to(&self->cursor, &cursor->cursor);
    Py_XDECREF(self->node);
    self->node = NULL;
    Py_RETURN_NONE;
}

PyObject *tree_cursor_copy(PyObject *self, PyObject *Py_UNUSED(args)) {
    ModuleState *state = GET_MODULE_STATE(self);
    TreeCursor *origin = (TreeCursor *)self;
    TreeCursor *copied = PyObject_New(TreeCursor, state->tree_cursor_type);
    if (copied == NULL) {
        return NULL;
    }

    Py_INCREF(origin->tree);
    copied->tree = origin->tree;
    copied->cursor = ts_tree_cursor_copy(&origin->cursor);
    return PyObject_Init((PyObject *)copied, state->tree_cursor_type);
}

PyDoc_STRVAR(tree_cursor_goto_first_child_doc,
             "goto_first_child(self, /)\n--\n\n"
             "Move this cursor to the first child of its current node." DOC_RETURNS "``True`` "
             "if the cursor successfully moved, or ``False`` if there were no children.");
PyDoc_STRVAR(
    tree_cursor_goto_last_child_doc,
    "goto_last_child(self, /)\n--\n\n"
    "Move this cursor to the last child of its current node." DOC_RETURNS "``True`` "
    "if the cursor successfully moved, or ``False`` if there were no children." DOC_ATTENTION
    "This method may be slower than :meth:`goto_first_child` because it needs "
    "to iterate through all the children to compute the child's position.");
PyDoc_STRVAR(tree_cursor_goto_parent_doc,
             "goto_parent(self, /)\n--\n\n"
             "Move this cursor to the parent of its current node." DOC_RETURNS "``True`` "
             "if the cursor successfully moved, or ``False`` if there was no parent node "
             "(i.e. the cursor was already on the root node).");
PyDoc_STRVAR(tree_cursor_goto_next_sibling_doc,
             "goto_next_sibling(self, /)\n--\n\n"
             "Move this cursor to the next sibling of its current node." DOC_RETURNS "``True`` "
             "if the cursor successfully moved, or ``False`` if there was no next sibling.");
PyDoc_STRVAR(tree_cursor_goto_previous_sibling_doc,
             "goto_previous_sibling(self, /)\n--\n\n"
             "Move this cursor to the previous sibling of its current node." DOC_RETURNS
             "``True`` if the cursor successfully moved, or ``False`` if there was no previous "
             "sibling." DOC_ATTENTION
             "This method may be slower than :meth:`goto_next_sibling` due to how node positions "
             "are stored.\nIn the worst case, this will need to iterate through all the children "
             "up to the previous sibling node to recalculate its position.");
PyDoc_STRVAR(
    tree_cursor_goto_descendant_doc,
    "goto_descendant(self, index, /)\n--\n\n"
    "Move the cursor to the node that is the n-th descendant of the original node that the "
    "cursor was constructed with, where ``0`` represents the original node itself.");
PyDoc_STRVAR(tree_cursor_goto_first_child_for_byte_doc,
             "goto_first_child_for_byte(self, byte, /)\n--\n\n"
             "Move this cursor to the first child of its current node that extends beyond the "
             "given byte offset." DOC_RETURNS
             "``True`` if the child node was found, ``False`` otherwise.");
PyDoc_STRVAR(tree_cursor_goto_first_child_for_point_doc,
             "goto_first_child_for_point(self, point, /)\n--\n\n"
             "Move this cursor to the first child of its current node that extends beyond the "
             "given row/column point.\n\n" DOC_RETURNS
             "``True`` if the child node was found, ``False`` otherwise.");
PyDoc_STRVAR(tree_cursor_reset_doc, "reset(self, node, /)\n--\n\n"
                                    "Re-initialize the cursor to start at the original node "
                                    "that it was constructed with.");
PyDoc_STRVAR(tree_cursor_reset_to_doc,
             "reset_to(self, cursor, /)\n--\n\n"
             "Re-initialize the cursor to the same position as another cursor.\n\n"
             "Unlike :meth:`reset`, this will not lose parent information and allows reusing "
             "already created cursors.");
PyDoc_STRVAR(tree_cursor_copy_doc, "copy(self, /)\n--\n\n"
                                   "Create an independent copy of the cursor.");
PyDoc_STRVAR(tree_cursor_copy2_doc, "__copy__(self, /)\n--\n\n"
                                    "Use :func:`copy.copy` to create a copy of the cursor.");

static PyMethodDef tree_cursor_methods[] = {
    {
        .ml_name = "goto_first_child",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_goto_first_child_doc,
    },
    {
        .ml_name = "goto_last_child",
        .ml_meth = (PyCFunction)tree_cursor_goto_last_child,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_goto_last_child_doc,
    },
    {
        .ml_name = "goto_parent",
        .ml_meth = (PyCFunction)tree_cursor_goto_parent,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_goto_parent_doc,
    },
    {
        .ml_name = "goto_next_sibling",
        .ml_meth = (PyCFunction)tree_cursor_goto_next_sibling,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_goto_next_sibling_doc,
    },
    {
        .ml_name = "goto_previous_sibling",
        .ml_meth = (PyCFunction)tree_cursor_goto_previous_sibling,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_goto_previous_sibling_doc,
    },
    {
        .ml_name = "goto_descendant",
        .ml_meth = (PyCFunction)tree_cursor_goto_descendant,
        .ml_flags = METH_VARARGS,
        .ml_doc = tree_cursor_goto_descendant_doc,
    },
    {
        .ml_name = "goto_first_child_for_byte",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child_for_byte,
        .ml_flags = METH_VARARGS,
        .ml_doc = tree_cursor_goto_first_child_for_byte_doc,
    },
    {
        .ml_name = "goto_first_child_for_point",
        .ml_meth = (PyCFunction)tree_cursor_goto_first_child_for_point,
        .ml_flags = METH_VARARGS,
        .ml_doc = tree_cursor_goto_first_child_for_point_doc,
    },
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)tree_cursor_reset,
        .ml_flags = METH_VARARGS,
        .ml_doc = tree_cursor_reset_doc,
    },
    {
        .ml_name = "reset_to",
        .ml_meth = (PyCFunction)tree_cursor_reset_to,
        .ml_flags = METH_VARARGS,
        .ml_doc = tree_cursor_reset_to_doc,
    },
    {
        .ml_name = "copy",
        .ml_meth = (PyCFunction)tree_cursor_copy,
        .ml_flags = METH_NOARGS,
        .ml_doc = tree_cursor_copy_doc,
    },
    {.ml_name = "__copy__",
     .ml_meth = (PyCFunction)tree_cursor_copy,
     .ml_flags = METH_NOARGS,
     .ml_doc = tree_cursor_copy2_doc},
    {NULL},
};

static PyGetSetDef tree_cursor_accessors[] = {
    {"node", (getter)tree_cursor_get_node, NULL, "The current node.", NULL},
    {"descendant_index", (getter)tree_cursor_get_descendant_index, NULL,
     PyDoc_STR("The index of the cursor's current node out of all of the descendants of the "
               "original node that the cursor was constructed with.\n\n"),
     NULL},
    {"field_id", (getter)tree_cursor_get_field_id, NULL,
     PyDoc_STR("The numerical field id of this tree cursor's current node, if available."), NULL},
    {"field_name", (getter)tree_cursor_get_field_name, NULL,
     PyDoc_STR("The field name of this tree cursor's current node, if available."), NULL},
    {"depth", (getter)tree_cursor_get_depth, NULL,
     PyDoc_STR("The depth of the cursor's current node relative to the original node that it was "
               "constructed with."),
     NULL},
    {NULL},
};

static PyType_Slot tree_cursor_type_slots[] = {
    {Py_tp_doc,
     PyDoc_STR("A class for walking a syntax :class:`Tree` efficiently." DOC_IMPORTANT
               "The cursor can only walk into children of the node that it started from.")},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, tree_cursor_dealloc},
    {Py_tp_methods, tree_cursor_methods},
    {Py_tp_getset, tree_cursor_accessors},
    {0, NULL},
};

PyType_Spec tree_cursor_type_spec = {
    .name = "tree_sitter.TreeCursor",
    .basicsize = sizeof(TreeCursor),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = tree_cursor_type_slots,
};
