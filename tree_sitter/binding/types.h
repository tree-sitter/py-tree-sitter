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
    PyObject *language;
} Tree;

typedef struct {
    PyObject_HEAD
    TSLanguage *language;
    uint32_t abi_version;
    const char *name;
} Language;

typedef struct {
    PyObject_HEAD
    TSParser *parser;
    PyObject *language;
    PyObject *logger;
} Parser;

typedef struct {
    PyObject_HEAD
    TSTreeCursor cursor;
    PyObject *node;
    PyObject *tree;
} TreeCursor;

typedef struct {
    PyObject_HEAD
    uint32_t capture1_id;
    uint32_t capture2_id;
    bool is_positive;
    bool is_any;
} QueryPredicateEqCapture;

typedef struct {
    PyObject_HEAD
    uint32_t capture_id;
    PyObject *string_value;
    bool is_positive;
    bool is_any;
} QueryPredicateEqString;

typedef struct {
    PyObject_HEAD
    uint32_t capture_id;
    PyObject *pattern;
    bool is_positive;
    bool is_any;
} QueryPredicateMatch;

typedef struct {
    PyObject_HEAD
    uint32_t capture_id;
    PyObject *values;
    bool is_positive;
} QueryPredicateAnyOf;

typedef struct {
    PyObject_HEAD
    PyObject *predicate;
    PyObject *arguments;
	uint32_t pattern_index;
} QueryPredicateGeneric;

typedef struct {
    PyObject_HEAD
    TSQuery *query;
    PyObject *predicates;
    PyObject *settings;
    PyObject *assertions;
} Query;

typedef struct {
    PyObject_HEAD
    TSQueryCursor *cursor;
    PyObject *query;
} QueryCursor;

typedef struct {
    PyObject_HEAD
    TSRange range;
} Range;

typedef struct {
    PyObject_HEAD
    TSLookaheadIterator *lookahead_iterator;
    PyObject *language;
} LookaheadIterator;

typedef struct {
    TSTreeCursor default_cursor;
    PyObject *re_compile;
    PyObject *query_error;
    PyTypeObject *language_type;
    PyTypeObject *log_type_type;
    PyTypeObject *lookahead_iterator_type;
    PyTypeObject *node_type;
    PyTypeObject *parser_type;
    PyTypeObject *point_type;
    PyTypeObject *query_cursor_type;
    PyTypeObject *query_predicate_anyof_type;
    PyTypeObject *query_predicate_eq_capture_type;
    PyTypeObject *query_predicate_eq_string_type;
    PyTypeObject *query_predicate_generic_type;
    PyTypeObject *query_predicate_match_type;
    PyTypeObject *query_type;
    PyTypeObject *range_type;
    PyTypeObject *tree_cursor_type;
    PyTypeObject *tree_type;
} ModuleState;

// Macros

#define GET_MODULE_STATE(obj) ((ModuleState *)PyType_GetModuleState(Py_TYPE(obj)))

#define IS_INSTANCE_OF(obj, type) PyObject_IsInstance((obj), (PyObject *)(type))

#define IS_INSTANCE(obj, type_name) IS_INSTANCE_OF(obj, GET_MODULE_STATE(self)->type_name)

#define POINT_NEW(state, point)                                                                    \
    PyObject_CallFunction((PyObject *)(state)->point_type, "II", (point).row, (point).column)

#define DEPRECATE(msg) PyErr_WarnEx(PyExc_DeprecationWarning, msg, 1)

#define REPLACE(old, new) DEPRECATE(old " is deprecated. Use " new " instead.")

// Docstrings

#define DOC_ATTENTION "\n\nAttention\n---------\n"
#define DOC_CAUTION "\n\nCaution\n-------\n"
#define DOC_EXAMPLES "\n\nExamples\n--------\n"
#define DOC_IMPORTANT "\n\nImportant\n---------\n"
#define DOC_NOTE "\n\nNote\n----\n"
#define DOC_PARAMETERS "\n\nParameters\n----------\n"
#define DOC_RAISES "\n\nRaises\n------\n"
#define DOC_RETURNS "\n\nReturns\n-------\n"
#define DOC_SEE_ALSO "\n\nSee Also\n--------\n"
#define DOC_HINT "\n\nHint\n----\n"
#define DOC_TIP "\n\nTip\n---\n"
