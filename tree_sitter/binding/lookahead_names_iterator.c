#include "lookahead_names_iterator.h"

PyObject *lookahead_names_iterator_repr(LookaheadNamesIterator *self) {
    return PyUnicode_FromFormat("<LookaheadNamesIterator %p>", self->lookahead_iterator);
}

void lookahead_names_iterator_dealloc(LookaheadNamesIterator *self) {
    Py_TYPE(self)->tp_free(self);
}

PyObject *lookahead_names_iterator_iter(LookaheadNamesIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *lookahead_names_iterator_next(LookaheadNamesIterator *self) {
    if (!ts_lookahead_iterator_next(self->lookahead_iterator)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }
    const char *symbol = ts_lookahead_iterator_current_symbol_name(self->lookahead_iterator);
    return PyUnicode_FromString(symbol);
}

static PyType_Slot lookahead_names_iterator_type_slots[] = {
    {Py_tp_doc, "An iterator over the possible syntax nodes that could come next."},
    {Py_tp_new, NULL},
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
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = lookahead_names_iterator_type_slots,
};
