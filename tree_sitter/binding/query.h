#pragma once

#include "types.h"

PyObject *query_new(PyTypeObject *cls, PyObject *args, PyObject *kwargs);

void query_dealloc(Query *self);

PyObject *query_matches(Query *self, PyObject *args, PyObject *kwargs);

PyObject *query_captures(Query *self, PyObject *args, PyObject *kwargs);
