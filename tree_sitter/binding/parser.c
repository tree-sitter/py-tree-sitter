#include "parser.h"
#include "point.h"
#include "tree.h"

PyObject *parser_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    Parser *self = (Parser *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->parser = ts_parser_new();
    }
    return (PyObject *)self;
}

void parser_dealloc(Parser *self) {
    ts_parser_delete(self->parser);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static const char *parser_read_wrapper(void *payload, uint32_t byte_offset, TSPoint position,
                                       uint32_t *bytes_read) {
    ReadWrapperPayload *wrapper_payload = payload;
    PyObject *read_cb = wrapper_payload->read_cb;

    // We assume that the parser only needs the return value until the next time
    // this function is called or when ts_parser_parse() returns. We store the
    // return value from the callable in wrapper_payload->previous_return_value so
    // that its reference count will be decremented either during the next call to
    // this wrapper or after ts_parser_parse() has returned.
    Py_XDECREF(wrapper_payload->previous_return_value);
    wrapper_payload->previous_return_value = NULL;

    // Form arguments to callable.
    PyObject *byte_offset_obj = PyLong_FromSize_t((size_t)byte_offset);
    PyObject *position_obj = point_new(position);
    if (!position_obj || !byte_offset_obj) {
        *bytes_read = 0;
        return NULL;
    }

    PyObject *args = PyTuple_Pack(2, byte_offset_obj, position_obj);
    Py_XDECREF(byte_offset_obj);
    Py_XDECREF(position_obj);

    // Call callable.
    PyObject *rv = PyObject_Call(read_cb, args, NULL);
    Py_XDECREF(args);

    // If error or None returned, we've done parsing.
    if (!rv || (rv == Py_None)) {
        Py_XDECREF(rv);
        *bytes_read = 0;
        return NULL;
    }

    // If something other than None is returned, it must be a bytes object.
    if (!PyBytes_Check(rv)) {
        Py_XDECREF(rv);
        PyErr_SetString(PyExc_TypeError, "Read callable must return None or byte buffer type");
        *bytes_read = 0;
        return NULL;
    }

    // Store return value in payload so its reference count can be decremented and
    // return string representation of bytes.
    wrapper_payload->previous_return_value = rv;
    *bytes_read = PyBytes_Size(rv);
    return PyBytes_AsString(rv);
}

PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *source_or_callback = NULL;
    PyObject *old_tree_arg = NULL;
    int keep_text = 1;
    static char *keywords[] = {"", "old_tree", "keep_text", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Op:parse", keywords, &source_or_callback,
                                     &old_tree_arg, &keep_text)) {
        return NULL;
    }

    const TSTree *old_tree = NULL;
    if (old_tree_arg) {
        if (!PyObject_IsInstance(old_tree_arg, (PyObject *)state->tree_type)) {
            PyErr_SetString(PyExc_TypeError, "Second argument to parse must be a Tree");
            return NULL;
        }
        old_tree = ((Tree *)old_tree_arg)->tree;
    }

    TSTree *new_tree = NULL;
    Py_buffer source_view;
    if (!PyObject_GetBuffer(source_or_callback, &source_view, PyBUF_SIMPLE)) {
        // parse a buffer
        const char *source_bytes = (const char *)source_view.buf;
        size_t length = source_view.len;
        new_tree = ts_parser_parse_string(self->parser, old_tree, source_bytes, length);
        PyBuffer_Release(&source_view);
    } else if (PyCallable_Check(source_or_callback)) {
        PyErr_Clear(); // clear the GetBuffer error
        // parse a callable
        ReadWrapperPayload payload = {
            .read_cb = source_or_callback,
            .previous_return_value = NULL,
        };
        TSInput input = {
            .payload = &payload,
            .read = parser_read_wrapper,
            .encoding = TSInputEncodingUTF8,
        };
        new_tree = ts_parser_parse(self->parser, old_tree, input);
        Py_XDECREF(payload.previous_return_value);

        // don't allow tree_new_internal to keep the source text
        source_or_callback = Py_None;
        keep_text = 0;
    } else {
        PyErr_SetString(PyExc_TypeError, "First argument byte buffer type or callable");
        return NULL;
    }

    if (!new_tree) {
        PyErr_SetString(PyExc_ValueError, "Parsing failed");
        return NULL;
    }

    return tree_new_internal(state, new_tree, source_or_callback, keep_text);
}

PyObject *parser_reset(Parser *self, void *payload) {
    ts_parser_reset(self->parser);
    Py_RETURN_NONE;
}

PyObject *parser_get_timeout_micros(Parser *self, void *payload) {
    return PyLong_FromUnsignedLong(ts_parser_timeout_micros(self->parser));
}

