#include "types.h"

PyObject *lookahead_symbols_iterator_repr(LookaheadSymbolsIterator *self) {
    return PyUnicode_FromFormat("<LookaheadSymbolsIterator %p>", self->lookahead_iterator);
}

void lookahead_symbols_iterator_dealloc(LookaheadSymbolsIterator *self) {
    Py_TYPE(self)->tp_free(self);
}

PyObject *lookahead_symbols_iterator_iter(LookaheadSymbolsIterator *self) {
    return Py_NewRef(self);
}

PyObject *lookahead_symbols_iterator_next(LookaheadSymbolsIterator *self) {
    if (!ts_lookahead_iterator_next(self->lookahead_iterator)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }
    TSSymbol symbol = ts_lookahead_iterator_current_symbol(self->lookahead_iterator);
    return PyLong_FromUnsignedLong(symbol);
}

static PyType_Slot lookahead_symbols_iterator_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("An iterator over the syntax node IDs.")},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, lookahead_symbols_iterator_dealloc},
    {Py_tp_repr, lookahead_symbols_iterator_repr},
    {Py_tp_iter, lookahead_symbols_iterator_iter},
    {Py_tp_iternext, lookahead_symbols_iterator_next},
    {0, NULL},
};

PyType_Spec lookahead_symbols_iterator_type_spec = {
    .name = "tree_sitter.LookaheadSymbolsIterator",
    .basicsize = sizeof(LookaheadSymbolsIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = lookahead_symbols_iterator_type_slots,
};
