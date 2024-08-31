#include "types.h"

#include <string.h>

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

bool query_satisfies_predicates(Query *query, TSQueryMatch match, Tree *tree, PyObject *callable);

#define QUERY_ERROR(...) PyErr_Format(state->query_error, __VA_ARGS__)

static inline bool is_valid_identifier_char(char ch) { return Py_ISALNUM(ch) || ch == '_'; }

static inline bool is_valid_predicate_char(char ch) {
    return Py_ISALNUM(ch) || ch == '-' || ch == '_' || ch == '?' || ch == '.' || ch == '!';
}

void query_dealloc(Query *self) {
    if (self->query) {
        ts_query_delete(self->query);
    }
    if (self->cursor) {
        ts_query_cursor_delete(self->cursor);
    }
    Py_XDECREF(self->capture_names);
    Py_XDECREF(self->predicates);
    Py_XDECREF(self->settings);
    Py_XDECREF(self->assertions);
    Py_TYPE(self)->tp_free(self);
}

PyObject *query_new(PyTypeObject *cls, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
    Query *query = (Query *)cls->tp_alloc(cls, 0);
    if (query == NULL) {
        return NULL;
    }

    PyObject *language_obj;
    char *source;
    Py_ssize_t source_len;
    ModuleState *state = (ModuleState *)PyType_GetModuleState(cls);
    if (!PyArg_ParseTuple(args, "O!s#:__new__", state->language_type, &language_obj, &source,
                          &source_len)) {
        return NULL;
    }

    uint32_t error_offset;
    TSQueryError error_type;
    PyObject *pattern_predicates = NULL, *pattern_settings = NULL, *pattern_assertions = NULL;
    TSLanguage *language_id = ((Language *)language_obj)->language;
    query->query = ts_query_new(language_id, source, source_len, &error_offset, &error_type);
    query->cursor = ts_query_cursor_new();
    query->capture_names = NULL;
    query->predicates = NULL;
    query->settings = NULL;
    query->assertions = NULL;

    if (!query->query) {
        uint32_t start = 0, end = 0, row = 0, column;
#ifndef _MSC_VER
        char *line = strtok(source, "\n");
#else
        char *next_token = NULL;
        char *line = strtok_s(source, "\n", &next_token);
#endif
        while (line != NULL) {
            end = start + strlen(line) + 1;
            if (end > error_offset)
                break;
            start = end;
            row += 1;
#ifndef _MSC_VER
            line = strtok(NULL, "\n");
#else
            line = strtok_s(NULL, "\n", &next_token);
#endif
        }
        column = error_offset - start, end = 0;

        switch (error_type) {
            case TSQueryErrorSyntax: {
                if (error_offset < source_len) {
                    QUERY_ERROR("Invalid syntax at row %u, column %u", row, column);
                } else {
                    PyErr_SetString(state->query_error, "Unexpected EOF");
                }
                break;
            }
            case TSQueryErrorCapture: {
                while (is_valid_predicate_char(source[error_offset + end])) {
                    end += 1;
                }

                char *capture = PyMem_Calloc(end + 1, sizeof(char));
                memcpy(capture, &source[error_offset], end);
                QUERY_ERROR("Invalid capture name at row %u, column %u: %s", row, column, capture);
                PyMem_Free(capture);
                break;
            }
            case TSQueryErrorNodeType: {
                while (is_valid_identifier_char(source[error_offset + end])) {
                    end += 1;
                }

                char *node = PyMem_Calloc(end + 1, sizeof(char));
                memcpy(node, &source[error_offset], end);
                QUERY_ERROR("Invalid node type at row %u, column %u: %s", row, column, node);
                PyMem_Free(node);
                break;
            }
            case TSQueryErrorField: {
                while (is_valid_identifier_char(source[error_offset + end])) {
                    end += 1;
                }

                char *field = PyMem_Calloc(end + 1, sizeof(char));
                memcpy(field, &source[error_offset], end);
                QUERY_ERROR("Invalid field name at row %u, column %u: %s", row, column, field);
                PyMem_Free(field);
                break;
            }
            case TSQueryErrorStructure: {
                QUERY_ERROR("Impossible pattern at row %u, column %u", row, column);
                break;
            }
            default:
                Py_UNREACHABLE();
        }
        goto error;
    }

    uint32_t n = ts_query_capture_count(query->query), length;
    query->capture_names = PyList_New(n);
    for (uint32_t i = 0; i < n; ++i) {
        const char *capture_name = ts_query_capture_name_for_id(query->query, i, &length);
        PyObject *value = PyUnicode_FromStringAndSize(capture_name, length);
        PyList_SetItem(query->capture_names, i, value);
    }

    uint32_t pattern_count = ts_query_pattern_count(query->query);
    query->predicates = PyList_New(pattern_count);
    if (query->predicates == NULL) {
        goto error;
    }
    query->settings = PyList_New(pattern_count);
    if (query->settings == NULL) {
        goto error;
    }
    query->assertions = PyList_New(pattern_count);
    if (query->assertions == NULL) {
        goto error;
    }

    for (uint32_t i = 0; i < pattern_count; ++i) {
        uint32_t offset = ts_query_start_byte_for_pattern(query->query, i), row = 0, steps;
        for (uint32_t k = 0; k < offset; ++k) {
            if (source[k] == '\n') {
                row += 1;
            }
        }

        pattern_predicates = PyList_New(0);
        if (pattern_predicates == NULL) {
            goto error;
        }
        pattern_settings = PyDict_New();
        if (pattern_settings == NULL) {
            goto error;
        }
        pattern_assertions = PyDict_New();
        if (pattern_assertions == NULL) {
            goto error;
        }

        const TSQueryPredicateStep *predicate_step =
            ts_query_predicates_for_pattern(query->query, i, &steps);
        for (uint32_t j = 0; j < steps; ++j) {
            uint32_t predicate_len = 0;
            while ((predicate_step + predicate_len)->type != TSQueryPredicateStepTypeDone) {
                ++predicate_len;
            }

            if (predicate_step->type != TSQueryPredicateStepTypeString) {
                const char *capture_name =
                    ts_query_capture_name_for_id(query->query, predicate_step->value_id, &length);
                QUERY_ERROR("Invalid predicate in pattern at row %u: @%s", row, capture_name);
                goto error;
            }

            const char *predicate_name =
                ts_query_string_value_for_id(query->query, predicate_step->value_id, &length);

            if (strncmp(predicate_name, "eq?", length) == 0 ||
                strncmp(predicate_name, "not-eq?", length) == 0 ||
                strncmp(predicate_name, "any-eq?", length) == 0 ||
                strncmp(predicate_name, "any-not-eq?", length) == 0) {
                if (predicate_len != 3) {
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "#%s expects 2 arguments, got %u",
                                row, predicate_name, predicate_len - 1);
                    goto error;
                }

                if ((predicate_step + 1)->type != TSQueryPredicateStepTypeCapture) {
                    const char *first_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "first argument to #%s must be a capture name, got \"%s\"",
                                row, predicate_name, first_arg);
                    goto error;
                }

                PyObject *predicate_obj;
                bool is_any = strncmp("any", predicate_name, 3) == 0;
                bool is_positive = strncmp(predicate_name, "eq?", length) == 0 ||
                                   strncmp(predicate_name, "any-eq?", length) == 0;
                if ((predicate_step + 2)->type == TSQueryPredicateStepTypeCapture) {
                    QueryPredicateEqCapture *predicate = PyObject_New(
                        QueryPredicateEqCapture, state->query_predicate_eq_capture_type);
                    predicate->capture1_id = (predicate_step + 1)->value_id;
                    predicate->capture2_id = (predicate_step + 2)->value_id;
                    predicate->is_any = is_any;
                    predicate->is_positive = is_positive;
                    predicate_obj = PyObject_Init((PyObject *)predicate,
                                                  state->query_predicate_eq_capture_type);
                } else {
                    QueryPredicateEqString *predicate =
                        PyObject_New(QueryPredicateEqString, state->query_predicate_eq_string_type);
                    predicate->capture_id = (predicate_step + 1)->value_id;
                    const char *second_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 2)->value_id, &length);
                    predicate->string_value = PyBytes_FromStringAndSize(second_arg, length);
                    predicate->is_any = is_any;
                    predicate->is_positive = is_positive;
                    predicate_obj =
                        PyObject_Init((PyObject *)predicate, state->query_predicate_eq_string_type);
                }
                PyList_Append(pattern_predicates, predicate_obj);
                Py_XDECREF(predicate_obj);
            } else if (strncmp(predicate_name, "match?", length) == 0 ||
                       strncmp(predicate_name, "not-match?", length) == 0 ||
                       strncmp(predicate_name, "any-match?", length) == 0 ||
                       strncmp(predicate_name, "any-not-match?", length) == 0) {

                if (predicate_len != 3) {
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "#%s expects 2 arguments, got %u",
                                row, predicate_name, predicate_len - 1);
                    goto error;
                }

                if ((predicate_step + 1)->type != TSQueryPredicateStepTypeCapture) {
                    const char *first_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "first argument to #%s must be a capture name, got \"%s\"",
                                row, predicate_name, first_arg);
                    goto error;
                }

                if ((predicate_step + 2)->type != TSQueryPredicateStepTypeString) {
                    const char *second_arg = ts_query_capture_name_for_id(
                        query->query, (predicate_step + 2)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "second argument to #%s must be a string literal, got @%s",
                                row, predicate_name, second_arg);
                    goto error;
                }

                bool is_any = strncmp("any", predicate_name, 3) == 0;
                bool is_positive = strncmp(predicate_name, "match?", length) == 0 ||
                                   strncmp(predicate_name, "any-match?", length) == 0;
                const char *second_arg = ts_query_string_value_for_id(
                    query->query, (predicate_step + 2)->value_id, &length);
                PyObject *pattern =
                    PyObject_CallFunction(state->re_compile, "s#", second_arg, length);
                if (pattern == NULL) {
                    const char *msg =
                        "Invalid predicate in pattern at row %u: regular expression error";
#if PY_MINOR_VERSION < 12
                    PyObject *etype, *cause, *exc, *trace;
                    PyErr_Fetch(&etype, &cause, &trace);
                    PyErr_NormalizeException(&etype, &cause, &trace);
                    if (trace != NULL) {
                        PyException_SetTraceback(cause, trace);
                        Py_DECREF(trace);
                    }
                    Py_DECREF(etype);
                    PyErr_Format(state->query_error, msg, row);
                    PyErr_Fetch(&etype, &exc, &trace);
                    PyErr_NormalizeException(&etype, &exc, &trace);
                    Py_INCREF(cause);
                    PyException_SetCause(exc, cause);
                    PyException_SetContext(exc, cause);
                    PyErr_Restore(etype, exc, trace);
#else
                    PyObject *cause = PyErr_GetRaisedException();
                    PyErr_Format(state->query_error, msg, row);
                    PyObject *exc = PyErr_GetRaisedException();
                    PyException_SetCause(exc, Py_NewRef(cause));
                    PyException_SetContext(exc, Py_NewRef(cause));
                    Py_DECREF(cause);
                    PyErr_SetRaisedException(exc);
#endif
                    goto error;
                }

                QueryPredicateMatch *predicate =
                    PyObject_New(QueryPredicateMatch, state->query_predicate_match_type);
                predicate->capture_id = (predicate_step + 1)->value_id;
                predicate->pattern = pattern;
                predicate->is_any = is_any;
                predicate->is_positive = is_positive;
                PyObject *predicate_obj =
                    PyObject_Init((PyObject *)predicate, state->query_predicate_match_type);
                PyList_Append(pattern_predicates, predicate_obj);
                Py_XDECREF(predicate_obj);
            } else if (strncmp(predicate_name, "any-of?", length) == 0 ||
                       strncmp(predicate_name, "not-any-of?", length) == 0) {
                if (predicate_len < 3) {
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "#%s expects at least 2 arguments, got %u",
                                row, predicate_name, predicate_len - 1);
                    goto error;
                }

                if ((predicate_step + 1)->type != TSQueryPredicateStepTypeCapture) {
                    const char *first_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "first argument to #%s must be a capture name, got \"%s\"",
                                row, predicate_name, first_arg);
                    goto error;
                }

                bool is_positive = length == 7; // any-of?
                PyObject *values = PyList_New(predicate_len - 2);
                for (uint32_t k = 2; k < predicate_len; ++k) {
                    if ((predicate_step + k)->type != TSQueryPredicateStepTypeString) {
                        const char *arg = ts_query_capture_name_for_id(
                            query->query, (predicate_step + k)->value_id, &length);
                        QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                    "arguments to #%s must be string literals, got @%s",
                                    row, predicate_name, arg);
                        Py_DECREF(values);
                        goto error;
                    }
                    const char *arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + k)->value_id, &length);
                    PyList_SetItem(values, k - 2, PyBytes_FromStringAndSize(arg, length));
                }

                QueryPredicateAnyOf *predicate =
                    PyObject_New(QueryPredicateAnyOf, state->query_predicate_anyof_type);
                predicate->is_positive = is_positive;
                predicate->values = values;
                PyObject *predicate_obj =
                    PyObject_Init((PyObject *)predicate, state->query_predicate_anyof_type);
                PyList_Append(pattern_predicates, predicate_obj);
                Py_XDECREF(predicate_obj);
            } else if (strncmp(predicate_name, "is?", length) == 0 ||
                       strncmp(predicate_name, "is-not?", length) == 0) {
                if (predicate_len == 1 || predicate_len > 3) {
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "#%s expects 1-2 arguments, got %u",
                                row, predicate_name, predicate_len - 1);
                    goto error;
                }

                if ((predicate_step + 1)->type != TSQueryPredicateStepTypeString) {
                    const char *first_arg = ts_query_capture_name_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "first argument to #%s must be a string literal, got @%s",
                                row, predicate_name, first_arg);
                    goto error;
                }

                const char *first_arg = ts_query_string_value_for_id(
                    query->query, (predicate_step + 1)->value_id, &length);
                PyObject *is_positive = PyBool_FromLong(length == 3); // is?
                if (predicate_len == 2) {
                    PyObject *assertion = PyTuple_Pack(2, Py_None, is_positive);
                    Py_DECREF(is_positive);
                    PyDict_SetItemString(pattern_assertions, first_arg, assertion);
                    Py_DECREF(assertion);
                } else if ((predicate_step + 2)->type == TSQueryPredicateStepTypeString) {
                    const char *second_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 2)->value_id, &length);
                    PyObject *value = PyUnicode_FromString(second_arg);
                    PyObject *assertion = PyTuple_Pack(2, value, is_positive);
                    Py_DECREF(value);
                    Py_DECREF(is_positive);
                    PyDict_SetItemString(pattern_assertions, first_arg, assertion);
                    Py_DECREF(assertion);
                } else {
                    const char *second_arg = ts_query_capture_name_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "second argument to #%s must be a string literal, got @%s",
                                row, predicate_name, second_arg);
                    goto error;
                }

            } else if (strncmp(predicate_name, "set!", length) == 0) {
                if (predicate_len == 1 || predicate_len > 3) {
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "#%s expects 1-2 arguments, got %u",
                                row, predicate_name, predicate_len - 1);
                    goto error;
                }

                if ((predicate_step + 1)->type != TSQueryPredicateStepTypeString) {
                    const char *first_arg = ts_query_capture_name_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "first argument to #%s must be a string literal, got @%s",
                                row, predicate_name, first_arg);
                    goto error;
                }

                const char *first_arg = ts_query_string_value_for_id(
                    query->query, (predicate_step + 1)->value_id, &length);
                if (predicate_len == 2) {
                    PyDict_SetItemString(pattern_settings, first_arg, Py_None);
                } else if ((predicate_step + 2)->type == TSQueryPredicateStepTypeString) {
                    const char *second_arg = ts_query_string_value_for_id(
                        query->query, (predicate_step + 2)->value_id, &length);
                    PyObject *value = PyUnicode_FromString(second_arg);
                    PyDict_SetItemString(pattern_settings, first_arg, value);
                    Py_DECREF(value);
                } else {
                    const char *second_arg = ts_query_capture_name_for_id(
                        query->query, (predicate_step + 1)->value_id, &length);
                    QUERY_ERROR("Invalid predicate in pattern at row %u: "
                                "second argument to #%s must be a string literal, got @%s",
                                row, predicate_name, second_arg);
                    goto error;
                }
            } else {
                QueryPredicateGeneric *predicate =
                    PyObject_New(QueryPredicateGeneric, state->query_predicate_generic_type);
                predicate->predicate = PyUnicode_FromStringAndSize(predicate_name, length);
                predicate->arguments = PyList_New(predicate_len - 1);
                for (uint32_t k = 1; k < predicate_len; ++k) {
                    PyObject *item;
                    if ((predicate_step + k)->type == TSQueryPredicateStepTypeCapture) {
                        const char *arg_value = ts_query_capture_name_for_id(
                            query->query, (predicate_step + k)->value_id, &length);
                        item = PyTuple_Pack(2, PyUnicode_FromStringAndSize(arg_value, length),
                                            PyUnicode_FromString("capture"));
                    } else {
                        const char *arg_value = ts_query_string_value_for_id(
                            query->query, (predicate_step + k)->value_id, &length);
                        item = PyTuple_Pack(2, PyUnicode_FromStringAndSize(arg_value, length),
                                            PyUnicode_FromString("string"));
                    }
                    PyList_SetItem(predicate->arguments, k - 1, item);
                }
                PyObject *predicate_obj =
                    PyObject_Init((PyObject *)predicate, state->query_predicate_generic_type);
                PyList_Append(pattern_predicates, predicate_obj);
                Py_XDECREF(predicate_obj);
            }

            j += predicate_len;
            predicate_step += predicate_len + 1;
        }

        PyList_SetItem(query->predicates, i, pattern_predicates);
        PyList_SetItem(query->settings, i, pattern_settings);
        PyList_SetItem(query->assertions, i, pattern_assertions);
    }

    return (PyObject *)query;

