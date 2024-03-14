#pragma once

#include "types.h"

PyObject *tree_cursor_new_internal(ModuleState *state, TSNode node, PyObject *tree);

void tree_cursor_dealloc(TreeCursor *self);

PyObject *tree_cursor_get_node(TreeCursor *self, void *payload);

PyObject *tree_cursor_current_field_id(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_current_field_name(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_current_depth(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_current_descendant_index(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_first_child(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_last_child(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_parent(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_next_sibling(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_previous_sibling(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_descendant(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_first_child_for_byte(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_goto_first_child_for_point(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_reset(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_reset_to(TreeCursor *self, PyObject *args);

PyObject *tree_cursor_copy(PyObject *self);
