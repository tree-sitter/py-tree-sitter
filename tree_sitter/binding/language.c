#include "types.h"

int language_init(Language *self, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
    PyObject *language;
    if (!PyArg_ParseTuple(args, "O:__init__", &language)) {
        return -1;
    }

    if (PyCapsule_CheckExact(language)) {
        self->language = PyCapsule_GetPointer(language, "tree_sitter.Language");
    } else {
        Py_uintptr_t language_id = PyLong_AsSize_t(language);
        if (language_id == 0 || (language_id % sizeof(TSLanguage *)) != 0) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError, "invalid language ID");
            }
            return -1;
        }
        self->language = PyLong_AsVoidPtr(language);
    }

    if (self->language == NULL) {
        return -1;
    }
    self->version = ts_language_version(self->language);
#if HAS_LANGUAGE_NAMES
    self->name = ts_language_name(self->language);
#endif
    return 0;
}

void language_dealloc(Language *self) {
    ts_language_delete(self->language);
    Py_TYPE(self)->tp_free(self);
}

PyObject *language_repr(Language *self) {
#if HAS_LANGUAGE_NAMES
    if (self->name == NULL) {
        return PyUnicode_FromFormat("<Language id=%" PRIuPTR ", version=%u, name=None>",
                                    (Py_uintptr_t)self->language, self->version);
    }
    return PyUnicode_FromFormat("<Language id=%" PRIuPTR ", version=%u, name=\"%s\">",
                                (Py_uintptr_t)self->language, self->version, self->name);
#else
    return PyUnicode_FromFormat("<Language id=%" PRIuPTR ", version=%u>",
                                (Py_uintptr_t)self->language, self->version);
#endif
}

Py_hash_t language_hash(Language *self) { return (Py_hash_t)self->language; }

PyObject *language_compare(Language *self, PyObject *other, int op) {
    if ((op != Py_EQ && op != Py_NE) || !IS_INSTANCE(other, language_type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    Language *lang = (Language *)other;
    bool result = (Py_uintptr_t)self->language == (Py_uintptr_t)lang->language;
    return PyBool_FromLong(result ^ (op == Py_NE));
}

#if HAS_LANGUAGE_NAMES
PyObject *language_get_name(Language *self, void *Py_UNUSED(payload)) {
    return PyUnicode_FromString(self->name);
}
#endif

PyObject *language_get_version(Language *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(self->version);
}

PyObject *language_get_node_kind_count(Language *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_language_symbol_count(self->language));
}

PyObject *language_get_parse_state_count(Language *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_language_state_count(self->language));
}

PyObject *language_get_field_count(Language *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_language_field_count(self->language));
}

PyObject *language_node_kind_for_id(Language *self, PyObject *args) {
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "H:node_kind_for_id", &symbol)) {
        return NULL;
    }
    const char *name = ts_language_symbol_name(self->language, symbol);
    if (name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name);
}

PyObject *language_id_for_node_kind(Language *self, PyObject *args) {
    char *kind;
    Py_ssize_t length;
    int named;
    if (!PyArg_ParseTuple(args, "s#p:id_for_node_kind", &kind, &length, &named)) {
        return NULL;
    }
    TSSymbol symbol = ts_language_symbol_for_name(self->language, kind, length, named);
    if (symbol == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(symbol);
}

PyObject *language_node_kind_is_named(Language *self, PyObject *args) {
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "H:node_kind_is_named", &symbol)) {
        return NULL;
    }
    TSSymbolType symbol_type = ts_language_symbol_type(self->language, symbol);
    return PyBool_FromLong(symbol_type == TSSymbolTypeRegular);
}

PyObject *language_node_kind_is_visible(Language *self, PyObject *args) {
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "H:node_kind_is_visible", &symbol)) {
        return NULL;
    }
    TSSymbolType symbol_type = ts_language_symbol_type(self->language, symbol);
    return PyBool_FromLong(symbol_type <= TSSymbolTypeAnonymous);
}

PyObject *language_field_name_for_id(Language *self, PyObject *args) {
    uint16_t field_id;
    if (!PyArg_ParseTuple(args, "H:field_name_for_id", &field_id)) {
        return NULL;
    }
    const char *field_name = ts_language_field_name_for_id(self->language, field_id);
    if (field_name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(field_name);
}

PyObject *language_field_id_for_name(Language *self, PyObject *args) {
    char *field_name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:field_id_for_name", &field_name, &length)) {
        return NULL;
    }
    TSFieldId field_id = ts_language_field_id_for_name(self->language, field_name, length);
    if (field_id == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(field_id);
}

PyObject *language_next_state(Language *self, PyObject *args) {
    uint16_t state_id, symbol;
    if (!PyArg_ParseTuple(args, "HH:next_state", &state_id, &symbol)) {
        return NULL;
    }
    TSStateId state = ts_language_next_state(self->language, state_id, symbol);
    return PyLong_FromUnsignedLong(state);
}

PyObject *language_lookahead_iterator(Language *self, PyObject *args) {
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "H:lookahead_iterator", &state_id)) {
        return NULL;
    }
    TSLookaheadIterator *lookahead_iterator = ts_lookahead_iterator_new(self->language, state_id);
    if (lookahead_iterator == NULL) {
        Py_RETURN_NONE;
    }
    ModuleState *state = GET_MODULE_STATE(self);
    LookaheadIterator *iter = PyObject_New(LookaheadIterator, state->lookahead_iterator_type);
    if (iter == NULL) {
        return NULL;
    }
    Py_INCREF(self);
    iter->language = (PyObject *)self;
    iter->lookahead_iterator = lookahead_iterator;
    return PyObject_Init((PyObject *)iter, state->lookahead_iterator_type);
}

