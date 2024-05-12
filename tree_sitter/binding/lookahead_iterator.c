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
        self->language = PyObject_Init((PyObject *)language, state->language_type);
    }
    Py_INCREF(self->language);
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
    PyObject *language_obj;
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "OH:reset", &language_obj, &state_id)) {
        return NULL;
    }
    if (REPLACE("reset()", "reset_state()") < 0) {
        return NULL;
    }

    Py_ssize_t language_id = PyLong_AsSsize_t(language_obj);
    if (language_id < 1 || (language_id % sizeof(TSLanguage *)) != 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError, "invalid language ID");
        }
        return NULL;
    }

    language = PyLong_AsVoidPtr(language_obj);
    if (language == NULL) {
        return NULL;
    }

    bool result = ts_lookahead_iterator_reset(self->lookahead_iterator, language, state_id);
    return PyBool_FromLong(result);
}

PyObject *lookahead_iterator_reset_state(LookaheadIterator *self, PyObject *args,
                                         PyObject *kwargs) {
    uint16_t state_id;
    PyObject *language_obj = NULL;
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

PyObject *lookahead_iterator_iter_names(LookaheadIterator *self) {
    ModuleState *state = GET_MODULE_STATE(self);
    LookaheadNamesIterator *iter =
        PyObject_New(LookaheadNamesIterator, state->lookahead_names_iterator_type);
    if (iter == NULL) {
        return NULL;
    }
    iter->lookahead_iterator = self->lookahead_iterator;
    return PyObject_Init((PyObject *)iter, state->lookahead_names_iterator_type);
}

PyDoc_STRVAR(lookahead_iterator_reset_doc,
             "reset(self, language, state, /)\n--\n\n"
             "Reset the lookahead iterator.\n\n"
             ".. deprecated:: 0.22.0\n\n   Use :meth:`reset_state` instead." DOC_RETURNS
             "``True`` if it was reset successfully or ``False`` if it failed.");
PyDoc_STRVAR(lookahead_iterator_reset_state_doc,
             "reset_state(self, state, language=None)\n--\n\n"
             "Reset the lookahead iterator." DOC_RETURNS
             "``True`` if it was reset successfully or ``False`` if it failed.");
PyDoc_STRVAR(lookahead_iterator_iter_names_doc, "iter_names(self, /)\n--\n\n"
                                                "Iterate symbol names.");

static PyMethodDef lookahead_iterator_methods[] = {
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)lookahead_iterator_reset,
        .ml_flags = METH_VARARGS,
        .ml_doc = lookahead_iterator_reset_doc,
    },
    {
        .ml_name = "reset_state",
        .ml_meth = (PyCFunction)lookahead_iterator_reset_state,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = lookahead_iterator_reset_state_doc,
    },
    {
        .ml_name = "iter_names",
        .ml_meth = (PyCFunction)lookahead_iterator_iter_names,
        .ml_flags = METH_NOARGS,
        .ml_doc = lookahead_iterator_iter_names_doc,
    },
    {NULL},
};

static PyGetSetDef lookahead_iterator_accessors[] = {
    {"language", (getter)lookahead_iterator_get_language, NULL, PyDoc_STR("The current language."),
     NULL},
    {"current_symbol", (getter)lookahead_iterator_get_current_symbol, NULL,
     PyDoc_STR("The current symbol.\n\nNewly created iterators will return the ``ERROR`` symbol."),
     NULL},
    {"current_symbol_name", (getter)lookahead_iterator_get_current_symbol_name, NULL,
     PyDoc_STR("The current symbol name."), NULL},
    {NULL},
};

static PyType_Slot lookahead_iterator_type_slots[] = {
    {Py_tp_doc,
     PyDoc_STR(
         "A class that is used to look up symbols valid in a specific parse state." DOC_TIP
         "Lookahead iterators can be useful to generate suggestions and improve syntax error "
         "diagnostics.\n\nTo get symbols valid in an ``ERROR`` node, use the lookahead iterator "
         "on its first leaf node state.\nFor ``MISSING`` nodes, a lookahead iterator created "
         "on the previous non-extra leaf node may be appropriate.")},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, lookahead_iterator_dealloc},
    {Py_tp_repr, lookahead_iterator_repr},
    {Py_tp_iter, lookahead_iterator_iter},
    {Py_tp_iternext, lookahead_iterator_next},
    {Py_tp_methods, lookahead_iterator_methods},
    {Py_tp_getset, lookahead_iterator_accessors},
    {0, NULL},
};

PyType_Spec lookahead_iterator_type_spec = {
    .name = "tree_sitter.LookaheadIterator",
    .basicsize = sizeof(LookaheadIterator),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = lookahead_iterator_type_slots,
};