error:
    Py_XDECREF(pattern_predicates);
    Py_XDECREF(pattern_settings);
    Py_XDECREF(pattern_assertions);
    query_dealloc(query);
    return NULL;
}

PyObject *query_matches(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {"node", "predicate", NULL};
    PyObject *node_obj, *predicate = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O:matches", keywords, state->node_type,
                                     &node_obj, &predicate)) {
        return NULL;
    }
    if (predicate != NULL && !PyCallable_Check(predicate)) {
        PyErr_Format(PyExc_TypeError, "Second argument to captures must be a callable, not %s",
                     predicate->ob_type->tp_name);
        return NULL;
    }

    PyObject *result = PyList_New(0);
    if (result == NULL) {
        return NULL;
    }

    TSQueryMatch match;
    Node *node = (Node *)node_obj;
    ts_query_cursor_exec(self->cursor, self->query, node->node);
    while (ts_query_cursor_next_match(self->cursor, &match)) {
        if (!query_satisfies_predicates(self, match, (Tree *)node->tree, predicate)) {
            continue;
        }

        PyObject *captures_for_match = PyDict_New();
        for (uint16_t i = 0; i < match.capture_count; ++i) {
            TSQueryCapture capture = match.captures[i];
            PyObject *capture_name = PyList_GetItem(self->capture_names, capture.index);
            PyObject *capture_node = node_new_internal(state, capture.node, node->tree);
            PyObject *default_list = PyList_New(0);
            PyObject *capture_list =
                PyDict_SetDefault(captures_for_match, capture_name, default_list);
            Py_DECREF(default_list);
            PyList_Append(capture_list, capture_node);
            Py_XDECREF(capture_node);
        }
        PyObject *pattern_index = PyLong_FromSize_t(match.pattern_index);
        PyObject *tuple_match = PyTuple_Pack(2, pattern_index, captures_for_match);
        Py_DECREF(pattern_index);
        Py_DECREF(captures_for_match);
        PyList_Append(result, tuple_match);
        Py_XDECREF(tuple_match);
    }

    return PyErr_Occurred() == NULL ? result : NULL;
}

