#pragma once

#include "types.h"

int language_init(Language *self, PyObject *args, PyObject *kwargs);

void language_dealloc(Language *self);

PyObject *language_repr(Language *self);

PyObject *language_compare(Language *self, PyObject *other, int op);

Py_hash_t language_hash(Language *self);

#if HAS_LANGUAGE_NAMES
PyObject *language_get_name(Language *self, void *payload);
#endif

PyObject *language_get_version(Language *self, void *payload);

PyObject *language_get_node_kind_count(Language *self, void *payload);

PyObject *language_get_parse_state_count(Language *self, void *payload);

PyObject *language_get_field_count(Language *self, void *payload);

PyObject *language_node_kind_for_id(Language *self, PyObject *args);

PyObject *language_id_for_node_kind(Language *self, PyObject *args);

PyObject *language_node_kind_is_named(Language *self, PyObject *args);

PyObject *language_node_kind_is_visible(Language *self, PyObject *args);

PyObject *language_field_name_for_id(Language *self, PyObject *args);

PyObject *language_field_id_for_name(Language *self, PyObject *args);

PyObject *language_next_state(Language *self, PyObject *args);

PyObject *language_lookahead_iterator(Language *self, PyObject *args);

PyObject *language_query(Language *self, PyObject *args);
