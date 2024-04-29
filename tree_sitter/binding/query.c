#include "query.h"
#include "node.h"

// QueryCapture {{{

static inline PyObject *query_capture_new_internal(ModuleState *state, TSQueryCapture capture) {
    QueryCapture *self = PyObject_New(QueryCapture, state->query_capture_type);
    if (self == NULL) {
        return NULL;
    }
    self->capture = capture;
    return PyObject_Init((PyObject *)self, state->query_capture_type);
}

void capture_dealloc(QueryCapture *self) { Py_TYPE(self)->tp_free(self); }

static PyType_Slot query_capture_type_slots[] = {
    {Py_tp_doc, "A query capture"},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, capture_dealloc},
    {0, NULL},
};

PyType_Spec query_capture_type_spec = {
    .name = "tree_sitter.Capture",
    .basicsize = sizeof(QueryCapture),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_capture_type_slots,
};

// }}}

// QueryMatch {{{

static inline PyObject *query_match_new_internal(ModuleState *state, TSQueryMatch match) {
    QueryMatch *self = PyObject_New(QueryMatch, state->query_match_type);
    if (self == NULL) {
        return NULL;
    }
    self->match = match;
    self->captures = PyList_New(0);
    self->pattern_index = 0;
    return PyObject_Init((PyObject *)self, state->query_match_type);
}

void match_dealloc(QueryMatch *self) { Py_TYPE(self)->tp_free(self); }

static PyType_Slot query_match_type_slots[] = {
    {Py_tp_doc, "A query match"},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, match_dealloc},
    {0, NULL},
};

PyType_Spec query_match_type_spec = {
    .name = "tree_sitter.QueryMatch",
    .basicsize = sizeof(QueryMatch),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = query_match_type_slots,
};

// }}}

// TODO(0.23): refactor predicate API

// CaptureEqCapture {{{

static inline PyObject *capture_eq_capture_new_internal(ModuleState *state,
                                                        uint32_t capture1_value_id,
                                                        uint32_t capture2_value_id,
                                                        int is_positive) {
    CaptureEqCapture *self = PyObject_New(CaptureEqCapture, state->capture_eq_capture_type);
    if (self == NULL) {
        return NULL;
    }
    self->capture1_value_id = capture1_value_id;
    self->capture2_value_id = capture2_value_id;
    self->is_positive = is_positive;
    return PyObject_Init((PyObject *)self, state->capture_eq_capture_type);
}

void capture_eq_capture_dealloc(CaptureEqCapture *self) { Py_TYPE(self)->tp_free(self); }

static PyType_Slot capture_eq_capture_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #eq? @capture1 @capture2"},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, capture_eq_capture_dealloc},
    {0, NULL},
};

PyType_Spec capture_eq_capture_type_spec = {
    .name = "tree_sitter.CaptureEqCapture",
    .basicsize = sizeof(CaptureEqCapture),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = capture_eq_capture_type_slots,
};

// }}}

// CaptureEqString {{{

static inline PyObject *capture_eq_string_new_internal(ModuleState *state,
                                                       uint32_t capture_value_id,
                                                       const char *string_value, int is_positive) {
    CaptureEqString *self = PyObject_New(CaptureEqString, state->capture_eq_string_type);
    if (self == NULL) {
        return NULL;
    }
    self->capture_value_id = capture_value_id;
    self->string_value = PyBytes_FromString(string_value);
    self->is_positive = is_positive;
    return PyObject_Init((PyObject *)self, state->capture_eq_string_type);
}

void capture_eq_string_dealloc(CaptureEqString *self) {
    Py_XDECREF(self->string_value);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot capture_eq_string_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #eq? @capture string"},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, capture_eq_string_dealloc},
    {0, NULL},
};

PyType_Spec capture_eq_string_type_spec = {
    .name = "tree_sitter.CaptureEqString",
    .basicsize = sizeof(CaptureEqString),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = capture_eq_string_type_slots,
};

// }}}