PyObject *query_captures(Query *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {"node", "predicate", NULL};
    PyObject *node_obj, *predicate = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O:captures", keywords, state->node_type,
                                     &node_obj, &predicate)) {
        return NULL;
    }
    if (predicate != NULL && !PyCallable_Check(predicate)) {
        PyErr_Format(PyExc_TypeError, "Second argument to captures must be a callable, not %s",
                     predicate->ob_type->tp_name);
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    uint32_t capture_index;
    TSQueryMatch match;
    Node *node = (Node *)node_obj;
    ts_query_cursor_exec(self->cursor, self->query, node->node);
    while (ts_query_cursor_next_capture(self->cursor, &match, &capture_index)) {
        if (!query_satisfies_predicates(self, match, (Tree *)node->tree, predicate)) {
            continue;
        }

        TSQueryCapture capture = match.captures[capture_index];
        PyObject *capture_name = PyList_GetItem(self->capture_names, capture.index);
        PyObject *capture_node = node_new_internal(state, capture.node, node->tree);
        PyObject *default_set = PySet_New(NULL);
        PyObject *capture_set = PyDict_SetDefault(result, capture_name, default_set);
        Py_DECREF(default_set);
        PySet_Add(capture_set, capture_node);
        Py_XDECREF(capture_node);
    }

    Py_ssize_t pos = 0;
    PyObject *key, *value;
    // convert each set to a list so it can be subscriptable
    while (PyDict_Next(result, &pos, &key, &value)) {
        PyObject *list = PySequence_List(value);
        PyDict_SetItem(result, key, list);
        Py_DECREF(list);
    }

    return PyErr_Occurred() == NULL ? result : NULL;
}

