#include "language.h"
#include "lookahead_iterator.h"
#include "query.h"

#ifndef _MSC_VER
#include <setjmp.h>
#include <signal.h>

static jmp_buf segv_jmp;

static void segfault_handler(int signal) {
    if (signal == SIGSEGV) {
        longjmp(segv_jmp, true);
    }
}
#else
#include <windows.h>
#endif

int language_init(Language *self, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
    PyObject *ptr;
    const char *name;
    Py_ssize_t length;

    if (!PyArg_ParseTuple(args, "Os#:__init__", &ptr, &name, &length)) {
        return -1;
    }

    if (PyLong_AsLong(ptr) < 1) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError, "Language ID must be a positive int.");
        }
        return -1;
    }

    if (length == 0) {
        PyErr_SetString(PyExc_ValueError, "Language name cannot be empty.");
        return -1;
    }

    self->language = (TSLanguage *)PyLong_AsVoidPtr(ptr);

    // HACK: recover from invalid pointer using a signal handler or SEH
#ifndef _MSC_VER
    PyOS_sighandler_t old_handler = PyOS_setsig(SIGSEGV, segfault_handler);
    if (!setjmp(segv_jmp)) {
#else
    __try {
#endif
        self->version = ts_language_version(self->language);
#ifndef _MSC_VER
    } else {
#else
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                 : EXCEPTION_CONTINUE_SEARCH) {
#endif
        PyErr_SetString(PyExc_RuntimeError, "Invalid TSLanguage pointer.");
    }
#ifndef _MSC_VER
    PyOS_setsig(SIGSEGV, old_handler);
#endif
    if (PyErr_Occurred()) {
        return -1;
    }

    self->name = PyMem_RawCalloc(length + 1, sizeof(char));
    strncpy(self->name, name, length);

    return 0;
}

void language_dealloc(Language *self) {
    PyMem_RawFree(self->name);
    Py_TYPE(self)->tp_free(self);
}

PyObject *language_repr(Language *self) {
    const char *format_string = "<Language id=%" PRIuPTR ", name=\"%s\", version=%u>";
    return PyUnicode_FromFormat(format_string, (uintptr_t)self->language, self->name,
                                self->version);
}

PyObject *language_str(Language *self) { return PyUnicode_FromString(self->name); }

PyObject *language_int(Language *self) { return PyLong_FromVoidPtr((void *)self->language); }

Py_hash_t language_hash(Language *self) { return (1 << self->version) * (uintptr_t)self->language; }

PyObject *language_compare(Language *self, PyObject *other, int op) {
    if ((op != Py_EQ && op != Py_NE) || !IS_INSTANCE(other, language_type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    Language *lang = (Language *)other;
    bool result = self->version == lang->version && strcmp(self->name, lang->name) == 0;
    return PyBool_FromLong(result & (op == Py_EQ));
}

PyObject *language_get_name(Language *self, void *Py_UNUSED(payload)) {
    return PyUnicode_FromString(self->name);
}

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
    bool named;
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
    uint16_t state_id;
    uint16_t symbol;
    if (!PyArg_ParseTuple(args, "HH:next_state", &state_id, &symbol)) {
        return NULL;
    }
    TSStateId state = ts_language_next_state(self->language, state_id, symbol);
    return PyLong_FromUnsignedLong(state);
}

PyObject *language_lookahead_iterator(Language *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    uint16_t state_id;
    if (!PyArg_ParseTuple(args, "H:lookahead_iterator", &state_id)) {
        return NULL;
    }
    TSLookaheadIterator *lookahead_iterator = ts_lookahead_iterator_new(self->language, state_id);

    if (lookahead_iterator == NULL) {
        Py_RETURN_NONE;
    }

    return lookahead_iterator_new_internal(state, lookahead_iterator);
}

PyObject *language_query(Language *self, PyObject *args) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    char *source;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:query", &source, &length)) {
        return NULL;
    }
    return query_new_internal(state, self->language, source, length);
}

static PyMethodDef language_methods[] = {
    {.ml_name = "node_kind_for_id",
     .ml_meth = (PyCFunction)language_node_kind_for_id,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Get the name of the node kind for the given numerical id."},
    {.ml_name = "id_for_node_kind",
     .ml_meth = (PyCFunction)language_id_for_node_kind,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Get the numerical id for the given node kind."},
    {.ml_name = "node_kind_is_named",
     .ml_meth = (PyCFunction)language_node_kind_is_named,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Check if the node type for the given numerical id is named"
               " (as opposed to an anonymous node type)."},
    {.ml_name = "node_kind_is_visible",
     .ml_meth = (PyCFunction)language_node_kind_is_visible,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Check if the node type for the given numerical id is visible"
               " (as opposed to an auxiliary node type)."},
    {.ml_name = "field_name_for_id",
     .ml_meth = (PyCFunction)language_field_name_for_id,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Get the name of the field for the given numerical id."},
    {.ml_name = "field_id_for_name",
     .ml_meth = (PyCFunction)language_field_id_for_name,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Return the field id for a field name."},
    {.ml_name = "next_state",
     .ml_meth = (PyCFunction)language_next_state,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Get the next parse state. Combine this with `lookahead_iterator` "
               "to generate completion suggestions or valid symbols in error nodes."},
    {.ml_name = "lookahead_iterator",
     .ml_meth = (PyCFunction)language_lookahead_iterator,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Create a new lookahead iterator for this language and parse state. "
               "Returns `None` if state is invalid for this language.\n\n"
               "Iterating `LookaheadIterator` will yield valid symbols in the "
               "given parse state. Newly created lookahead iterators will return "
               "the `ERROR` symbol from `LookaheadIterator.current_symbol`.\n\n"
               "Lookahead iterators can be useful to generate suggestions and improve syntax "
               "error diagnostics. To get symbols valid in an `ERROR` node, use the lookahead "
               "iterator on its first leaf node state. For `MISSING` nodes, a lookahead "
               "iterator created on the previous non-extra leaf node may be appropriate."},
    {.ml_name = "query",
     .ml_meth = (PyCFunction)language_query,
     .ml_flags = METH_VARARGS,
     .ml_doc = "Create a Query with the given source code."},
    {NULL},
};

static PyGetSetDef language_accessors[] = {
    {"name", (getter)language_get_name, NULL, "The name of the language.", NULL},
    {"version", (getter)language_get_version, NULL,
     "Get the ABI version number that indicates which version of "
     "the Tree-sitter CLI was used to generate this Language.",
     NULL},
    {"node_kind_count", (getter)language_get_node_kind_count, NULL,
     "Get the number of distinct node types in this language.", NULL},
    {"parse_state_count", (getter)language_get_parse_state_count, NULL,
     "Get the number of valid states in this language.", NULL},
    {"field_count", (getter)language_get_field_count, NULL,
     "Get the number of fields in this language.", NULL},
    {NULL},
};

static PyType_Slot language_type_slots[] = {
    {Py_tp_doc, "A tree-sitter language."},
    {Py_tp_init, language_init},
    {Py_tp_repr, language_repr},
    {Py_tp_str, language_str},
    {Py_tp_hash, language_hash},
    {Py_tp_richcompare, language_compare},
    {Py_tp_dealloc, language_dealloc},
    {Py_tp_methods, language_methods},
    {Py_tp_getset, language_accessors},
    {Py_nb_int, language_int},
    {0, NULL},
};

PyType_Spec language_type_spec = {
    .name = "tree_sitter.Language",
    .basicsize = sizeof(Language),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = language_type_slots,
};
