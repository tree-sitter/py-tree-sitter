#include "tree.h"
#include "node.h"

void tree_dealloc(Tree *self) {
    ts_tree_delete(self->tree);
    Py_XDECREF(self->source);
    Py_TYPE(self)->tp_free(self);
}

PyObject *tree_get_root_node(Tree *self, void *Py_UNUSED(payload)) {
    ModuleState *state = GET_MODULE_STATE(self);
    TSNode node = ts_tree_root_node(self->tree);
    return node_new_internal(state, node, (PyObject *)self);
}

PyObject *tree_get_text(Tree *self, void *Py_UNUSED(payload)) {
    if (REPLACE("Tree.text", "Tree.root_node.text") < 0) {
        return NULL;
    }

    PyObject *source = self->source;
    if (source == NULL) {
        Py_RETURN_NONE;
    }
    Py_INCREF(source);
    return source;
}

PyObject *tree_root_node_with_offset(Tree *self, PyObject *args) {
    uint32_t offset_bytes;
    TSPoint offset_extent;
    if (!PyArg_ParseTuple(args, "I(II):root_node_with_offset", &offset_bytes, &offset_extent.row,
                          &offset_extent.column)) {
        return NULL;
    }

    ModuleState *state = GET_MODULE_STATE(self);
    TSNode node = ts_tree_root_node_with_offset(self->tree, offset_bytes, offset_extent);
    return node_new_internal(state, node, (PyObject *)self);
}

PyObject *tree_walk(Tree *self, PyObject *Py_UNUSED(args)) {
    ModuleState *state = GET_MODULE_STATE(self);
    TreeCursor *tree_cursor = PyObject_New(TreeCursor, state->tree_cursor_type);
    if (tree_cursor == NULL) {
        return NULL;
    }
    tree_cursor->cursor = ts_tree_cursor_new(ts_tree_root_node(self->tree));
    Py_INCREF(self);
    tree_cursor->tree = (PyObject *)self;
    return PyObject_Init((PyObject *)tree_cursor, state->tree_cursor_type);
}

PyObject *tree_edit(Tree *self, PyObject *args, PyObject *kwargs) {
    unsigned start_byte, start_row, start_column;
    unsigned old_end_byte, old_end_row, old_end_column;
    unsigned new_end_byte, new_end_row, new_end_column;

    char *keywords[] = {
        "start_byte",    "old_end_byte",  "new_end_byte", "start_point",
        "old_end_point", "new_end_point", NULL,
    };

    int ok = PyArg_ParseTupleAndKeywords(
        args, kwargs, "III(II)(II)(II):edit", keywords, &start_byte, &old_end_byte, &new_end_byte,
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

PyObject *tree_changed_ranges(Tree *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *new_tree;
    char *keywords[] = {"new_tree", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:changed_ranges", keywords, state->tree_type,
                                     &new_tree)) {
        return NULL;
    }

    uint32_t length = 0;
    TSTree *tree = ((Tree *)new_tree)->tree;
    TSRange *ranges = ts_tree_get_changed_ranges(self->tree, tree, &length);

    PyObject *result = PyList_New(length);
    if (result == NULL) {
        return NULL;
    }
    for (unsigned i = 0; i < length; ++i) {
        Range *range = PyObject_New(Range, state->range_type);
        if (range == NULL) {
            return NULL;
        }
        range->range = ranges[i];
        PyList_SetItem(result, i, PyObject_Init((PyObject *)range, state->range_type));
    }

    PyMem_Free(ranges);
    return result;
}

PyObject *tree_get_included_ranges(Tree *self, PyObject *Py_UNUSED(args)) {
    ModuleState *state = GET_MODULE_STATE(self);
    uint32_t length = 0;
    TSRange *ranges = ts_tree_included_ranges(self->tree, &length);

    PyObject *result = PyList_New(length);
    if (result == NULL) {
        return NULL;
    }
    for (unsigned i = 0; i < length; ++i) {
        Range *range = PyObject_New(Range, state->range_type);
        if (range == NULL) {
            return NULL;
        }
        range->range = ranges[i];
        PyList_SetItem(result, i, PyObject_Init((PyObject *)range, state->range_type));
    }

    PyMem_Free(ranges);
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
    {Py_tp_doc, "A syntax tree"},   {Py_tp_new, NULL},
    {Py_tp_dealloc, tree_dealloc},  {Py_tp_methods, tree_methods},
    {Py_tp_getset, tree_accessors}, {0, NULL},
};

PyType_Spec tree_type_spec = {
    .name = "tree_sitter.Tree",
    .basicsize = sizeof(Tree),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = tree_type_slots,
};