PyObject *query_pattern_settings(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:pattern_settings", &pattern_index)) {
        return NULL;
    }
    uint32_t count = ts_query_pattern_count(self->query);
    if (pattern_index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds count %u", pattern_index, count);
        return NULL;
    }
    PyObject *item = PyList_GetItem(self->settings, pattern_index);
    Py_INCREF(item);
    return item;
}

PyObject *query_pattern_assertions(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:pattern_assertions", &pattern_index)) {
        return NULL;
    }
    uint32_t count = ts_query_pattern_count(self->query);
    if (pattern_index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds count %u", pattern_index, count);
        return NULL;
    }
    PyObject *item = PyList_GetItem(self->assertions, pattern_index);
    Py_INCREF(item);
    return item;
}

PyObject *query_set_match_limit(Query *self, PyObject *args) {
    uint32_t match_limit;
    if (!PyArg_ParseTuple(args, "I:set_match_limit", &match_limit)) {
        return NULL;
    }
    if (match_limit == 0) {
        PyErr_SetString(PyExc_ValueError, "Match limit cannot be set to 0");
        return NULL;
    }
    ts_query_cursor_set_match_limit(self->cursor, match_limit);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_set_max_start_depth(Query *self, PyObject *args) {
    uint32_t max_start_depth;
    if (!PyArg_ParseTuple(args, "I:set_max_start_depth", &max_start_depth)) {
        return NULL;
    }
    ts_query_cursor_set_max_start_depth(self->cursor, max_start_depth);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_set_byte_range(Query *self, PyObject *args) {
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "(II):set_byte_range", &start_byte, &end_byte)) {
        return NULL;
    }
    ts_query_cursor_set_byte_range(self->cursor, start_byte, end_byte);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_set_point_range(Query *self, PyObject *args) {
    TSPoint start_point, end_point;
    if (!PyArg_ParseTuple(args, "((II)(II)):set_point_range", &start_point.row, &start_point.column,
                          &end_point.row, &end_point.column)) {
        return NULL;
    }
    ts_query_cursor_set_point_range(self->cursor, start_point, end_point);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_disable_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:disable_pattern", &pattern_index)) {
        return NULL;
    }
    ts_query_disable_pattern(self->query, pattern_index);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_disable_capture(Query *self, PyObject *args) {
    char *capture_name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:disable_capture", &capture_name, &length)) {
        return NULL;
    }
    ts_query_disable_capture(self->query, capture_name, length);
    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject *query_start_byte_for_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index, start_byte;
    if (!PyArg_ParseTuple(args, "I:start_byte_for_pattern", &pattern_index)) {
        return NULL;
    }
    start_byte = ts_query_start_byte_for_pattern(self->query, pattern_index);
    return PyLong_FromSize_t(start_byte);
}

