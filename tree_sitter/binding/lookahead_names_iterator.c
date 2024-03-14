#include "lookahead_names_iterator.h"

PyObject *lookahead_names_iterator_new_internal(ModuleState *state,
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

PyObject *lookahead_names_iterator_repr(LookaheadNamesIterator *self) {
    const char *format_string = "<LookaheadNamesIterator %x>";
    return PyUnicode_FromFormat(format_string, self->lookahead_iterator);
}

void lookahead_names_iterator_dealloc(LookaheadNamesIterator *self) {
    Py_TYPE(self)->tp_free(self);
}

PyObject *lookahead_names_iterator_iter(LookaheadNamesIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *lookahead_names_iterator_next(LookaheadNamesIterator *self) {
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

PyType_Spec lookahead_names_iterator_type_spec = {
    .name = "tree_sitter.LookaheadNamesIterator",
    .basicsize = sizeof(LookaheadNamesIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = lookahead_names_iterator_type_slots,
};
