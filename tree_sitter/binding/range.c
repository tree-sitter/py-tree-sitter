#include "range.h"

int range_init(Range *self, PyObject *args, PyObject *kwargs) {
    uint32_t start_row, start_col, end_row, end_col, start_byte, end_byte;
    char *keywords[] = {
        "start_point", "end_point", "start_byte", "end_byte", NULL,
    };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "(II)(II)II:__init__", keywords, &start_row,
                                     &start_col, &end_row, &end_col, &start_byte, &end_byte)) {
        return -1;
    }

    self->range.start_point.row = start_row;
    self->range.start_point.column = start_col;
    self->range.end_point.row = end_row;
    self->range.end_point.column = end_col;
    self->range.start_byte = start_byte;
    self->range.end_byte = end_byte;

    return 0;
}

void range_dealloc(Range *self) { Py_TYPE(self)->tp_free(self); }

PyObject *range_repr(Range *self) {
    const char *format_string =
        "<Range start_point=(%u, %u), end_point=(%u, %u), start_byte=%u, end_byte=%u>";
    return PyUnicode_FromFormat(format_string, self->range.start_point.row,
                                self->range.start_point.column, self->range.end_point.row,
                                self->range.end_point.column, self->range.start_byte,
                                self->range.end_byte);
}

Py_hash_t range_hash(Range *self) {
    // FIXME: replace with an efficient integer hashing algorithm
    PyObject *row_tuple = PyTuple_Pack(2, PyLong_FromLong(self->range.start_point.row),
                                       PyLong_FromLong(self->range.end_point.row));
    PyObject *col_tuple = PyTuple_Pack(2, PyLong_FromLong(self->range.start_point.column),
                                       PyLong_FromLong(self->range.end_point.column));
    PyObject *bytes_tuple = PyTuple_Pack(2, PyLong_FromLong(self->range.start_byte),
                                         PyLong_FromLong(self->range.end_byte));
    PyObject *range_tuple = PyTuple_Pack(3, row_tuple, col_tuple, bytes_tuple);
    Py_hash_t hash = PyObject_Hash(range_tuple);

    Py_DECREF(range_tuple);
    Py_DECREF(row_tuple);
    Py_DECREF(col_tuple);
    Py_DECREF(bytes_tuple);
    return hash;
}

PyObject *range_compare(Range *self, PyObject *other, int op) {
    if ((op != Py_EQ && op != Py_NE) || !IS_INSTANCE(other, range_type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    Range *range = (Range *)other;
    bool result = ((self->range.start_point.row == range->range.start_point.row) &&
                   (self->range.start_point.column == range->range.start_point.column) &&
                   (self->range.start_byte == range->range.start_byte) &&
                   (self->range.end_point.row == range->range.end_point.row) &&
                   (self->range.end_point.column == range->range.end_point.column) &&
                   (self->range.end_byte == range->range.end_byte));
    return PyBool_FromLong(result & (op == Py_EQ));
}

PyObject *range_get_start_point(Range *self, void *Py_UNUSED(payload)) {
    return POINT_NEW(GET_MODULE_STATE(self), self->range.start_point);
}

PyObject *range_get_end_point(Range *self, void *Py_UNUSED(payload)) {
    return POINT_NEW(GET_MODULE_STATE(self), self->range.end_point);
}

PyObject *range_get_start_byte(Range *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(self->range.start_byte);
}

PyObject *range_get_end_byte(Range *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(self->range.end_byte);
}

static PyGetSetDef range_accessors[] = {
    {"start_point", (getter)range_get_start_point, NULL, PyDoc_STR("The start point."), NULL},
    {"start_byte", (getter)range_get_start_byte, NULL, PyDoc_STR("The start byte."), NULL},
    {"end_point", (getter)range_get_end_point, NULL, PyDoc_STR("The end point."), NULL},
    {"end_byte", (getter)range_get_end_byte, NULL, PyDoc_STR("The end byte."), NULL},
    {NULL},
};

static PyType_Slot range_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("A range of positions in a multi-line text document, "
                          "both in terms of bytes and of rows and columns.")},
    {Py_tp_init, range_init},
    {Py_tp_dealloc, range_dealloc},
    {Py_tp_repr, range_repr},
    {Py_tp_hash, range_hash},
    {Py_tp_richcompare, range_compare},
    {Py_tp_getset, range_accessors},
    {0, NULL},
};

PyType_Spec range_type_spec = {
    .name = "tree_sitter.Range",
    .basicsize = sizeof(Range),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = range_type_slots,
};