PyObject *query_end_byte_for_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index, end_byte;
    if (!PyArg_ParseTuple(args, "I:end_byte_for_pattern", &pattern_index)) {
        return NULL;
    }
    end_byte = ts_query_end_byte_for_pattern(self->query, pattern_index);
    return PyLong_FromSize_t(end_byte);
}

PyObject *query_is_pattern_rooted(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:is_pattern_rooted", &pattern_index)) {
        return NULL;
    }
    bool result = ts_query_is_pattern_rooted(self->query, pattern_index);
    return PyBool_FromLong(result);
}

PyObject *query_is_pattern_non_local(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:is_pattern_non_local", &pattern_index)) {
        return NULL;
    }
    bool result = ts_query_is_pattern_non_local(self->query, pattern_index);
    return PyBool_FromLong(result);
}

PyObject *query_is_pattern_guaranteed_at_step(Query *self, PyObject *args) {
    uint32_t byte_offset;
    if (!PyArg_ParseTuple(args, "I:is_pattern_guaranteed_at_step", &byte_offset)) {
        return NULL;
    }
    bool result = ts_query_is_pattern_guaranteed_at_step(self->query, byte_offset);
    return PyBool_FromLong(result);
}

PyObject *query_get_pattern_count(Query *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_pattern_count(self->query));
}