// CaptureMatchString {{{

static inline PyObject *capture_match_string_new_internal(ModuleState *state,
                                                          uint32_t capture_value_id,
                                                          const char *string_value,
                                                          int is_positive) {
    CaptureMatchString *self = PyObject_New(CaptureMatchString, state->capture_match_string_type);
    if (self == NULL) {
        return NULL;
    }
    self->capture_value_id = capture_value_id;
    self->regex = PyObject_CallFunction(state->re_compile, "s", string_value);
    self->is_positive = is_positive;
    return PyObject_Init((PyObject *)self, state->capture_match_string_type);
}

void capture_match_string_dealloc(CaptureMatchString *self) {
    Py_XDECREF(self->regex);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot capture_match_string_type_slots[] = {
    {Py_tp_doc, "Text predicate of the form #match? @capture regex"},
    {Py_tp_new, NULL},
    {Py_tp_dealloc, capture_match_string_dealloc},
    {0, NULL},
};

PyType_Spec capture_match_string_type_spec = {
    .name = "tree_sitter.CaptureMatchString",
    .basicsize = sizeof(CaptureMatchString),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
    .slots = capture_match_string_type_slots,
};

// }}}

// Query {{{

static inline Node *node_for_capture_index(ModuleState *state, uint32_t index, TSQueryMatch match,
                                           Tree *tree) {
    for (unsigned i = 0; i < match.capture_count; ++i) {
        TSQueryCapture capture = match.captures[i];
        if (capture.index == index) {
            return (Node *)node_new_internal(state, capture.node, (PyObject *)tree);
        }
    }
    return NULL;
}

static bool satisfies_text_predicates(Query *query, TSQueryMatch match, Tree *tree) {
    ModuleState *state = GET_MODULE_STATE(query);
    PyObject *pattern_text_predicates = PyList_GetItem(query->text_predicates, match.pattern_index);
    // if there is no source, ignore the text predicates
    if (tree->source == Py_None || tree->source == NULL) {
        return true;
    }

    Node *node1 = NULL, *node2 = NULL;
    PyObject *node1_text = NULL, *node2_text = NULL;
    // check if all text_predicates are satisfied
    for (Py_ssize_t j = 0; j < PyList_Size(pattern_text_predicates); ++j) {
        PyObject *self = PyList_GetItem(pattern_text_predicates, j);
        int is_satisfied;
        // TODO(0.23): refactor into separate functions
        if (IS_INSTANCE(self, capture_eq_capture_type)) {
            uint32_t capture1_value_id = ((CaptureEqCapture *)self)->capture1_value_id;
            uint32_t capture2_value_id = ((CaptureEqCapture *)self)->capture2_value_id;
            node1 = node_for_capture_index(state, capture1_value_id, match, tree);
            node2 = node_for_capture_index(state, capture2_value_id, match, tree);
            if (node1 == NULL || node2 == NULL) {
                is_satisfied = true;
                if (node1 != NULL) {
                    Py_XDECREF(node1);
                }
                if (node2 != NULL) {
                    Py_XDECREF(node2);
                }
            } else {
                node1_text = node_get_text(node1, NULL);
                node2_text = node_get_text(node2, NULL);
                if (node1_text == NULL || node2_text == NULL) {
                    goto error;
                }
                is_satisfied = PyObject_RichCompareBool(node1_text, node2_text, Py_EQ) ==
                               ((CaptureEqCapture *)self)->is_positive;
                Py_XDECREF(node1);
                Py_XDECREF(node2);
                Py_XDECREF(node1_text);
                Py_XDECREF(node2_text);
            }
            if (!is_satisfied) {
                return false;
            }
        } else if (IS_INSTANCE(self, capture_eq_string_type)) {
            uint32_t capture_value_id = ((CaptureEqString *)self)->capture_value_id;
            node1 = node_for_capture_index(state, capture_value_id, match, tree);
            if (node1 == NULL) {
                is_satisfied = true;
            } else {
                node1_text = node_get_text(node1, NULL);
                if (node1_text == NULL) {
                    goto error;
                }
                PyObject *string_value = ((CaptureEqString *)self)->string_value;
                is_satisfied = PyObject_RichCompareBool(node1_text, string_value, Py_EQ) ==
                               ((CaptureEqString *)self)->is_positive;
                Py_XDECREF(node1_text);
            }
            Py_XDECREF(node1);
            if (!is_satisfied) {
                return false;
            }
        } else if (IS_INSTANCE(self, capture_match_string_type)) {
            uint32_t capture_value_id = ((CaptureMatchString *)self)->capture_value_id;
            node1 = node_for_capture_index(state, capture_value_id, match, tree);
            if (node1 == NULL) {
                is_satisfied = true;
            } else {
                node1_text = node_get_text(node1, NULL);
                if (node1_text == NULL) {
                    goto error;
                }
                PyObject *search_result =
                    PyObject_CallMethod(((CaptureMatchString *)self)->regex, "search", "s",
                                        PyBytes_AsString(node1_text));
                Py_XDECREF(node1_text);
                is_satisfied = (search_result != NULL && search_result != Py_None) ==
                               ((CaptureMatchString *)self)->is_positive;
                if (search_result != NULL) {
                    Py_DECREF(search_result);
                }
            }
            Py_XDECREF(node1);
            if (!is_satisfied) {
                return false;
            }
        }
    }
    return true;

error:
    Py_XDECREF(node1);
    Py_XDECREF(node2);
    Py_XDECREF(node1_text);
    Py_XDECREF(node2_text);
    return false;
}

static inline bool is_valid_predicate_char(char ch) {
    return Py_ISALNUM(ch) || ch == '-' || ch == '_' || ch == '?' || ch == '.';
}

static inline bool is_list_capture(TSQuery *query, TSQueryMatch *match,
                                   unsigned int capture_index) {
    TSQuantifier quantifier = ts_query_capture_quantifier_for_id(
        query, match->pattern_index, match->captures[capture_index].index);
    return quantifier == TSQuantifierZeroOrMore || quantifier == TSQuantifierOneOrMore;
}

PyObject *query_new(PyTypeObject *cls, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
    Query *query = (Query *)cls->tp_alloc(cls, 0);
    if (query == NULL) {
        return NULL;
    }

    PyObject *language_obj;
    char *source;
    Py_ssize_t length;
    ModuleState *state = (ModuleState *)PyType_GetModuleState(cls);
    if (!PyArg_ParseTuple(args, "O!s#:__new__", state->language_type, &language_obj, &source,
                          &length)) {
        return NULL;
    }

    uint32_t error_offset;
    TSQueryError error_type;
    PyObject *pattern_text_predicates = NULL;
    TSLanguage *language_id = ((Language *)language_obj)->language;
    query->query = ts_query_new(language_id, source, length, &error_offset, &error_type);

    if (!query->query) {
        char *word_start = &source[error_offset];
        char *word_end = word_start;
        while (word_end < &source[length] && is_valid_predicate_char(*word_end)) {
            ++word_end;
        }
        char c = *word_end;
        *word_end = 0;
        // TODO(0.23): implement custom error types
        switch (error_type) {
        case TSQueryErrorNodeType:
            PyErr_Format(PyExc_NameError, "Invalid node type %s", &source[error_offset]);
            break;
        case TSQueryErrorField:
            PyErr_Format(PyExc_NameError, "Invalid field name %s", &source[error_offset]);
            break;
        case TSQueryErrorCapture:
            PyErr_Format(PyExc_NameError, "Invalid capture name %s", &source[error_offset]);
            break;
        default:
            PyErr_Format(PyExc_SyntaxError, "Invalid syntax at offset %u", error_offset);
        }
        *word_end = c;
        goto error;
    }

    unsigned n = ts_query_capture_count(query->query);
    query->capture_names = PyList_New(n);
    for (unsigned i = 0; i < n; ++i) {
        unsigned length;
        const char *capture_name = ts_query_capture_name_for_id(query->query, i, &length);
        PyList_SetItem(query->capture_names, i, PyUnicode_FromStringAndSize(capture_name, length));
    }

    unsigned pattern_count = ts_query_pattern_count(query->query);
    query->text_predicates = PyList_New(pattern_count);
    if (query->text_predicates == NULL) {
        goto error;
    }

    for (unsigned i = 0; i < pattern_count; ++i) {
        unsigned length;
        const TSQueryPredicateStep *predicate_step =
            ts_query_predicates_for_pattern(query->query, i, &length);
        pattern_text_predicates = PyList_New(0);
        if (pattern_text_predicates == NULL) {
            goto error;
        }
        for (unsigned j = 0; j < length; ++j) {
            unsigned predicate_len = 0;
            while ((predicate_step + predicate_len)->type != TSQueryPredicateStepTypeDone) {
                ++predicate_len;
            }

            if (predicate_step->type != TSQueryPredicateStepTypeString) {
                PyErr_Format(
                    PyExc_RuntimeError,
                    "Capture predicate must start with a string i=%d/pattern_count=%d "
                    "j=%d/length=%d predicate_step->type=%d TSQueryPredicateStepTypeDone=%d "
                    "TSQueryPredicateStepTypeCapture=%d TSQueryPredicateStepTypeString=%d",
                    i, pattern_count, j, length, predicate_step->type, TSQueryPredicateStepTypeDone,
                    TSQueryPredicateStepTypeCapture, TSQueryPredicateStepTypeString);
                goto error;
            }

            // Build a predicate for each of the supported predicate function names
            unsigned length;
            const char *operator_name =
                ts_query_string_value_for_id(query->query, predicate_step->value_id, &length);
            if (strcmp(operator_name, "eq?") == 0 || strcmp(operator_name, "not-eq?") == 0) {
                if (predicate_len != 3) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "Wrong number of arguments to #eq? or #not-eq? predicate");
                    goto error;
                }
                if (predicate_step[1].type != TSQueryPredicateStepTypeCapture) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "First argument to #eq? or #not-eq? must be a capture name");
                    goto error;
                }
                int is_positive = strcmp(operator_name, "eq?") == 0;
                switch (predicate_step[2].type) {
                case TSQueryPredicateStepTypeCapture:;
                    CaptureEqCapture *capture_eq_capture_predicate =
                        (CaptureEqCapture *)capture_eq_capture_new_internal(
                            state, predicate_step[1].value_id, predicate_step[2].value_id,
                            is_positive);
                    if (capture_eq_capture_predicate == NULL) {
                        goto error;
                    }
                    PyList_Append(pattern_text_predicates,
                                  (PyObject *)capture_eq_capture_predicate);
                    Py_DECREF(capture_eq_capture_predicate);
                    break;
                case TSQueryPredicateStepTypeString:;
                    const char *string_value = ts_query_string_value_for_id(
                        query->query, predicate_step[2].value_id, &length);
                    CaptureEqString *capture_eq_string_predicate =
                        (CaptureEqString *)capture_eq_string_new_internal(
                            state, predicate_step[1].value_id, string_value, is_positive);
                    if (capture_eq_string_predicate == NULL) {
                        goto error;
                    }
                    PyList_Append(pattern_text_predicates, (PyObject *)capture_eq_string_predicate);
                    Py_DECREF(capture_eq_string_predicate);
                    break;
                default:
                    PyErr_SetString(PyExc_RuntimeError, "Second argument to #eq? or #not-eq? must "
                                                        "be a capture name or a string literal");
                    goto error;
                }
            } else if (strcmp(operator_name, "match?") == 0 ||
                       strcmp(operator_name, "not-match?") == 0) {
                if (predicate_len != 3) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "Wrong number of arguments to #match? or #not-match? predicate");
                    goto error;
                }
                if (predicate_step[1].type != TSQueryPredicateStepTypeCapture) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "First argument to #match? or #not-match? must be a capture name");
                    goto error;
                }
                if (predicate_step[2].type != TSQueryPredicateStepTypeString) {
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        "Second argument to #match? or #not-match? must be a regex string");
                    goto error;
                }
                const char *string_value =
                    ts_query_string_value_for_id(query->query, predicate_step[2].value_id, &length);
                int is_positive = strcmp(operator_name, "match?") == 0;
                CaptureMatchString *capture_match_string_predicate =
                    (CaptureMatchString *)capture_match_string_new_internal(
                        state, predicate_step[1].value_id, string_value, is_positive);
                if (capture_match_string_predicate == NULL) {
                    goto error;
                }
                PyList_Append(pattern_text_predicates, (PyObject *)capture_match_string_predicate);
                Py_DECREF(capture_match_string_predicate);
            }
            predicate_step += predicate_len + 1;
            j += predicate_len;
        }
        PyList_SetItem(query->text_predicates, i, pattern_text_predicates);
    }
    return (PyObject *)query;

