#pragma once

#include "types.h"

PyObject *lookahead_iterator_new_internal(ModuleState *state,
                                          TSLookaheadIterator *lookahead_iterator);

void lookahead_iterator_dealloc(LookaheadIterator *self);

PyObject *lookahead_iterator_repr(LookaheadIterator *self);

PyObject *lookahead_iterator_get_language(LookaheadIterator *self, void *payload);

PyObject *lookahead_iterator_get_current_symbol(LookaheadIterator *self, void *payload);

PyObject *lookahead_iterator_get_current_symbol_name(LookaheadIterator *self, void *payload);

PyObject *lookahead_iterator_reset(LookaheadIterator *self, PyObject *args);

PyObject *lookahead_iterator_reset_state(LookaheadIterator *self, PyObject *args, PyObject *kwargs);

PyObject *lookahead_iterator_iter(LookaheadIterator *self);

PyObject *lookahead_iterator_next(LookaheadIterator *self);

PyObject *lookahead_iterator_names_iterator(LookaheadIterator *self);
