#include "lookahead_iterator.h"
#include "lookahead_names_iterator.h"
#include "node.h"
#include "parser.h"
#include "point.h"
#include "query.h"
#include "range.h"
#include "tree.h"
#include "tree_cursor.h"

extern PyType_Spec capture_eq_capture_type_spec;
extern PyType_Spec capture_eq_string_type_spec;
extern PyType_Spec capture_match_string_type_spec;
extern PyType_Spec lookahead_iterator_type_spec;
extern PyType_Spec lookahead_names_iterator_type_spec;
extern PyType_Spec node_type_spec;
extern PyType_Spec parser_type_spec;
extern PyType_Spec query_capture_type_spec;
extern PyType_Spec query_match_type_spec;
extern PyType_Spec query_type_spec;
extern PyType_Spec range_type_spec;
extern PyType_Spec tree_cursor_type_spec;
extern PyType_Spec tree_type_spec;

static PyObject *language_version(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_version(language));
}

static PyObject *language_symbol_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_symbol_count(language));
}

static PyObject *language_state_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_state_count(language));
}

static PyObject *language_symbol_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    const char *name = ts_language_symbol_name(language, symbol);
    if (name == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name);
}

static PyObject *language_symbol_for_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    char *kind;
    Py_ssize_t length;
    bool named;
    if (!PyArg_ParseTuple(args, "Os#p", &language_id, &kind, &length, &named)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    TSSymbol symbol = ts_language_symbol_for_name(language, kind, length, named);
    if (symbol == 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromSize_t((size_t)symbol);
}

static PyObject *language_symbol_type(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    TSSymbol symbol;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t(ts_language_symbol_type(language, symbol));
}

static PyObject *language_field_count(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    if (!PyArg_ParseTuple(args, "O", &language_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t(ts_language_field_count(language));
}

static PyObject *language_field_name_for_id(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t field_id;
    if (!PyArg_ParseTuple(args, "OH", &language_id, &field_id)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    const char *field_name = ts_language_field_name_for_id(language, field_id);

    if (field_name == NULL) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromString(field_name);
}

static PyObject *language_field_id_for_name(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    char *field_name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "Os#", &language_id, &field_name, &length)) {
        return NULL;
    }

    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    TSFieldId field_id = ts_language_field_id_for_name(language, field_name, length);

    if (field_id == 0) {
        Py_RETURN_NONE;
    }

    return PyLong_FromSize_t((size_t)field_id);
}

static PyObject *language_query(PyObject *self, PyObject *args) {
    ModuleState *state = PyModule_GetState(self);
    TSLanguage *language;
    PyObject *language_id;
    char *source;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "Os#", &language_id, &source, &length)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return query_new_internal(state, language, source, length);
}

static PyObject *next_state(PyObject *self, PyObject *args) {
    TSLanguage *language;
    PyObject *language_id;
    uint16_t state_id;
    uint16_t symbol;
    if (!PyArg_ParseTuple(args, "OHH", &language_id, &state_id, &symbol)) {
        return NULL;
    }
    language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    return PyLong_FromSize_t((size_t)ts_language_next_state(language, state_id, symbol));
}

static void module_free(void *self) {
    ModuleState *state = PyModule_GetState((PyObject *)self);
    ts_query_cursor_delete(state->query_cursor);
    Py_XDECREF(state->tree_type);
    Py_XDECREF(state->tree_cursor_type);
    Py_XDECREF(state->parser_type);
    Py_XDECREF(state->node_type);
    Py_XDECREF(state->query_type);
    Py_XDECREF(state->range_type);
    Py_XDECREF(state->query_capture_type);
    Py_XDECREF(state->capture_eq_capture_type);
    Py_XDECREF(state->capture_eq_string_type);
    Py_XDECREF(state->capture_match_string_type);
    Py_XDECREF(state->lookahead_iterator_type);
    Py_XDECREF(state->re_compile);
}

static PyMethodDef module_methods[] = {
    {
        .ml_name = "_language_version",
        .ml_meth = (PyCFunction)language_version,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_count",
        .ml_meth = (PyCFunction)language_symbol_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_state_count",
        .ml_meth = (PyCFunction)language_state_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_name",
        .ml_meth = (PyCFunction)language_symbol_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_for_name",
        .ml_meth = (PyCFunction)language_symbol_for_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_symbol_type",
        .ml_meth = (PyCFunction)language_symbol_type,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_count",
        .ml_meth = (PyCFunction)language_field_count,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_name_for_id",
        .ml_meth = (PyCFunction)language_field_name_for_id,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_field_id_for_name",
        .ml_meth = (PyCFunction)language_field_id_for_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_next_state",
        .ml_meth = (PyCFunction)next_state,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_lookahead_iterator",
        .ml_meth = (PyCFunction)lookahead_iterator,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {
        .ml_name = "_language_query",
        .ml_meth = (PyCFunction)language_query,
        .ml_flags = METH_VARARGS,
        .ml_doc = "(internal)",
    },
    {NULL},
};

static struct PyModuleDef module_definition = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "binding",
    .m_doc = NULL,
    .m_size = sizeof(ModuleState),
    .m_free = module_free,
    .m_methods = module_methods,
};

PyMODINIT_FUNC PyInit__binding(void) {
    PyObject *module = PyModule_Create(&module_definition);
    if (module == NULL) {
        return NULL;
    }

    ModuleState *state = PyModule_GetState(module);

    state->tree_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_type_spec, NULL);
    state->tree_cursor_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_cursor_type_spec, NULL);
    state->parser_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &parser_type_spec, NULL);
    state->node_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &node_type_spec, NULL);
    state->query_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_type_spec, NULL);
    state->range_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &range_type_spec, NULL);
    state->query_capture_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_capture_type_spec, NULL);
    state->query_match_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_match_type_spec, NULL);
    state->capture_eq_capture_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_eq_capture_type_spec, NULL);
    state->capture_eq_string_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_eq_string_type_spec, NULL);
    state->capture_match_string_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &capture_match_string_type_spec, NULL);
    state->lookahead_iterator_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &lookahead_iterator_type_spec, NULL);
    state->lookahead_names_iterator_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &lookahead_names_iterator_type_spec, NULL);

    state->query_cursor = ts_query_cursor_new();
    if ((AddObjectRef(module, "Tree", (PyObject *)state->tree_type) < 0) ||
        (AddObjectRef(module, "TreeCursor", (PyObject *)state->tree_cursor_type) < 0) ||
        (AddObjectRef(module, "Parser", (PyObject *)state->parser_type) < 0) ||
        (AddObjectRef(module, "Node", (PyObject *)state->node_type) < 0) ||
        (AddObjectRef(module, "Query", (PyObject *)state->query_type) < 0) ||
        (AddObjectRef(module, "Range", (PyObject *)state->range_type) < 0) ||
        (AddObjectRef(module, "QueryCapture", (PyObject *)state->query_capture_type) < 0) ||
        (AddObjectRef(module, "QueryMatch", (PyObject *)state->query_match_type) < 0) ||
        (AddObjectRef(module, "CaptureEqCapture", (PyObject *)state->capture_eq_capture_type) <
         0) ||
        (AddObjectRef(module, "CaptureEqString", (PyObject *)state->capture_eq_string_type) < 0) ||
        (AddObjectRef(module, "CaptureMatchString", (PyObject *)state->capture_match_string_type) <
         0) ||
        (AddObjectRef(module, "LookaheadIterator", (PyObject *)state->lookahead_iterator_type) <
         0) ||
        (AddObjectRef(module, "LookaheadNamesIterator",
                      (PyObject *)state->lookahead_names_iterator_type) < 0)) {
        goto cleanup;
    }

    PyObject *re_module = PyImport_ImportModule("re");
    if (re_module == NULL) {
        goto cleanup;
    }
    state->re_compile = PyObject_GetAttrString(re_module, (char *)"compile");
    Py_DECREF(re_module);
    if (state->re_compile == NULL) {
        goto cleanup;
    }

    return module;

cleanup:
    Py_XDECREF(module);
    return NULL;
}
