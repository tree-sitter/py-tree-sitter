#include "types.h"

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

bool query_satisfies_predicates(Query *query, TSQueryMatch match, Tree *tree, PyObject *callable);

void query_cursor_dealloc(QueryCursor *self) {
    ts_query_cursor_delete(self->cursor);
    Py_XDECREF(self->query);
    Py_TYPE(self)->tp_free(self);
}

PyObject *query_cursor_new(PyTypeObject *cls, PyObject *Py_UNUSED(args),
                           PyObject *Py_UNUSED(kwargs)) {
    QueryCursor *self = (QueryCursor *)cls->tp_alloc(cls, 0);
    if (self != NULL) {
        self->cursor = ts_query_cursor_new();
    }
    return (PyObject *)self;
}

int query_cursor_init(QueryCursor *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *query = NULL;
    uint32_t match_limit = UINT32_MAX;
    char *keywords[] = {"query", "match_limit", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|$I:__init__", keywords, state->query_type,
                                     &query, &match_limit)) {
        return -1;
    }

    self->query = Py_NewRef(query);
    ts_query_cursor_set_match_limit(self->cursor, match_limit);

    return 0;
}

PyObject *query_cursor_set_max_start_depth(QueryCursor *self, PyObject *args) {
    uint32_t max_start_depth;
    if (!PyArg_ParseTuple(args, "I:set_max_start_depth", &max_start_depth)) {
        return NULL;
    }
    ts_query_cursor_set_max_start_depth(self->cursor, max_start_depth);
    return Py_NewRef(self);
}

PyObject *query_cursor_set_byte_range(QueryCursor *self, PyObject *args) {
    uint32_t start_byte, end_byte;
    if (!PyArg_ParseTuple(args, "II:set_byte_range", &start_byte, &end_byte)) {
        return NULL;
    }
    if (!ts_query_cursor_set_byte_range(self->cursor, start_byte, end_byte)) {
        PyErr_SetString(PyExc_ValueError, "Invalid byte range");
        return NULL;
    }
    return Py_NewRef(self);
}

PyObject *query_cursor_set_point_range(QueryCursor *self, PyObject *args) {
    TSPoint start_point, end_point;
    if (!PyArg_ParseTuple(args, "(II)(II):set_point_range", &start_point.row, &start_point.column,
                          &end_point.row, &end_point.column)) {
        return NULL;
    }
    if (!ts_query_cursor_set_point_range(self->cursor, start_point, end_point)) {
        PyErr_SetString(PyExc_ValueError, "Invalid point range");
        return NULL;
    }
    return Py_NewRef(self);
}

static bool query_cursor_progress_callback(TSQueryCursorState *state) {
    PyObject *result =
        PyObject_CallFunction((PyObject *)state->payload, "I", state->current_byte_offset);
    return PyObject_IsTrue(result);
}