error:
    query_dealloc(query);
    Py_XDECREF(pattern_text_predicates);
    return NULL;
}

void query_dealloc(Query *self) {
    if (self->query) {
        ts_query_delete(self->query);
    }
    Py_XDECREF(self->capture_names);
    Py_XDECREF(self->text_predicates);
    Py_TYPE(self)->tp_free(self);
}

PyObject *query_matches(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {
        "node", "start_point", "end_point", "start_byte", "end_byte", NULL,
    };
    PyObject *node_obj;
    TSPoint start_point = {.row = 0, .column = 0};
    TSPoint end_point = {.row = UINT32_MAX, .column = UINT32_MAX};
    unsigned start_byte = 0, end_byte = UINT32_MAX;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$(II)(II)II:matches", keywords,
                                     state->node_type, &node_obj, &start_point.row,
                                     &start_point.column, &end_point.row, &end_point.column,
                                     &start_byte, &end_byte)) {
        return NULL;
    }

    Node *node = (Node *)node_obj;
    ts_query_cursor_set_byte_range(state->query_cursor, start_byte, end_byte);
    ts_query_cursor_set_point_range(state->query_cursor, start_point, end_point);
    ts_query_cursor_exec(state->query_cursor, self->query, node->node);

    QueryMatch *match = NULL;
    PyObject *result = PyList_New(0);
    if (result == NULL) {
        goto error;
    }

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(state->query_cursor, &_match)) {
        match = (QueryMatch *)query_match_new_internal(state, _match);
        if (match == NULL) {
            goto error;
        }
        PyObject *captures_for_match = PyDict_New();
        if (captures_for_match == NULL) {
            goto error;
        }
        bool is_satisfied = satisfies_text_predicates(self, _match, (Tree *)node->tree);
        for (unsigned i = 0; i < _match.capture_count; ++i) {
            QueryCapture *capture =
                (QueryCapture *)query_capture_new_internal(state, _match.captures[i]);
            if (capture == NULL) {
                Py_XDECREF(captures_for_match);
                goto error;
            }
            if (is_satisfied) {
                PyObject *capture_name =
                    PyList_GetItem(self->capture_names, capture->capture.index);
                PyObject *capture_node =
                    node_new_internal(state, capture->capture.node, node->tree);

                if (is_list_capture(self->query, &_match, i)) {
                    PyObject *defult_new_capture_list = PyList_New(0);
                    PyObject *capture_list = PyDict_SetDefault(captures_for_match, capture_name,
                                                               defult_new_capture_list);
                    Py_INCREF(capture_list);
                    Py_DECREF(defult_new_capture_list);
                    PyList_Append(capture_list, capture_node);
                    Py_DECREF(capture_list);
                } else {
                    PyDict_SetItem(captures_for_match, capture_name, capture_node);
                }
                Py_XDECREF(capture_node);
            }
            Py_XDECREF(capture);
        }
        PyObject *pattern_index = PyLong_FromLong(_match.pattern_index);
        PyObject *tuple_match = PyTuple_Pack(2, pattern_index, captures_for_match);
        PyList_Append(result, tuple_match);
        Py_XDECREF(tuple_match);
        Py_XDECREF(pattern_index);
        Py_XDECREF(captures_for_match);
        Py_XDECREF(match);
    }
    return result;

