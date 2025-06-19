#include "types.h"

#include <string.h>

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

bool query_satisfies_predicates(Query *query, TSQueryMatch match, Tree *tree, PyObject *callable);

#define QUERY_ERROR(...) PyErr_Format(state->query_error, __VA_ARGS__)

#define CHECK_INDEX(query, index)                                                                  \
    do {                                                                                           \
        uint32_t count = ts_query_pattern_count(query);                                            \
        if ((index) >= count) {                                                                    \
            PyErr_Format(PyExc_IndexError, "Index %u exceeds count %u", index, count);             \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

static inline bool is_valid_identifier_char(char ch) { return Py_ISALNUM(ch) || ch == '_'; }

static inline bool is_valid_predicate_char(char ch) {
    return Py_ISALNUM(ch) || ch == '-' || ch == '_' || ch == '?' || ch == '.' || ch == '!';
}

void query_dealloc(Query *self) {
    if (self->query) {
        ts_query_delete(self->query);
    }
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

    uint32_t length, pattern_count = ts_query_pattern_count(query->query);
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
                predicate->capture_id = (predicate_step + 1)->value_id;
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

PyObject *query_capture_name(Query *self, PyObject *args) {
    uint32_t index, length, count;
    if (!PyArg_ParseTuple(args, "I:capture_name", &index)) {
        return NULL;
    }
    count = ts_query_capture_count(self->query);
    if (index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds count %u", index, count);
        return NULL;
    }
    const char *capture = ts_query_capture_name_for_id(self->query, index, &length);
    return PyUnicode_FromStringAndSize(capture, length);
}

PyObject *query_capture_quantifier(Query *self, PyObject *args) {
    uint32_t pattern_index, capture_index, count;
    if (!PyArg_ParseTuple(args, "II:capture_quantifier", &pattern_index, &capture_index)) {
        return NULL;
    }
    count = ts_query_pattern_count(self->query);
    if (pattern_index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds pattern count %u", pattern_index, count);
        return NULL;
    }
    count = ts_query_capture_count(self->query);
    if (capture_index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds capture count %u", capture_index, count);
        return NULL;
    }
    TSQuantifier quantifier =
        ts_query_capture_quantifier_for_id(self->query, pattern_index, capture_index);
    switch (quantifier) {
        case TSQuantifierOne:
            return PyUnicode_FromString("");
        case TSQuantifierZeroOrOne:
            return PyUnicode_FromString("?");
        case TSQuantifierZeroOrMore:
            return PyUnicode_FromString("*");
        case TSQuantifierOneOrMore:
            return PyUnicode_FromString("+");
        default:
            PyErr_SetString(PyExc_SystemError, "Unexpected capture quantifier of 0");
            return NULL;
    }
}

PyObject *query_string_value(Query *self, PyObject *args) {
    uint32_t index, length, count;
    if (!PyArg_ParseTuple(args, "I:string_value", &index)) {
        return NULL;
    }
    count = ts_query_capture_count(self->query);
    if (index >= count) {
        PyErr_Format(PyExc_IndexError, "Index %u exceeds count %u", index, count);
        return NULL;
    }
    const char *value = ts_query_string_value_for_id(self->query, index, &length);
    return PyUnicode_FromStringAndSize(value, length);
}

PyObject *query_pattern_settings(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:pattern_settings", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
    PyObject *item = PyList_GetItem(self->settings, pattern_index);
    return Py_NewRef(item);
}

PyObject *query_pattern_assertions(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:pattern_assertions", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
    PyObject *item = PyList_GetItem(self->assertions, pattern_index);
    return Py_NewRef(item);
}

PyObject *query_disable_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:disable_pattern", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
    ts_query_disable_pattern(self->query, pattern_index);
    return Py_NewRef(self);
}

PyObject *query_disable_capture(Query *self, PyObject *args) {
    char *capture_name;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "s#:disable_capture", &capture_name, &length)) {
        return NULL;
    }
    ts_query_disable_capture(self->query, capture_name, length);
    return Py_NewRef(self);
}

PyObject *query_start_byte_for_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index, start_byte;
    if (!PyArg_ParseTuple(args, "I:start_byte_for_pattern", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
    start_byte = ts_query_start_byte_for_pattern(self->query, pattern_index);
    return PyLong_FromSize_t(start_byte);
}

PyObject *query_end_byte_for_pattern(Query *self, PyObject *args) {
    uint32_t pattern_index, end_byte;
    if (!PyArg_ParseTuple(args, "I:end_byte_for_pattern", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
    end_byte = ts_query_end_byte_for_pattern(self->query, pattern_index);
    return PyLong_FromSize_t(end_byte);
}

PyObject *query_is_pattern_rooted(Query *self, PyObject *args) {
    uint32_t pattern_index;
    if (!PyArg_ParseTuple(args, "I:is_pattern_rooted", &pattern_index)) {
        return NULL;
    }
    CHECK_INDEX(self->query, pattern_index);
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

PyObject *query_get_string_count(Query *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_string_count(self->query));
}

PyDoc_STRVAR(query_disable_pattern_doc, "disable_pattern(self, index)\n--\n\n"
                                        "Disable a certain pattern within a query." DOC_IMPORTANT
                                        "Currently, there is no way to undo this.");
PyDoc_STRVAR(query_disable_capture_doc, "disable_capture(self, name)\n--\n\n"
                                        "Disable a certain capture within a query." DOC_IMPORTANT
                                        "Currently, there is no way to undo this.");
PyDoc_STRVAR(query_capture_name_doc, "capture_name(self, index)\n--\n\n"
                                     "Get the name of the capture at the given index.");
PyDoc_STRVAR(query_string_value_doc, "string_value(self, index)\n--\n\n"
                                     "Get the string literal at the given index.");
PyDoc_STRVAR(query_capture_quantifier_doc,
             "capture_quantifier(self, pattern_index, capture_index)\n--\n\n"
             "Get the quantifier of the capture at the given indexes.");
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
        .ml_name = "capture_name",
        .ml_meth = (PyCFunction)query_capture_name,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_capture_name_doc,
    },
    {
        .ml_name = "capture_quantifier",
        .ml_meth = (PyCFunction)query_capture_quantifier,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_capture_quantifier_doc,
    },
    {
        .ml_name = "string_value",
        .ml_meth = (PyCFunction)query_string_value,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_string_value_doc,
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
    {"string_count", (getter)query_get_string_count, NULL,
     PyDoc_STR("The number of string literals in the query."), NULL},
    {NULL},
};

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