PyObject *language_query(Language *self, PyObject *args) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *source;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:query", &source, &length)) {
        return NULL;
    }
    return PyObject_CallFunction((PyObject *)state->query_type, "Os#", self, source, length);
}

PyDoc_STRVAR(language_node_kind_for_id_doc,
             "node_kind_for_id(self, id, /)\n--\n\n"
             "Get the name of the node kind for the given numerical id.");
PyDoc_STRVAR(language_id_for_node_kind_doc, "id_for_node_kind(self, kind, named, /)\n--\n\n"
                                            "Get the numerical id for the given node kind.");
PyDoc_STRVAR(language_node_kind_is_named_doc, "node_kind_is_named(self, id, /)\n--\n\n"
                                              "Check if the node type for the given numerical id "
                                              "is named (as opposed to an anonymous node type).");
PyDoc_STRVAR(language_node_kind_is_visible_doc,
             "node_kind_is_visible(self, id, /)\n--\n\n"
             "Check if the node type for the given numerical id "
             "is visible (as opposed to an auxiliary node type).");
PyDoc_STRVAR(language_field_name_for_id_doc, "field_name_for_id(self, field_id, /)\n--\n\n"
                                             "Get the field name for the given numerical id.");
PyDoc_STRVAR(language_field_id_for_name_doc, "field_id_for_name(self, name, /)\n--\n\n"
                                             "Get the numerical id for the given field name.");
PyDoc_STRVAR(language_next_state_doc,
             "next_state(self, state, id, /)\n--\n\n"
             "Get the next parse state." DOC_TIP "Combine this with ``lookahead_iterator`` to "
             "generate completion suggestions or valid symbols in error nodes." DOC_EXAMPLES
             ">>> state = language.next_state(node.parse_state, node.grammar_id)");
PyDoc_STRVAR(language_lookahead_iterator_doc,
             "lookahead_iterator(self, state, /)\n--\n\n"
             "Create a new :class:`LookaheadIterator` for this language and parse state.");
PyDoc_STRVAR(
    language_query_doc,
    "query(self, source, /)\n--\n\n"
    "Create a new :class:`Query` from a string containing one or more S-expression patterns.");

static PyMethodDef language_methods[] = {
    {
        .ml_name = "node_kind_for_id",
        .ml_meth = (PyCFunction)language_node_kind_for_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_node_kind_for_id_doc,
    },
    {
        .ml_name = "id_for_node_kind",
        .ml_meth = (PyCFunction)language_id_for_node_kind,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_id_for_node_kind_doc,
    },
    {
        .ml_name = "node_kind_is_named",
        .ml_meth = (PyCFunction)language_node_kind_is_named,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_node_kind_is_named_doc,
    },
    {
        .ml_name = "node_kind_is_visible",
        .ml_meth = (PyCFunction)language_node_kind_is_visible,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_node_kind_is_visible_doc,
    },
    {
        .ml_name = "field_name_for_id",
        .ml_meth = (PyCFunction)language_field_name_for_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_field_name_for_id_doc,
    },
    {
        .ml_name = "field_id_for_name",
        .ml_meth = (PyCFunction)language_field_id_for_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_field_id_for_name_doc,
    },
    {
        .ml_name = "next_state",
        .ml_meth = (PyCFunction)language_next_state,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_next_state_doc,
    },
    {
        .ml_name = "lookahead_iterator",
        .ml_meth = (PyCFunction)language_lookahead_iterator,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_lookahead_iterator_doc,
    },
    {
        .ml_name = "query",
        .ml_meth = (PyCFunction)language_query,
        .ml_flags = METH_VARARGS,
        .ml_doc = language_query_doc,
    },
    {NULL},
};

static PyGetSetDef language_accessors[] = {
#if HAS_LANGUAGE_NAMES
    {"name", (getter)language_get_name, NULL, PyDoc_STR("The name of the language."), NULL},
#endif
    {"version", (getter)language_get_version, NULL,
     PyDoc_STR("The ABI version number that indicates which version of "
               "the Tree-sitter CLI was used to generate this Language."),
     NULL},
    {"node_kind_count", (getter)language_get_node_kind_count, NULL,
     PyDoc_STR("The number of distinct node types in this language."), NULL},
    {"parse_state_count", (getter)language_get_parse_state_count, NULL,
     PyDoc_STR("The number of valid states in this language."), NULL},
    {"field_count", (getter)language_get_field_count, NULL,
     PyDoc_STR("The number of distinct field names in this language."), NULL},
    {NULL},
};

static PyType_Slot language_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("A class that defines how to parse a particular language.")},
    {Py_tp_init, language_init},
    {Py_tp_repr, language_repr},
    {Py_tp_hash, language_hash},
    {Py_tp_richcompare, language_compare},
    {Py_tp_dealloc, language_dealloc},
    {Py_tp_methods, language_methods},
    {Py_tp_getset, language_accessors},
    {0, NULL},
};

PyType_Spec language_type_spec = {
    .name = "tree_sitter.Language",
    .basicsize = sizeof(Language),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = language_type_slots,
};