PyObject *query_get_capture_count(Query *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_capture_count(self->query));
}

PyObject *query_get_match_limit(Query *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_cursor_match_limit(self->cursor));
}

PyObject *query_get_did_exceed_match_limit(Query *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_cursor_did_exceed_match_limit(self->cursor));
}

PyDoc_STRVAR(query_set_match_limit_doc, "set_match_limit(self, match_limit)\n--\n\n"
                                        "Set the maximum number of in-progress matches." DOC_RAISES
                                        "ValueError\n\n   If set to ``0``.");
PyDoc_STRVAR(query_set_max_start_depth_doc, "set_max_start_depth(self, max_start_depth)\n--\n\n"
                                            "Set the maximum start depth for the query.");
PyDoc_STRVAR(query_set_byte_range_doc,
             "set_byte_range(self, byte_range)\n--\n\n"
             "Set the range of bytes in which the query will be executed.");
PyDoc_STRVAR(query_set_point_range_doc,
             "set_point_range(self, point_range)\n--\n\n"
             "Set the range of points in which the query will be executed.");
PyDoc_STRVAR(query_disable_pattern_doc, "disable_pattern(self, index)\n--\n\n"
                                        "Disable a certain pattern within a query." DOC_IMPORTANT
                                        "Currently, there is no way to undo this.");
