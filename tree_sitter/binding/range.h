#pragma once

#include "types.h"

PyObject *range_new_internal(ModuleState *state, TSRange range);

PyObject *range_init(Range *self, PyObject *args, PyObject *kwargs);

void range_dealloc(Range *self);

PyObject *range_repr(Range *self);

PyObject *range_compare(Range *self, Range *other, int op);

PyObject *range_get_start_point(Range *self, void *payload);

PyObject *range_get_end_point(Range *self, void *payload);

PyObject *range_get_start_byte(Range *self, void *payload);

PyObject *range_get_end_byte(Range *self, void *payload);
