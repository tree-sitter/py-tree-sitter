#pragma once

#include "types.h"

void tree_cursor_dealloc(TreeCursor *self);

PyObject *tree_cursor_get_node(TreeCursor *self, void *payload);

PyObject *tree_cursor_get_field_id(TreeCursor *self, void *payload);

PyObject *tree_cursor_get_field_name(TreeCursor *self, void *payload);

PyObject *tree_cursor_get_depth(TreeCursor *self, void *payload);

PyObject *tree_cursor_get_descendant_index(TreeCursor *self, void *payload);

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

PyObject *tree_cursor_copy(PyObject *self, PyObject *args);
