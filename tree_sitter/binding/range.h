#pragma once

#include "types.h"

int range_init(Range *self, PyObject *args, PyObject *kwargs);

void range_dealloc(Range *self);

PyObject *range_repr(Range *self);

Py_hash_t range_hash(Range *self);

PyObject *range_compare(Range *self, PyObject *other, int op);

PyObject *range_get_start_point(Range *self, void *payload);

PyObject *range_get_end_point(Range *self, void *payload);

PyObject *range_get_start_byte(Range *self, void *payload);

PyObject *range_get_end_byte(Range *self, void *payload);