error:
    Py_XDECREF(result);
    Py_XDECREF(match);
    return NULL;
}

PyObject *query_captures(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {
        "node", "start_point", "end_point", "start_byte", "end_byte", NULL,
    };
    PyObject *node_obj;
    TSPoint start_point = {0, 0};
    TSPoint end_point = {UINT32_MAX, UINT32_MAX};
    unsigned start_byte = 0, end_byte = UINT32_MAX;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|$(II)(II)II:captures", keywords,
                                     state->node_type, &node_obj, &start_point.row,
                                     &start_point.column, &end_point.row, &end_point.column,
                                     &start_byte, &end_byte)) {
        return NULL;
    }

    Node *node = (Node *)node_obj;
    ts_query_cursor_set_byte_range(state->query_cursor, start_byte, end_byte);
    ts_query_cursor_set_point_range(state->query_cursor, start_point, end_point);
    ts_query_cursor_exec(state->query_cursor, self->query, node->node);

    QueryCapture *capture = NULL;
    PyObject *result = PyList_New(0);
    if (result == NULL) {
        goto error;
    }

    uint32_t capture_index;
    TSQueryMatch match;
    while (ts_query_cursor_next_capture(state->query_cursor, &match, &capture_index)) {
        capture = (QueryCapture *)query_capture_new_internal(state, match.captures[capture_index]);
        if (capture == NULL) {
            goto error;
        }
        if (satisfies_text_predicates(self, match, (Tree *)node->tree)) {
            PyObject *capture_name = PyList_GetItem(self->capture_names, capture->capture.index);
            PyObject *capture_node = node_new_internal(state, capture->capture.node, node->tree);
            PyObject *item = PyTuple_Pack(2, capture_node, capture_name);
            if (item == NULL) {
                goto error;
            }
            Py_XDECREF(capture_node);
            PyList_Append(result, item);
            Py_XDECREF(item);
        }
        Py_XDECREF(capture);
    }
    return result;