PyObject *parser_set_timeout_micros(Parser *self, PyObject *arg) {
    long timeout;
    if (!PyArg_Parse(arg, "l", &timeout)) {
        return NULL;
    }

    if (timeout < 0) {
        PyErr_SetString(PyExc_ValueError, "Timeout must be a positive integer");
        return NULL;
    }

    ts_parser_set_timeout_micros(self->parser, timeout);
    Py_RETURN_NONE;
}

PyObject *parser_set_included_ranges(Parser *self, PyObject *arg) {
    ModuleState *state = PyType_GetModuleState(Py_TYPE(self));
    PyObject *ranges = NULL;
    if (!PyArg_Parse(arg, "O", &ranges)) {
        return NULL;
    }

    if (!PyList_Check(ranges)) {
        PyErr_SetString(PyExc_TypeError, "Included ranges must be a list");
        return NULL;
    }

    uint32_t length = PyList_Size(ranges);
    TSRange *c_ranges = malloc(sizeof(TSRange) * length);
    if (!c_ranges) {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    for (unsigned i = 0; i < length; i++) {
        PyObject *range = PyList_GetItem(ranges, i);
        if (!PyObject_IsInstance(range, (PyObject *)state->range_type)) {
            PyErr_SetString(PyExc_TypeError, "Included range must be a Range");
            free(c_ranges);
            return NULL;
        }
        c_ranges[i] = ((Range *)range)->range;
    }

    bool res = ts_parser_set_included_ranges(self->parser, c_ranges, length);
    if (!res) {
        PyErr_SetString(PyExc_ValueError,
                        "Included ranges must not overlap or end before it starts");
        free(c_ranges);
        return NULL;
    }

    free(c_ranges);
    Py_RETURN_NONE;
}

PyObject *parser_set_language(Parser *self, PyObject *arg) {
    PyObject *language_id = PyObject_GetAttrString(arg, "language_id");
    if (!language_id) {
        PyErr_SetString(PyExc_TypeError, "Argument to set_language must be a Language");
        return NULL;
    }

    if (!PyLong_Check(language_id)) {
        PyErr_SetString(PyExc_TypeError, "Language ID must be an integer");
        return NULL;
    }

    TSLanguage *language = (TSLanguage *)PyLong_AsVoidPtr(language_id);
    Py_XDECREF(language_id);
    if (!language) {
        PyErr_SetString(PyExc_ValueError, "Language ID must not be null");
        return NULL;
    }

    unsigned version = ts_language_version(language);
    if (version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
        TREE_SITTER_LANGUAGE_VERSION < version) {
        return PyErr_Format(
            PyExc_ValueError, "Incompatible Language version %u. Must be between %u and %u",
            version, TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION, TREE_SITTER_LANGUAGE_VERSION);
    }

    ts_parser_set_language(self->parser, language);
    Py_RETURN_NONE;
}

static PyGetSetDef Py_UNUSED(parser_accessors[]) = {
    {"timeout_micros", (getter)parser_get_timeout_micros, NULL,
     "The timeout for parsing, in microseconds.", NULL},
    {NULL},
};

static PyMethodDef parser_methods[] = {
    {
        .ml_name = "parse",
        .ml_meth = (PyCFunction)parser_parse,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = "parse(bytes, old_tree=None, keep_text=True)\n--\n\n\
               Parse source code, creating a syntax tree.",
    },
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)parser_reset,
        .ml_flags = METH_NOARGS,
        .ml_doc = "reset()\n--\n\n\
			   Instruct the parser to start the next parse from the beginning.",
    },
    {
        .ml_name = "set_timeout_micros",
        .ml_meth = (PyCFunction)parser_set_timeout_micros,
        .ml_flags = METH_O,
        .ml_doc = "set_timeout_micros(timeout_micros)\n--\n\n\
			  Set the maximum duration in microseconds that parsing should be allowed to\
              take before halting.",
    },
    {
        .ml_name = "set_included_ranges",
        .ml_meth = (PyCFunction)parser_set_included_ranges,
        .ml_flags = METH_O,
        .ml_doc = "set_included_ranges(ranges)\n--\n\n\
			   Set the ranges of text that the parser should include when parsing.",
    },
    {
        .ml_name = "set_language",
        .ml_meth = (PyCFunction)parser_set_language,
        .ml_flags = METH_O,
        .ml_doc = "set_language(language)\n--\n\n\
               Set the parser language.",
    },
    {NULL},
};

static PyType_Slot parser_type_slots[] = {
    {Py_tp_doc, "A parser"},
    {Py_tp_new, parser_new},
    {Py_tp_dealloc, parser_dealloc},
    {Py_tp_methods, parser_methods},
    {0, NULL},
};

PyType_Spec parser_type_spec = {
    .name = "tree_sitter.Parser",
    .basicsize = sizeof(Parser),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = parser_type_slots,
};
