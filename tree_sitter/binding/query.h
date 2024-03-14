#pragma once

#include "types.h"

PyObject *query_new_internal(ModuleState *state, TSLanguage *language, char *source, int length);

void query_dealloc(Query *self);

PyObject *query_matches(Query *self, PyObject *args, PyObject *kwargs);

PyObject *query_captures(Query *self, PyObject *args, PyObject *kwargs);
