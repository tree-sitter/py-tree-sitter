#include "types.h"

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

PyObject *node_get_text(Node *self, void *payload);

#define PREDICATE_CMP(val1, val2, predicate)                                                       \
    PyObject_RichCompareBool((val1), (val2), (predicate)->is_positive ? Py_EQ : Py_NE)

// clang-format off
#define PREDICATE_BREAK(predicate, result)                                                         \
    if (((result) != 1 && !(predicate)->is_any) || ((result) == 1 && (predicate)->is_any)) break
// clang-format on

static inline PyObject *nodes_for_capture_index(ModuleState *state, uint32_t index,
                                                TSQueryMatch *match, Tree *tree) {
    PyObject *result = PyList_New(0);
    for (uint16_t i = 0; i < match->capture_count; ++i) {
        TSQueryCapture capture = match->captures[i];
        if (capture.index == index) {
            PyObject *node = node_new_internal(state, capture.node, (PyObject *)tree);
            PyList_Append(result, node);
            Py_XDECREF(node);
        }
    }
    return result;
}

static inline PyObject *captures_for_match(ModuleState *state, TSQuery *query, TSQueryMatch *match,
                                           Tree *tree) {
    uint32_t name_length;
    PyObject *captures = PyDict_New();
    for (uint32_t j = 0; j < match->capture_count; ++j) {
        TSQueryCapture capture = match->captures[j];
        const char *capture_name =
            ts_query_capture_name_for_id(query, capture.index, &name_length);
        PyObject *capture_name_obj = PyUnicode_FromStringAndSize(capture_name, name_length);
        if (capture_name_obj == NULL) {
            return NULL;
        }
        PyObject *nodes = nodes_for_capture_index(state, capture.index, match, tree);
        if (PyDict_SetItem(captures, capture_name_obj, nodes) == -1) {
            return NULL;
        }
        Py_DECREF(capture_name_obj);
    }
    return captures;
}

static inline bool satisfies_anyof(ModuleState *state, QueryPredicateAnyOf *predicate,
                                   TSQueryMatch *match, Tree *tree) {
    PyObject *nodes = nodes_for_capture_index(state, predicate->capture_id, match, tree);
    for (size_t i = 0, l = (size_t)PyList_Size(nodes); i < l; ++i) {
        Node *node = (Node *)PyList_GetItem(nodes, i);
        PyObject *text1 = node_get_text(node, NULL);
        bool found_match = false;

        for (size_t j = 0, k = (size_t)PyList_Size(predicate->values); j < k; ++j) {
            PyObject *text2 = PyList_GetItem(predicate->values, j);
            if (PREDICATE_CMP(text1, text2, predicate) == 1) {
                found_match = true;
                break;
            }
        }

        Py_DECREF(text1);

        if (!found_match) {
            Py_DECREF(nodes);
            return false;
        }
    }

    Py_DECREF(nodes);
    return true;
}

static inline bool satisfies_eq_capture(ModuleState *state, QueryPredicateEqCapture *predicate,
                                        TSQueryMatch *match, Tree *tree) {
    PyObject *nodes1 = nodes_for_capture_index(state, predicate->capture1_id, match, tree),
             *nodes2 = nodes_for_capture_index(state, predicate->capture2_id, match, tree);
    PyObject *text1, *text2;
    size_t size1 = (size_t)PyList_Size(nodes1), size2 = (size_t)PyList_Size(nodes2);
    int result = 1;
    for (size_t i = 0, l = size1 < size2 ? size1 : size2; i < l; ++i) {
        text1 = node_get_text((Node *)PyList_GetItem(nodes1, i), NULL);
        text2 = node_get_text((Node *)PyList_GetItem(nodes2, i), NULL);
        result = PREDICATE_CMP(text1, text2, predicate);
        Py_DECREF(text1);
        Py_DECREF(text2);
        PREDICATE_BREAK(predicate, result);
    }
    Py_DECREF(nodes1);
    Py_DECREF(nodes2);
    return result == 1;
}

static inline bool satisfies_eq_string(ModuleState *state, QueryPredicateEqString *predicate,
                                       TSQueryMatch *match, Tree *tree) {
    PyObject *nodes = nodes_for_capture_index(state, predicate->capture_id, match, tree);
    PyObject *text1, *text2 = predicate->string_value;
    int result = 1;
    for (size_t i = 0, l = (size_t)PyList_Size(nodes); i < l; ++i) {
        text1 = node_get_text((Node *)PyList_GetItem(nodes, i), NULL);
        result = PREDICATE_CMP(text1, text2, predicate);
        Py_DECREF(text1);
        PREDICATE_BREAK(predicate, result);
    }
    Py_DECREF(nodes);
    return result == 1;
}

static inline bool satisfies_match(ModuleState *state, QueryPredicateMatch *predicate,
                                   TSQueryMatch *match, Tree *tree) {
    PyObject *nodes = nodes_for_capture_index(state, predicate->capture_id, match, tree);
    PyObject *text, *search_result;
    int result = 1;
    for (size_t i = 0, l = (size_t)PyList_Size(nodes); i < l; ++i) {
        text = node_get_text((Node *)PyList_GetItem(nodes, i), NULL);
        search_result =
            PyObject_CallMethod(predicate->pattern, "search", "s", PyBytes_AsString(text));
        result = (search_result != NULL && search_result != Py_None) == predicate->is_positive;
        Py_DECREF(text);
        Py_XDECREF(search_result);
        PREDICATE_BREAK(predicate, result);
    }
    Py_DECREF(nodes);
    return result == 1;
}

