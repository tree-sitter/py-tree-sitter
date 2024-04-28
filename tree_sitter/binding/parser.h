#pragma once

#include "types.h"

typedef struct {
    PyObject *read_cb;
    PyObject *previous_return_value;
    ModuleState *state;
} ReadWrapperPayload;

PyObject *parser_new(PyTypeObject *cls, PyObject *args, PyObject *kwds);

int parser_init(Parser *self, PyObject *args, PyObject *kwargs);

void parser_dealloc(Parser *self);

PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs);

PyObject *parser_reset(Parser *self, void *payload);

PyObject *parser_get_timeout_micros(Parser *self, void *payload);

PyObject *parser_set_timeout_micros_old(Parser *self, PyObject *arg);

int parser_set_timeout_micros(Parser *self, PyObject *arg, void *payload);

PyObject *parser_get_included_ranges(Parser *self, void *payload);

PyObject *parser_set_included_ranges_old(Parser *self, PyObject *arg);

int parser_set_included_ranges(Parser *self, PyObject *arg, void *payload);

PyObject *parser_get_language(Parser *self, void *payload);

PyObject *parser_set_language_old(Parser *self, PyObject *arg);

int parser_set_language(Parser *self, PyObject *arg, void *payload);