error:
    Py_XDECREF(result);
    Py_XDECREF(capture);
    return NULL;
}

#define QUERY_METHOD_SIGNATURE                                                                     \
    "(self, node, *, start_point=None, end_point=None, start_byte=None, end_byte=None)\n--\n\n"

PyDoc_STRVAR(query_matches_doc,
             "matches" QUERY_METHOD_SIGNATURE "Get a list of *matches* within the given node.\n\n"
             "You can optionally limit the matches to a range of row/column points or of bytes.");
PyDoc_STRVAR(
    query_captures_doc,
    "captures" QUERY_METHOD_SIGNATURE "Get a list of *captures* within the given node.\n\n"
    "You can optionally limit the captures to a range of row/column points or of bytes." DOC_HINT
    "This method returns all of the captures while :meth:`matches` only returns the last match.");

static PyMethodDef query_methods[] = {
    {
        .ml_name = "matches",
        .ml_meth = (PyCFunction)query_matches,
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = query_matches_doc,
    },
    {
        .ml_name = "captures",
        .ml_meth = (PyCFunction)query_captures,
        .ml_flags = METH_KEYWORDS | METH_VARARGS,
        .ml_doc = query_captures_doc,
    },
    {NULL},
};

static PyType_Slot query_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("A set of patterns that match nodes in a syntax tree.")},
    {Py_tp_new, query_new},
    {Py_tp_dealloc, query_dealloc},
    {Py_tp_methods, query_methods},
    {0, NULL},
};

PyType_Spec query_type_spec = {
    .name = "tree_sitter.Query",
    .basicsize = sizeof(Query),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = query_type_slots,
};

// }}}
