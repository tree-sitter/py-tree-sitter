#pragma once

#include "types.h"

typedef struct {
    PyObject *read_cb;
    PyObject *previous_return_value;
} ReadWrapperPayload;

PyObject *parser_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

void parser_dealloc(Parser *self);

PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs);

PyObject *parser_reset(Parser *self, void *payload);

PyObject *parser_get_timeout_micros(Parser *self, void *payload);

PyObject *parser_set_timeout_micros(Parser *self, PyObject *arg);

PyObject *parser_set_included_ranges(Parser *self, PyObject *arg);

PyObject *parser_set_language(Parser *self, PyObject *arg);