PyDoc_STRVAR(query_disable_capture_doc, "disable_capture(self, capture)\n--\n\n"
                                        "Disable a certain capture within a query." DOC_IMPORTANT
                                        "Currently, there is no way to undo this.");
PyDoc_STRVAR(query_matches_doc,
             "matches(self, node, /, predicate=None)\n--\n\n"
             "Get a list of *matches* within the given node." DOC_RETURNS
             "A list of tuples where the first element is the pattern index and "
             "the second element is a dictionary that maps capture names to nodes.");
PyDoc_STRVAR(query_captures_doc,
             "captures(self, node, /, predicate=None)\n--\n\n"
             "Get a list of *captures* within the given node.\n\n" DOC_RETURNS
             "A list of tuples where the first element is the name of the capture and "
             "the second element is the captured node." DOC_HINT "This method returns "
             "all of the captures while :meth:`matches` only returns the last match.");
PyDoc_STRVAR(query_pattern_settings_doc,
             "pattern_settings(self, index)\n--\n\n"
             "Get the property settings for the given pattern index.\n\n"
             "Properties are set using the ``#set!`` predicate." DOC_RETURNS
             "A dictionary of properties with optional values.");
PyDoc_STRVAR(query_pattern_assertions_doc,
             "pattern_assertions(self, index)\n--\n\n"
             "Get the property assertions for the given pattern index.\n\n"
             "Assertions are performed using the ``#is?`` and ``#is-not?`` predicates." DOC_RETURNS
             "A dictionary of assertions, where the first item is the optional property value "
             "and the second item indicates whether the assertion was positive or negative.");
PyDoc_STRVAR(query_start_byte_for_pattern_doc,
             "start_byte_for_pattern(self, index)\n--\n\n"
             "Get the byte offset where the given pattern starts in the query's source.");
PyDoc_STRVAR(query_end_byte_for_pattern_doc,
             "end_byte_for_pattern(self, index)\n--\n\n"
             "Get the byte offset where the given pattern ends in the query's source.");
PyDoc_STRVAR(query_is_pattern_rooted_doc,
             "is_pattern_rooted(self, index)\n--\n\n"
             "Check if the pattern with the given index has a single root node.");
PyDoc_STRVAR(query_is_pattern_non_local_doc,
             "is_pattern_non_local(self, index)\n--\n\n"
             "Check if the pattern with the given index is \"non-local\"." DOC_NOTE
             "A non-local pattern has multiple root nodes and can match within "
             "a repeating sequence of nodes, as specified by the grammar. "
             "Non-local patterns disable certain optimizations that would otherwise "
             "be possible when executing a query on a specific range of a syntax tree.");
