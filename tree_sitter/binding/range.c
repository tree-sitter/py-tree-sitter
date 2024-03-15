#include "range.h"
#include "point.h"

PyObject *range_new_internal(ModuleState *state, TSRange range) {
    Range *self = (Range *)state->range_type->tp_alloc(state->range_type, 0);
    if (self != NULL) {
        self->range = range;
    }
    return (PyObject *)self;
}

PyObject *range_init(Range *self, PyObject *args, PyObject *kwargs) {
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

void range_dealloc(Range *self) { Py_TYPE(self)->tp_free(self); }

PyObject *range_repr(Range *self) {
    const char *format_string =
        "<Range start_point=(%u, %u), start_byte=%u, end_point=(%u, %u), end_byte=%u>";
    return PyUnicode_FromFormat(format_string, self->range.start_point.row,
                                self->range.start_point.column, self->range.start_byte,
                                self->range.end_point.row, self->range.end_point.column,
                                self->range.end_byte);
}

static inline bool range_is_instance(PyObject *self) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    return PyObject_IsInstance(self, (PyObject *)state->range_type);
}

PyObject *range_compare(Range *self, Range *other, int op) {
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

PyObject *range_get_start_point(Range *self, void *payload) {
    return POINT_NEW(GET_MODULE_STATE(Py_TYPE(self)), self->range.start_point);
}

PyObject *range_get_end_point(Range *self, void *payload) {
    return POINT_NEW(GET_MODULE_STATE(Py_TYPE(self)), self->range.end_point);
}

PyObject *range_get_start_byte(Range *self, void *payload) {
    return PyLong_FromSize_t((size_t)(self->range.start_byte));
}

PyObject *range_get_end_byte(Range *self, void *payload) {
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

PyType_Spec range_type_spec = {
    .name = "tree_sitter.Range",
    .basicsize = sizeof(Range),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = range_type_slots,
};