PyObject *query_cursor_matches(QueryCursor *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {"node", "predicate", "progress_callback", NULL};
    PyObject *node_obj, *predicate = NULL, *progress_callback_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OO:matches", keywords, state->node_type,
                                     &node_obj, &predicate, &progress_callback_obj)) {
        return NULL;
    }
    if (predicate != NULL && !PyCallable_Check(predicate)) {
        PyErr_Format(PyExc_TypeError, "predicate must be a callable, not %s",
                     predicate->ob_type->tp_name);
        return NULL;
    }
    if (progress_callback_obj != NULL && !PyCallable_Check(progress_callback_obj)) {
        PyErr_Format(PyExc_TypeError, "progress_callback must be a callable, not %s",
                     progress_callback_obj->ob_type->tp_name);
        return NULL;
    }

    PyObject *result = PyList_New(0);
    if (result == NULL) {
        return NULL;
    }

    TSQueryMatch match;
    uint32_t name_length;
    Node *node = (Node *)node_obj;
    Query *query = (Query *)self->query;
    if (progress_callback_obj == NULL) {
        ts_query_cursor_exec(self->cursor, query->query, node->node);
    } else {
        TSQueryCursorOptions options = {
            .payload = progress_callback_obj,
            .progress_callback = query_cursor_progress_callback,
        };
        ts_query_cursor_exec_with_options(self->cursor, query->query, node->node, &options);
    }
    while (ts_query_cursor_next_match(self->cursor, &match)) {
        if (!query_satisfies_predicates(query, match, (Tree *)node->tree, predicate)) {
            continue;
        }

        PyObject *captures_for_match = PyDict_New();
        for (uint16_t i = 0; i < match.capture_count; ++i) {
            TSQueryCapture capture = match.captures[i];
            const char *capture_name =
                ts_query_capture_name_for_id(query->query, capture.index, &name_length);
            PyObject *capture_name_obj = PyUnicode_FromStringAndSize(capture_name, name_length);
            PyObject *capture_node = node_new_internal(state, capture.node, node->tree);
            PyObject *default_list = PyList_New(0);
            PyObject *capture_list =
                PyDict_SetDefault(captures_for_match, capture_name_obj, default_list);
            Py_DECREF(capture_name_obj);
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

PyObject *query_cursor_captures(QueryCursor *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    char *keywords[] = {"node", "predicate", "progress_callback", NULL};
    PyObject *node_obj, *predicate = NULL, *progress_callback_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OO:captures", keywords, state->node_type,
                                     &node_obj, &predicate, &progress_callback_obj)) {
        return NULL;
    }
    if (predicate != NULL && !PyCallable_Check(predicate)) {
        PyErr_Format(PyExc_TypeError, "predicate must be a callable, not %s",
                     predicate->ob_type->tp_name);
        return NULL;
    }
    if (progress_callback_obj != NULL && !PyCallable_Check(progress_callback_obj)) {
        PyErr_Format(PyExc_TypeError, "progress_callback must be a callable, not %s",
                     progress_callback_obj->ob_type->tp_name);
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    uint32_t capture_index;
    TSQueryMatch match;
    uint32_t name_length;
    Node *node = (Node *)node_obj;
    Query *query = (Query *)self->query;
    if (progress_callback_obj == NULL) {
        ts_query_cursor_exec(self->cursor, query->query, node->node);
    } else {
        TSQueryCursorOptions options = {
            .payload = progress_callback_obj,
            .progress_callback = query_cursor_progress_callback,
        };
        ts_query_cursor_exec_with_options(self->cursor, query->query, node->node, &options);
    }
    while (ts_query_cursor_next_capture(self->cursor, &match, &capture_index)) {
        if (!query_satisfies_predicates(query, match, (Tree *)node->tree, predicate)) {
            continue;
        }
        if (PyErr_Occurred()) {
            return NULL;
        }

        TSQueryCapture capture = match.captures[capture_index];
        const char *capture_name =
            ts_query_capture_name_for_id(query->query, capture.index, &name_length);
        PyObject *capture_name_obj = PyUnicode_FromStringAndSize(capture_name, name_length);
        PyObject *capture_node = node_new_internal(state, capture.node, node->tree);
        PyObject *default_set = PySet_New(NULL);
        PyObject *capture_set = PyDict_SetDefault(result, capture_name_obj, default_set);
        Py_DECREF(capture_name_obj);
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

PyObject *query_cursor_get_did_exceed_match_limit(QueryCursor *self, void *Py_UNUSED(payload)) {
    return PyLong_FromSize_t(ts_query_cursor_did_exceed_match_limit(self->cursor));
}

PyObject *query_cursor_get_match_limit(QueryCursor *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_query_cursor_match_limit(self->cursor));
}

int query_cursor_set_match_limit(QueryCursor *self, PyObject *arg, void *Py_UNUSED(payload)) {
    if (arg == NULL || arg == Py_None) {
        ts_query_cursor_set_match_limit(self->cursor, UINT32_MAX);
        return 0;
    }
    if (!PyLong_Check(arg)) {
        PyErr_Format(PyExc_TypeError, "'match_limit' must be assigned an int, not %s",
                     arg->ob_type->tp_name);
        return -1;
    }

    ts_query_cursor_set_match_limit(self->cursor, PyLong_AsSize_t(arg));
    return 0;
}

PyDoc_STRVAR(query_cursor_set_max_start_depth_doc,
             "set_max_start_depth(self, max_start_depth)\n--\n\n"
             "Set the maximum start depth for the query.");
PyDoc_STRVAR(query_cursor_set_byte_range_doc,
             "set_byte_range(self, start, end)\n--\n\n"
             "Set the range of bytes in which the query will be executed." DOC_RAISES
             "ValueError\n\n   If the start byte exceeds the end byte." DOC_NOTE
             "The query cursor will return matches that intersect with the given byte range. "
             "This means that a match may be returned even if some of its captures fall outside "
             "the specified range, as long as at least part of the match overlaps with it.");
PyDoc_STRVAR(query_cursor_set_point_range_doc,
             "set_point_range(self, start, end)\n--\n\n"
             "Set the range of points in which the query will be executed." DOC_RAISES
             "ValueError\n\n   If the start point exceeds the end point." DOC_NOTE
             "The query cursor will return matches that intersect with the given point range. "
             "This means that a match may be returned even if some of its captures fall outside "
             "the specified range, as long as at least part of the match overlaps with it.");
PyDoc_STRVAR(query_cursor_matches_doc,
             "matches(self, node, /, predicate=None, progress_callback=None)\n--\n\n"
             "Get a list of *matches* within the given node." DOC_RETURNS
             "A list of tuples where the first element is the pattern index and "
             "the second element is a dictionary that maps capture names to nodes.");
PyDoc_STRVAR(query_cursor_captures_doc,
             "captures(self, node, /, predicate=None, progress_callback=None)\n--\n\n"
             "Get a list of *captures* within the given node.\n\n" DOC_RETURNS
             "A dict where the keys are the names of the captures and the values are "
             "lists of the captured nodes." DOC_HINT "This method returns all of the "
             "captures while :meth:`matches` only returns the last match.");

static PyMethodDef query_cursor_methods[] = {
    {
        .ml_name = "set_max_start_depth",
        .ml_meth = (PyCFunction)query_cursor_set_max_start_depth,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_cursor_set_max_start_depth_doc,
    },
    {
        .ml_name = "set_byte_range",
        .ml_meth = (PyCFunction)query_cursor_set_byte_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_cursor_set_byte_range_doc,
    },
    {
        .ml_name = "set_point_range",
        .ml_meth = (PyCFunction)query_cursor_set_point_range,
        .ml_flags = METH_VARARGS,
        .ml_doc = query_cursor_set_point_range_doc,
    },
    {
        .ml_name = "matches",
        .ml_meth = (PyCFunction)query_cursor_matches,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = query_cursor_matches_doc,
    },
    {
        .ml_name = "captures",
        .ml_meth = (PyCFunction)query_cursor_captures,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = query_cursor_captures_doc,
    },
    {NULL},
};

static PyGetSetDef query_cursor_accessors[] = {
    {"match_limit", (getter)query_cursor_get_match_limit, (setter)query_cursor_set_match_limit,
     PyDoc_STR("The maximum number of in-progress matches."), NULL},
    {"did_exceed_match_limit", (getter)query_cursor_get_did_exceed_match_limit, NULL,
     PyDoc_STR("Check if the query exceeded its maximum number of "
               "in-progress matches during its last execution."),
     NULL},
    {NULL},
};

static PyType_Slot query_cursor_type_slots[] = {
    {Py_tp_doc,
     PyDoc_STR("A class for executing a :class:`Query` on a syntax :class:`Tree`.")},
    {Py_tp_new, query_cursor_new},
    {Py_tp_init, query_cursor_init},
    {Py_tp_dealloc, query_cursor_dealloc},
    {Py_tp_methods, query_cursor_methods},
    {Py_tp_getset, query_cursor_accessors},
    {0, NULL},
};

PyType_Spec query_cursor_type_spec = {
    .name = "tree_sitter.QueryCursor",
    .basicsize = sizeof(QueryCursor),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = query_cursor_type_slots,
};
