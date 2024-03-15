#include "types.h"

extern PyType_Spec capture_eq_capture_type_spec;
extern PyType_Spec capture_eq_string_type_spec;
extern PyType_Spec capture_match_string_type_spec;
extern PyType_Spec language_type_spec;
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

// TODO(0.24): drop Python 3.9 support
#if PY_MINOR_VERSION > 9
#define AddObjectRef PyModule_AddObjectRef
#else
static int AddObjectRef(PyObject *module, const char *name, PyObject *value) {
    if (value == NULL) {
        PyErr_Format(PyExc_SystemError, "PyModule_AddObjectRef() %s == NULL", name);
        return -1;
    }
    int ret = PyModule_AddObject(module, name, value);
    if (ret == 0) {
        Py_INCREF(value);
    }
    return ret;
}
#endif

static inline PyObject *import_attribute(const char *mod, const char *attr) {
    PyObject *module = PyImport_ImportModule(mod);
    if (module == NULL) {
        return NULL;
    }
    PyObject *import = PyObject_GetAttrString(module, attr);
    Py_DECREF(module);
    return import;
}

static void module_free(void *self) {
    ModuleState *state = PyModule_GetState((PyObject *)self);
    ts_query_cursor_delete(state->query_cursor);
    Py_XDECREF(state->point_type);
    Py_XDECREF(state->tree_type);
    Py_XDECREF(state->tree_cursor_type);
    Py_XDECREF(state->language_type);
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
    Py_XDECREF(state->namedtuple);
}

static struct PyModuleDef module_definition = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_binding",
    .m_doc = NULL,
    .m_size = sizeof(ModuleState),
    .m_free = module_free,
};

PyMODINIT_FUNC PyInit__binding(void) {
    PyObject *module = PyModule_Create(&module_definition);
    if (module == NULL) {
        return NULL;
    }

    ModuleState *state = PyModule_GetState(module);

    state->query_cursor = ts_query_cursor_new();

    state->tree_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_type_spec, NULL);
    state->tree_cursor_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_cursor_type_spec, NULL);
    state->language_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &language_type_spec, NULL);
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

    if ((AddObjectRef(module, "Tree", (PyObject *)state->tree_type) < 0) ||
        (AddObjectRef(module, "TreeCursor", (PyObject *)state->tree_cursor_type) < 0) ||
        (AddObjectRef(module, "Language", (PyObject *)state->language_type) < 0) ||
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

    state->re_compile = import_attribute("re", "compile");
    if (state->re_compile == NULL) {
        goto cleanup;
    }

    state->namedtuple = import_attribute("collections", "namedtuple");
    if (state->namedtuple == NULL) {
        goto cleanup;
    }

    PyObject *point_args = Py_BuildValue("s[ss]", "Point", "row", "column");
    PyObject *point_kwargs = PyDict_New();
    PyDict_SetItemString(point_kwargs, "module", PyUnicode_FromString("tree_sitter"));
    state->point_type = (PyTypeObject *)PyObject_Call(state->namedtuple, point_args, point_kwargs);
    Py_DECREF(point_args);
    Py_DECREF(point_kwargs);
    if (state->point_type == NULL ||
        AddObjectRef(module, "Point", (PyObject *)state->point_type) < 0) {
        goto cleanup;
    }

    PyModule_AddIntConstant(module, "LANGUAGE_VERSION", TREE_SITTER_LANGUAGE_VERSION);
    PyModule_AddIntConstant(module, "MIN_COMPATIBLE_LANGUAGE_VERSION",
                            TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION);

    return module;

cleanup:
    Py_XDECREF(module);
    return NULL;
}
