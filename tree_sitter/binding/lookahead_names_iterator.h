#pragma once

#include "types.h"

PyObject *lookahead_names_iterator_repr(LookaheadNamesIterator *self);

void lookahead_names_iterator_dealloc(LookaheadNamesIterator *self);

PyObject *lookahead_names_iterator_iter(LookaheadNamesIterator *self);

PyObject *lookahead_names_iterator_next(LookaheadNamesIterator *self);
