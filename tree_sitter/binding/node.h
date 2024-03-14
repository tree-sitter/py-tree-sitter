#pragma once

#include "types.h"

PyObject *node_new_internal(ModuleState *state, TSNode node, PyObject *tree);

void node_dealloc(Node *self);

PyObject *node_repr(Node *self);

PyObject *node_compare(Node *self, Node *other, int op);

PyObject *node_sexp(Node *self, PyObject *args);

PyObject *node_walk(Node *self, PyObject *args);

PyObject *node_edit(Node *self, PyObject *args, PyObject *kwargs);

PyObject *node_child(Node *self, PyObject *args);

PyObject *node_named_child(Node *self, PyObject *args);

PyObject *node_child_by_field_id(Node *self, PyObject *args);

PyObject *node_child_by_field_name(Node *self, PyObject *args);

PyObject *node_children_by_field_id_internal(Node *self, TSFieldId field_id);

PyObject *node_children_by_field_id(Node *self, PyObject *args);

PyObject *node_children_by_field_name(Node *self, PyObject *args);

PyObject *node_field_name_for_child(Node *self, PyObject *args);

PyObject *node_descendant_for_byte_range(Node *self, PyObject *args);

PyObject *node_named_descendant_for_byte_range(Node *self, PyObject *args);

PyObject *node_descendant_for_point_range(Node *self, PyObject *args);

PyObject *node_named_descendant_for_point_range(Node *self, PyObject *args);

PyObject *node_get_id(Node *self, void *payload);

PyObject *node_get_kind_id(Node *self, void *payload);

PyObject *node_get_grammar_id(Node *self, void *payload);

PyObject *node_get_type(Node *self, void *payload);

PyObject *node_get_grammar_name(Node *self, void *payload);

PyObject *node_get_is_named(Node *self, void *payload);

PyObject *node_get_is_extra(Node *self, void *payload);

PyObject *node_get_has_changes(Node *self, void *payload);

PyObject *node_get_has_error(Node *self, void *payload);

PyObject *node_get_is_error(Node *self, void *payload);

PyObject *node_get_parse_state(Node *self, void *payload);

PyObject *node_get_next_parse_state(Node *self, void *payload);

PyObject *node_get_is_missing(Node *self, void *payload);

PyObject *node_get_start_byte(Node *self, void *payload);

PyObject *node_get_end_byte(Node *self, void *payload);

PyObject *node_get_byte_range(Node *self, void *payload);

PyObject *node_get_range(Node *self, void *payload);

PyObject *node_get_start_point(Node *self, void *payload);

PyObject *node_get_end_point(Node *self, void *payload);

PyObject *node_get_children(Node *self, void *payload);

PyObject *node_get_named_children(Node *self, void *payload);

PyObject *node_get_child_count(Node *self, void *payload);

PyObject *node_get_named_child_count(Node *self, void *payload);

PyObject *node_get_parent(Node *self, void *payload);

PyObject *node_get_next_sibling(Node *self, void *payload);

PyObject *node_get_prev_sibling(Node *self, void *payload);

PyObject *node_get_next_named_sibling(Node *self, void *payload);

PyObject *node_get_prev_named_sibling(Node *self, void *payload);

PyObject *node_get_descendant_count(Node *self, void *payload);

PyObject *node_get_text(Node *self, void *payload);

Py_hash_t node_hash(Node *self);
