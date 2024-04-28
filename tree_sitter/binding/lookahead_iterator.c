#include "lookahead_iterator.h"
#include "language.h"

void lookahead_iterator_dealloc(LookaheadIterator *self) {
    if (self->lookahead_iterator) {
        ts_lookahead_iterator_delete(self->lookahead_iterator);
    }
    Py_XDECREF(self->language);
    Py_TYPE(self)->tp_free(self);
}

PyObject *lookahead_iterator_repr(LookaheadIterator *self) {
    return PyUnicode_FromFormat("<LookaheadIterator %p>", self->lookahead_iterator);
}

PyObject *lookahead_iterator_get_language(LookaheadIterator *self, void *Py_UNUSED(payload)) {
    TSLanguage *language_id =
        (TSLanguage *)ts_lookahead_iterator_language(self->lookahead_iterator);
    if (self->language == NULL || ((Language *)self->language)->language != language_id) {
        ModuleState *state = GET_MODULE_STATE(self);
        Language *language = PyObject_New(Language, state->language_type);
        if (language == NULL) {
            return NULL;
        }
        language->language = language_id;
        language->version = ts_language_version(language->language);
        PyObject *obj = PyObject_Init((PyObject *)language, state->language_type);
        Py_XSETREF(self->language, obj);
    } else {
        Py_INCREF(self->language);
    }
    return self->language;
}

PyObject *lookahead_iterator_get_current_symbol(LookaheadIterator *self, void *Py_UNUSED(payload)) {
    TSSymbol symbol = ts_lookahead_iterator_current_symbol(self->lookahead_iterator);
    return PyLong_FromUnsignedLong(symbol);
}

PyObject *lookahead_iterator_get_current_symbol_name(LookaheadIterator *self,
                                                     void *Py_UNUSED(payload)) {
    const char *name = ts_lookahead_iterator_current_symbol_name(self->lookahead_iterator);
    return PyUnicode_FromString(name);
}

PyObject *lookahead_iterator_reset(LookaheadIterator *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH:reset", &language_id, &state_id)) {
        return NULL;
    }
    if (REPLACE("reset()", "reset_state()") < 0) {
        return NULL;
    }
    language = language_check_pointer(PyLong_AsVoidPtr(language_id));
    if (language == NULL) {
        return NULL;
    }

    bool result = ts_lookahead_iterator_reset(self->lookahead_iterator, language, state_id);
    return PyBool_FromLong(result);
}

PyObject *lookahead_iterator_reset_state(LookaheadIterator *self, PyObject *args,
                                         PyObject *kwargs) {
    uint16_t state_id;
    PyObject *language_obj;
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {"state", "language", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "H|O!:reset_state", keywords, &state_id,
                                     state->language_type, &language_obj)) {
        return NULL;
    }

    bool result;
    if (language_obj == NULL) {
        result = ts_lookahead_iterator_reset_state(self->lookahead_iterator, state_id);
    } else {
        TSLanguage *language_id = ((Language *)language_obj)->language;
        result = ts_lookahead_iterator_reset(self->lookahead_iterator, language_id, state_id);
    }
    return PyBool_FromLong(result);
}

PyObject *lookahead_iterator_iter(LookaheadIterator *self) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *lookahead_iterator_next(LookaheadIterator *self) {
    if (!ts_lookahead_iterator_next(self->lookahead_iterator)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }
    TSSymbol symbol = ts_lookahead_iterator_current_symbol(self->lookahead_iterator);
    return PyLong_FromUnsignedLong(symbol);
}

PyObject *lookahead_iterator_names_iterator(LookaheadIterator *self) {
    ModuleState *state = GET_MODULE_STATE(self);
    LookaheadNamesIterator *iter =
        PyObject_New(LookaheadNamesIterator, state->lookahead_names_iterator_type);
    if (iter == NULL) {
        return NULL;
    }
    iter->lookahead_iterator = self->lookahead_iterator;
    return PyObject_Init((PyObject *)iter, state->lookahead_names_iterator_type);
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
     .ml_flags = METH_VARARGS | METH_KEYWORDS,
     .ml_doc = "reset_state(state, language=None)\n--\n\n\
			  Reset the lookahead iterator to a new parse state and optional language.\n\
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
    {Py_tp_new, NULL},
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
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = lookahead_iterator_type_slots,
};
