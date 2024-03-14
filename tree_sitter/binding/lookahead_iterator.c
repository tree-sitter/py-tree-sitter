#include "lookahead_iterator.h"
#include "lookahead_names_iterator.h"

PyObject *lookahead_iterator_new_internal(ModuleState *state,
                                          TSLookaheadIterator *lookahead_iterator) {
    LookaheadIterator *self = (LookaheadIterator *)state->lookahead_iterator_type->tp_alloc(
        state->lookahead_iterator_type, 0);
    if (self != NULL) {
        self->lookahead_iterator = lookahead_iterator;
    }
    return (PyObject *)self;
}

void lookahead_iterator_dealloc(LookaheadIterator *self) {
    if (self->lookahead_iterator) {
        ts_lookahead_iterator_delete(self->lookahead_iterator);
    }
    Py_TYPE(self)->tp_free(self);
}

PyObject *lookahead_iterator_repr(LookaheadIterator *self) {
    const char *format_string = "<LookaheadIterator %x>";
    return PyUnicode_FromFormat(format_string, self->lookahead_iterator);
}

PyObject *lookahead_iterator_get_language(LookaheadIterator *self, void *payload) {
    return PyLong_FromVoidPtr((void *)ts_lookahead_iterator_language(self->lookahead_iterator));
}

PyObject *lookahead_iterator_get_current_symbol(LookaheadIterator *self, void *payload) {
    return PyLong_FromSize_t(
        (size_t)ts_lookahead_iterator_current_symbol(self->lookahead_iterator));
}

PyObject *lookahead_iterator_get_current_symbol_name(LookaheadIterator *self, void *payload) {
    const char *name = ts_lookahead_iterator_current_symbol_name(self->lookahead_iterator);
    return PyUnicode_FromString(name);
}

PyObject *lookahead_iterator_reset(LookaheadIterator *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &state_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyBool_FromLong(
        ts_lookahead_iterator_reset(self->lookahead_iterator, language, state_id));
}

PyObject *lookahead_iterator_reset_state(LookaheadIterator *self, PyObject *args) {
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "H", &state_id)) {
        return NULL;
    }
    return PyBool_FromLong(ts_lookahead_iterator_reset_state(self->lookahead_iterator, state_id));
}

PyObject *lookahead_iterator_iter(LookaheadIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *lookahead_iterator_next(LookaheadIterator *self) {
    bool res = ts_lookahead_iterator_next(self->lookahead_iterator);
    if (res) {
        return PyLong_FromSize_t(
            (size_t)ts_lookahead_iterator_current_symbol(self->lookahead_iterator));
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

PyObject *lookahead_iterator_names_iterator(LookaheadIterator *self) {
    return lookahead_names_iterator_new_internal(PyType_GetModuleState(Py_TYPE(self)),
                                                 self->lookahead_iterator);
}

PyObject *lookahead_iterator(PyObject *self, PyObject *args) {
    ModuleState *state = PyModule_GetState(self);

    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &state_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);

    TSLookaheadIterator *lookahead_iterator = ts_lookahead_iterator_new(language, state_id);

    if (lookahead_iterator == NULL) {
        Py_RETURN_NONE;
    }

    return lookahead_iterator_new_internal(state, lookahead_iterator);
}

static PyGetSetDef lookahead_iterator_accessors[] = {
    {"language", (getter)lookahead_iterator_get_language, NULL, "Get the language.", NULL},
    {"current_symbol", (getter)lookahead_iterator_get_current_symbol, NULL,
     "Get the current symbol.", NULL},
    {"current_symbol_name", (getter)lookahead_iterator_get_current_symbol_name, NULL,
     "Get the current symbol name.", NULL},
    {NULL},
};

static PyMethodDef lookahead_iterator_methods[] = {
    {.ml_name = "reset",
     .ml_meth = (PyCFunction)lookahead_iterator_reset,
     .ml_flags = METH_VARARGS,
     .ml_doc = "reset(language, state)\n--\n\n\
			  Reset the lookahead iterator to a new language and parse state.\n\
        	  This returns `True` if the language was set successfully, and `False` otherwise."},
    {.ml_name = "reset_state",
     .ml_meth = (PyCFunction)lookahead_iterator_reset_state,
     .ml_flags = METH_VARARGS,
     .ml_doc = "reset_state(state)\n--\n\n\
			  Reset the lookahead iterator to a new parse state.\n\
			  This returns `True` if the state was set successfully, and `False` otherwise."},
    {
        .ml_name = "iter_names",
        .ml_meth = (PyCFunction)lookahead_iterator_names_iterator,
        .ml_flags = METH_NOARGS,
        .ml_doc = "iter_names()\n--\n\n\
			  Get an iterator of the names of possible syntax nodes that could come next.",
    },
    {NULL},
};

static PyType_Slot lookahead_iterator_type_slots[] = {
    {Py_tp_doc, "An iterator over the possible syntax nodes that could come next."},
    {Py_tp_dealloc, lookahead_iterator_dealloc},
    {Py_tp_repr, lookahead_iterator_repr},
    {Py_tp_getset, lookahead_iterator_accessors},
    {Py_tp_methods, lookahead_iterator_methods},
    {Py_tp_iter, lookahead_iterator_iter},
    {Py_tp_iternext, lookahead_iterator_next},
    {0, NULL},
};

PyType_Spec lookahead_iterator_type_spec = {
    .name = "tree_sitter.LookaheadIterator",
    .basicsize = sizeof(LookaheadIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = lookahead_iterator_type_slots,
};
