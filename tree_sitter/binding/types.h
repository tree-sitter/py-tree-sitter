#pragma once

#include "tree_sitter/api.h"

#include <Python.h>

// Types

typedef struct {
    PyObject_HEAD
    TSNode node;
    PyObject *children;
    PyObject *tree;
} Node;

typedef struct {
    PyObject_HEAD
    TSTree *tree;
    PyObject *source;
} Tree;

typedef struct {
    PyObject_HEAD
    TSLanguage *language;
    uint32_t version;
    char *name;
} Language;

typedef struct {
    PyObject_HEAD
    TSParser *parser;
} Parser;

typedef struct {
    PyObject_HEAD
    TSTreeCursor cursor;
    PyObject *node;
    PyObject *tree;
} TreeCursor;

typedef struct {
    PyObject_HEAD
    uint32_t capture1_value_id;
    uint32_t capture2_value_id;
    int is_positive;
} CaptureEqCapture;

typedef struct {
    PyObject_HEAD
    uint32_t capture_value_id;
    PyObject *string_value;
    int is_positive;
} CaptureEqString;

typedef struct {
    PyObject_HEAD
    uint32_t capture_value_id;
    PyObject *regex;
    int is_positive;
} CaptureMatchString;

typedef struct {
    PyObject_HEAD
    TSQuery *query;
    PyObject *capture_names;
    PyObject *text_predicates;
} Query;

typedef struct {
    PyObject_HEAD
    TSQueryCapture capture;
} QueryCapture;

typedef struct {
    PyObject_HEAD
    TSQueryMatch match;
    PyObject *captures;
    PyObject *pattern_index;
} QueryMatch;

typedef struct {
    PyObject_HEAD
    TSRange range;
} Range;

typedef struct {
    PyObject_HEAD
    TSLookaheadIterator *lookahead_iterator;
} LookaheadIterator;

typedef LookaheadIterator LookaheadNamesIterator;

typedef struct {
    TSTreeCursor default_cursor;
    TSQueryCursor *query_cursor;

    PyObject *re_compile;
    PyObject *namedtuple;

    PyTypeObject *point_type;
    PyTypeObject *tree_type;
    PyTypeObject *tree_cursor_type;
    PyTypeObject *language_type;
    PyTypeObject *parser_type;
    PyTypeObject *node_type;
    PyTypeObject *query_type;
    PyTypeObject *range_type;
    PyTypeObject *query_capture_type;
    PyTypeObject *query_match_type;
    PyTypeObject *capture_eq_capture_type;
    PyTypeObject *capture_eq_string_type;
    PyTypeObject *capture_match_string_type;
    PyTypeObject *lookahead_iterator_type;
    PyTypeObject *lookahead_names_iterator_type;
} ModuleState;

#define GET_MODULE_STATE(type) ((ModuleState *)PyType_GetModuleState(type))

#define IS_INSTANCE(obj, type) \
    PyObject_IsInstance((obj), (PyObject *)(GET_MODULE_STATE(Py_TYPE(obj))->type))

#define POINT_NEW(state, point) \
    PyObject_CallFunction((PyObject *)(state)->point_type, "II", (point).row, (point).column)