PyDoc_STRVAR(query_is_pattern_guaranteed_at_step_doc,
             "is_pattern_guaranteed_at_step(self, index)\n--\n\n"
             "Check if a pattern is guaranteed to match once a given byte offset is reached.");

static PyMethodDef query_methods[] = {
    {
        .ml_name = "set_match_limit",
        .ml_meth = (PyCFunction)query_set_match_limit,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_set_match_limit_doc,
    },
    {
        .ml_name = "set_max_start_depth",
        .ml_meth = (PyCFunction)query_set_max_start_depth,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_set_max_start_depth_doc,
    },
    {
        .ml_name = "set_byte_range",
        .ml_meth = (PyCFunction)query_set_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_set_byte_range_doc,
    },
    {
        .ml_name = "set_point_range",
        .ml_meth = (PyCFunction)query_set_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_set_point_range_doc,
    },
    {
        .ml_name = "disable_pattern",
        .ml_meth = (PyCFunction)query_disable_pattern,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_disable_pattern_doc,
    },
    {
        .ml_name = "disable_capture",
        .ml_meth = (PyCFunction)query_disable_capture,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_disable_capture_doc,
    },
    {
        .ml_name = "matches",
        .ml_meth = (PyCFunction)query_matches,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = query_matches_doc,
    },
    {
        .ml_name = "captures",
        .ml_meth = (PyCFunction)query_captures,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = query_captures_doc,
    },
    {
        .ml_name = "pattern_settings",
        .ml_meth = (PyCFunction)query_pattern_settings,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_pattern_settings_doc,
    },
    {
        .ml_name = "pattern_assertions",
        .ml_meth = (PyCFunction)query_pattern_assertions,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_pattern_assertions_doc,
    },
    {
        .ml_name = "start_byte_for_pattern",
        .ml_meth = (PyCFunction)query_start_byte_for_pattern,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_start_byte_for_pattern_doc,
    },
    {
        .ml_name = "end_byte_for_pattern",
        .ml_meth = (PyCFunction)query_end_byte_for_pattern,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_end_byte_for_pattern_doc,
    },
    {
        .ml_name = "is_pattern_rooted",
        .ml_meth = (PyCFunction)query_is_pattern_rooted,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_is_pattern_rooted_doc,
    },
    {
        .ml_name = "is_pattern_non_local",
        .ml_meth = (PyCFunction)query_is_pattern_non_local,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_is_pattern_non_local_doc,
    },
    {
        .ml_name = "is_pattern_rooted",
        .ml_meth = (PyCFunction)query_is_pattern_rooted,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_is_pattern_rooted_doc,
    },
    {
        .ml_name = "is_pattern_guaranteed_at_step",
        .ml_meth = (PyCFunction)query_is_pattern_guaranteed_at_step,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_is_pattern_guaranteed_at_step_doc,
    },
    {NULL},
};

static PyGetSetDef query_accessors[] = {
    {"pattern_count", (getter)query_get_pattern_count, NULL,
     PyDoc_STR("The number of patterns in the query."), NULL},
    {"capture_count", (getter)query_get_capture_count, NULL,
     PyDoc_STR("The number of captures in the query."), NULL},
    {"match_limit", (getter)query_get_match_limit, NULL,
     PyDoc_STR("The maximum number of in-progress matches."), NULL},
    {"did_exceed_match_limit", (getter)query_get_did_exceed_match_limit, NULL,
     PyDoc_STR("Check if the query exceeded its maximum number of "
               "in-progress matches during its last execution."),
     NULL},
    {NULL}};

static PyType_Slot query_type_slots[] = {
    {Py_tp_doc, PyDoc_STR("A set of patterns that match nodes in a syntax tree." DOC_RAISES
                          "QueryError\n\n   If any error occurred while creating the query.")},
    {Py_tp_new, query_new},
    {Py_tp_dealloc, query_dealloc},
    {Py_tp_methods, query_methods},
    {Py_tp_getset, query_accessors},
    {0, NULL},
};

PyType_Spec query_type_spec = {
    .name = "tree_sitter.Query",
    .basicsize = sizeof(Query),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = query_type_slots,
};
