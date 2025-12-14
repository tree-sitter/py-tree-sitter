#include "types.h"

PyObject *point_new_internal(ModuleState *state, TSPoint point) {
    PyObject *self = PyTuple_New(2);
    if (self == NULL) {
        return NULL;
    }
    PyTuple_SET_ITEM(self, 0, PyLong_FromUnsignedLong(point.row));
    PyTuple_SET_ITEM(self, 1, PyLong_FromUnsignedLong(point.column));
    return PyObject_Init(self, state->point_type);
}

PyObject *point_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    uint32_t row, column;
    char *keywords[] = {"row", "column", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "II:__new__", keywords, &row, &column)) {
        return NULL;
    }

    PyObject *row_obj = PyLong_FromUnsignedLong(row), *col_obj = PyLong_FromUnsignedLong(column);
    PyObject *self = PyTuple_Pack(2, row_obj, col_obj);
    if (!self) {
        return NULL;
    }
    Py_SET_TYPE(self, type);
    return self;
}

PyObject *point_repr(PyObject *self) {
    uint32_t row = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(self, 0)),
             column = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(self, 1));
    return PyUnicode_FromFormat("<Point row=%u, column=%u>", row, column);
}

PyObject *point_get_row(PyObject *self, void *Py_UNUSED(payload)) {
    return PyTuple_GetItem(self, 0);
}

PyObject *point_get_column(PyObject *self, void *Py_UNUSED(payload)) {
    return PyTuple_GetItem(self, 1);
}

PyObject *point_edit(PyObject *self, PyObject *args, PyObject *kwargs) {
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

    uint32_t new_start_byte;
    uint32_t row = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(self, 0)),
             column = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(self, 1));
    TSPoint point = {row, column};
    TSInputEdit edit = {
        .start_byte = start_byte,
        .old_end_byte = old_end_byte,
        .new_end_byte = new_end_byte,
        .start_point = {start_row, start_column},
        .old_end_point = {old_end_row, old_end_column},
        .new_end_point = {new_end_row, new_end_column},
    };

    ts_point_edit(&point, &new_start_byte, &edit);
    PyObject *new_point = point_new_internal(GET_MODULE_STATE(self), point);
    return PyTuple_Pack(2, new_point, PyLong_FromUnsignedLong(new_start_byte));
}

PyDoc_STRVAR(point_edit_doc,
             "edit(self, /, start_byte, old_end_byte, new_end_byte, start_point, "
             "old_end_point, new_end_point)\n--\n\n"
             "Edit this point to keep it in-sync with source code that has been edited." DOC_RETURNS
             "The edited point and its new start byte." DOC_TIP
             "This is useful for editing points without requiring a tree or node instance.");

static PyMethodDef point_methods[] = {
    {
        .ml_name = "edit",
        .ml_meth = (PyCFunction)point_edit,
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = point_edit_doc,
    },
    {NULL},
};

static PyGetSetDef point_accessors[] = {
    {"row", (getter)point_get_row, NULL, PyDoc_STR("The zero-based row of the document."), NULL},
    {"column", (getter)point_get_column, NULL,
     PyDoc_STR("The zero-based column of the document." DOC_NOTE "Measured in bytes."), NULL},
    {NULL},
};

static PyType_Slot point_type_slots[] = {
    {Py_tp_doc,
     PyDoc_STR("A position in a multi-line text document, in terms of rows and columns.")},
    {Py_tp_new, point_new},
    {Py_tp_repr, point_repr},
    {Py_tp_methods, point_methods},
    {Py_tp_getset, point_accessors},
    {0, NULL},
};

PyType_Spec point_type_spec = {
    .name = "tree_sitter.Point",
    .basicsize = sizeof(PyTupleObject),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = point_type_slots,
};
