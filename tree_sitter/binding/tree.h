#pragma once

#include "types.h"

void tree_dealloc(Tree *self);

PyObject *tree_get_root_node(Tree *self, void *payload);

PyObject *tree_get_text(Tree *self, void *payload);

PyObject *tree_root_node_with_offset(Tree *self, PyObject *args);

PyObject *tree_walk(Tree *self, PyObject *args);

PyObject *tree_edit(Tree *self, PyObject *args, PyObject *kwargs);

PyObject *tree_changed_ranges(Tree *self, PyObject *args, PyObject *kwargs);

PyObject *tree_get_included_ranges(Tree *self, PyObject *args);