bool query_satisfies_predicates(Query *query, TSQueryMatch match, Tree *tree, PyObject *callable) {
    // if there is no source, ignore the predicates
    if (tree->source == NULL || tree->source == Py_None) {
        return true;
    }

    ModuleState *state = GET_MODULE_STATE(query);
    PyObject *pattern_predicates = PyList_GetItem(query->predicates, match.pattern_index);
    if (pattern_predicates == NULL) {
        return false;
    }

    // check if all predicates are satisfied
    bool is_satisfied = true;
    for (size_t i = 0, l = (size_t)PyList_Size(pattern_predicates); is_satisfied && i < l; ++i) {
        PyObject *item = PyList_GetItem(pattern_predicates, i);
        if (IS_INSTANCE_OF(item, state->query_predicate_anyof_type)) {
            is_satisfied = satisfies_anyof(state, (QueryPredicateAnyOf *)item, &match, tree);
        } else if (IS_INSTANCE_OF(item, state->query_predicate_eq_capture_type)) {
            is_satisfied =
                satisfies_eq_capture(state, (QueryPredicateEqCapture *)item, &match, tree);
        } else if (IS_INSTANCE_OF(item, state->query_predicate_eq_string_type)) {
            is_satisfied = satisfies_eq_string(state, (QueryPredicateEqString *)item, &match, tree);
        } else if (IS_INSTANCE_OF(item, state->query_predicate_match_type)) {
            is_satisfied = satisfies_match(state, (QueryPredicateMatch *)item, &match, tree);
        } else if (callable != NULL) {
            PyObject *captures = captures_for_match(state, query->query, &match, tree);
            if (captures == NULL) {
                is_satisfied = false;
                break;
            }
            QueryPredicateGeneric *predicate = (QueryPredicateGeneric *)item;
            PyObject *result = PyObject_CallFunction(callable, "OOIO", predicate->predicate,
                                                     predicate->arguments, i, captures);
            if (result == NULL) {
                is_satisfied = false;
                break;
            }
            is_satisfied = PyObject_IsTrue(result);
            Py_DECREF(result);
        }
    }

    return is_satisfied;
}

// QueryPredicateAnyOf {{{

static void query_predicate_anyof_dealloc(QueryPredicateAnyOf *self) {
    Py_XDECREF(self->values);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot query_predicate_anyof_slots[] = {
    {Py_tp_doc, ""},
    {Py_tp_dealloc, query_predicate_anyof_dealloc},
    {0, NULL},
};

PyType_Spec query_predicate_anyof_type_spec = {
    .name = "tree_sitter.QueryPredicateAnyOf",
    .basicsize = sizeof(QueryPredicateAnyOf),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_predicate_anyof_slots,
};

// }}}

// QueryPredicateEqCapture {{{

static void query_predicate_eq_capture_dealloc(QueryPredicateEqCapture *self) {
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot query_predicate_eq_capture_slots[] = {
    {Py_tp_doc, ""},
    {Py_tp_dealloc, query_predicate_eq_capture_dealloc},
    {0, NULL},
};

PyType_Spec query_predicate_eq_capture_type_spec = {
    .name = "tree_sitter.QueryPredicateEqCapture",
    .basicsize = sizeof(QueryPredicateEqCapture),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_predicate_eq_capture_slots,
};

// }}}

// QueryPredicateEqString {{{

static void query_predicate_eq_string_dealloc(QueryPredicateEqString *self) {
    Py_XDECREF(self->string_value);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot query_predicate_eq_string_slots[] = {
    {Py_tp_doc, ""},
    {Py_tp_dealloc, query_predicate_eq_string_dealloc},
    {0, NULL},
};

PyType_Spec query_predicate_eq_string_type_spec = {
    .name = "tree_sitter.QueryPredicateEqString",
    .basicsize = sizeof(QueryPredicateEqString),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_predicate_eq_string_slots,
};

// }}}

// QueryPredicateMatch {{{

static void query_predicate_match_dealloc(QueryPredicateMatch *self) {
    Py_XDECREF(self->pattern);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot query_predicate_match_slots[] = {
    {Py_tp_doc, ""},
    {Py_tp_dealloc, query_predicate_match_dealloc},
    {0, NULL},
};

PyType_Spec query_predicate_match_type_spec = {
    .name = "tree_sitter.QueryPredicateMatch",
    .basicsize = sizeof(QueryPredicateMatch),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_predicate_match_slots,
};

// }}}

// QueryPredicateGeneric {{{

static void query_predicate_generic_dealloc(QueryPredicateGeneric *self) {
    Py_XDECREF(self->predicate);
    Py_XDECREF(self->arguments);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot query_predicate_generic_slots[] = {
    {Py_tp_doc, ""},
    {Py_tp_dealloc, query_predicate_generic_dealloc},
    {0, NULL},
};

PyType_Spec query_predicate_generic_type_spec = {
    .name = "tree_sitter.QueryPredicateGeneric",
    .basicsize = sizeof(QueryPredicateGeneric),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_predicate_generic_slots,
};

// }}}
