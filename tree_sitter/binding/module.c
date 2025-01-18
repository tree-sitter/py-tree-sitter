#include "types.h"

extern PyType_Spec language_type_spec;
extern PyType_Spec lookahead_iterator_type_spec;
extern PyType_Spec node_type_spec;
extern PyType_Spec parser_type_spec;
extern PyType_Spec query_type_spec;
extern PyType_Spec query_predicate_anyof_type_spec;
extern PyType_Spec query_predicate_eq_capture_type_spec;
extern PyType_Spec query_predicate_eq_string_type_spec;
extern PyType_Spec query_predicate_generic_type_spec;
extern PyType_Spec query_predicate_match_type_spec;
extern PyType_Spec range_type_spec;
extern PyType_Spec tree_cursor_type_spec;
extern PyType_Spec tree_type_spec;

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
    ts_tree_cursor_delete(&state->default_cursor);
    Py_XDECREF(state->language_type);
    Py_XDECREF(state->log_type_type);
    Py_XDECREF(state->lookahead_iterator_type);
    Py_XDECREF(state->node_type);
    Py_XDECREF(state->parser_type);
    Py_XDECREF(state->point_type);
    Py_XDECREF(state->query_predicate_anyof_type);
    Py_XDECREF(state->query_predicate_eq_capture_type);
    Py_XDECREF(state->query_predicate_eq_string_type);
    Py_XDECREF(state->query_predicate_generic_type);
    Py_XDECREF(state->query_predicate_match_type);
    Py_XDECREF(state->query_type);
    Py_XDECREF(state->range_type);
    Py_XDECREF(state->tree_cursor_type);
    Py_XDECREF(state->tree_type);
    Py_XDECREF(state->query_error);
    Py_XDECREF(state->re_compile);
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

    ts_set_allocator(PyMem_Malloc, PyMem_Calloc, PyMem_Realloc, PyMem_Free);

    state->language_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &language_type_spec, NULL);
    state->lookahead_iterator_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &lookahead_iterator_type_spec, NULL);
    state->node_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &node_type_spec, NULL);
    state->parser_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &parser_type_spec, NULL);
    state->query_predicate_anyof_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_predicate_anyof_type_spec, NULL);
    state->query_predicate_eq_capture_type = (PyTypeObject *)PyType_FromModuleAndSpec(
        module, &query_predicate_eq_capture_type_spec, NULL);
    state->query_predicate_eq_string_type = (PyTypeObject *)PyType_FromModuleAndSpec(
        module, &query_predicate_eq_string_type_spec, NULL);
    state->query_predicate_generic_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_predicate_generic_type_spec, NULL);
    state->query_predicate_match_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_predicate_match_type_spec, NULL);
    state->query_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &query_type_spec, NULL);
    state->range_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &range_type_spec, NULL);
    state->tree_cursor_type =
        (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_cursor_type_spec, NULL);
    state->tree_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &tree_type_spec, NULL);

    if ((PyModule_AddObjectRef(module, "Language", (PyObject *)state->language_type) < 0) ||
        (PyModule_AddObjectRef(module, "LookaheadIterator",
                               (PyObject *)state->lookahead_iterator_type) < 0) ||
        (PyModule_AddObjectRef(module, "Node", (PyObject *)state->node_type) < 0) ||
        (PyModule_AddObjectRef(module, "Parser", (PyObject *)state->parser_type) < 0) ||
        (PyModule_AddObjectRef(module, "Query", (PyObject *)state->query_type) < 0) ||
        (PyModule_AddObjectRef(module, "QueryPredicateAnyof",
                               (PyObject *)state->query_predicate_anyof_type) < 0) ||
        (PyModule_AddObjectRef(module, "QueryPredicateEqCapture",
                               (PyObject *)state->query_predicate_eq_capture_type) < 0) ||
        (PyModule_AddObjectRef(module, "QueryPredicateEqString",
                               (PyObject *)state->query_predicate_eq_string_type) < 0) ||
        (PyModule_AddObjectRef(module, "QueryPredicateGeneric",
                               (PyObject *)state->query_predicate_generic_type) < 0) ||
        (PyModule_AddObjectRef(module, "QueryPredicateMatch",
                               (PyObject *)state->query_predicate_match_type) < 0) ||
        (PyModule_AddObjectRef(module, "Range", (PyObject *)state->range_type) < 0) ||
        (PyModule_AddObjectRef(module, "Tree", (PyObject *)state->tree_type) < 0) ||
        (PyModule_AddObjectRef(module, "TreeCursor", (PyObject *)state->tree_cursor_type) < 0)) {
        goto cleanup;
    }

    state->query_error = PyErr_NewExceptionWithDoc(
        "tree_sitter.QueryError",
        PyDoc_STR("An error that occurred while attempting to create a :class:`Query`."),
        PyExc_ValueError, NULL);
    if (state->query_error == NULL ||
        PyModule_AddObjectRef(module, "QueryError", state->query_error) < 0) {
        goto cleanup;
    }

    state->re_compile = import_attribute("re", "compile");
    if (state->re_compile == NULL) {
        goto cleanup;
    }

    PyObject *namedtuple = import_attribute("collections", "namedtuple");
    if (namedtuple == NULL) {
        goto cleanup;
    }
    PyObject *point_args = Py_BuildValue("s[ss]", "Point", "row", "column");
    PyObject *point_kwargs = PyDict_New();
    PyDict_SetItemString(point_kwargs, "module", PyUnicode_FromString("tree_sitter"));
    state->point_type = (PyTypeObject *)PyObject_Call(namedtuple, point_args, point_kwargs);
    Py_DECREF(point_args);
    Py_DECREF(point_kwargs);
    Py_DECREF(namedtuple);
    if (state->point_type == NULL ||
        PyModule_AddObjectRef(module, "Point", (PyObject *)state->point_type) < 0) {
        goto cleanup;
    }

    PyObject *int_enum = import_attribute("enum", "IntEnum");
    if (int_enum == NULL) {
        goto cleanup;
    }
    state->log_type_type = (PyTypeObject *)PyObject_CallFunction(
        int_enum, "s{sisi}", "LogType", "PARSE", TSLogTypeParse, "LEX", TSLogTypeLex);
    if (state->log_type_type == NULL ||
        PyModule_AddObjectRef(module, "LogType", (PyObject *)state->log_type_type) < 0) {
        goto cleanup;
    };
    Py_DECREF(int_enum);

    PyModule_AddIntConstant(module, "LANGUAGE_VERSION", TREE_SITTER_LANGUAGE_VERSION);
    PyModule_AddIntConstant(module, "MIN_COMPATIBLE_LANGUAGE_VERSION",
                            TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION);

#ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(module, Py_MOD_GIL_USED);
#endif
    return module;

cleanup:
    Py_XDECREF(module);
    return NULL;
}
