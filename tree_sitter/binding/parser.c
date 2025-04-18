#include "types.h"

#define SET_ATTRIBUTE_ERROR(name)                                                                  \
    (name != NULL && name != Py_None && parser_set_##name(self, name, NULL) < 0)

typedef struct {
    PyObject *read_cb;
    PyObject *previous_retval;
    ModuleState *state;
} ReadWrapperPayload;

typedef struct {
    PyObject *callback;
    PyTypeObject *log_type_type;
} LoggerPayload;

static void free_logger(const TSParser *parser) {
    TSLogger logger = ts_parser_logger(parser);
    if (logger.payload != NULL) {
        PyMem_Free(logger.payload);
    }
}

PyObject *parser_new(PyTypeObject *cls, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
    Parser *self = (Parser *)cls->tp_alloc(cls, 0);
    if (self != NULL) {
        self->parser = ts_parser_new();
        self->language = NULL;
        self->logger = NULL;
    }
    return (PyObject *)self;
}

void parser_dealloc(Parser *self) {
    free_logger(self->parser);
    ts_parser_delete(self->parser);
    Py_XDECREF(self->language);
    Py_XDECREF(self->logger);
    Py_TYPE(self)->tp_free(self);
}

static const char *parser_read_wrapper(void *payload, uint32_t byte_offset, TSPoint position,
                                       uint32_t *bytes_read) {
    ReadWrapperPayload *wrapper_payload = (ReadWrapperPayload *)payload;
    PyObject *read_cb = wrapper_payload->read_cb;

    // We assume that the parser only needs the return value until the next time
    // this function is called or when ts_parser_parse() returns. We store the
    // return value from the callable in wrapper_payload->previous_return_value so
    // that its reference count will be decremented either during the next call to
    // this wrapper or after ts_parser_parse() has returned.
    Py_XDECREF(wrapper_payload->previous_retval);
    wrapper_payload->previous_retval = NULL;

    // Form arguments to callable.
    PyObject *byte_offset_obj = PyLong_FromUnsignedLong(byte_offset);
    PyObject *position_obj = POINT_NEW(wrapper_payload->state, position);
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

    // If error or None returned, we're done parsing.
    if (rv == NULL || rv == Py_None) {
        Py_XDECREF(rv);
        *bytes_read = 0;
        return NULL;
    }

    // If something other than None is returned, it must be a bytes object.
    if (!PyBytes_Check(rv)) {
        Py_XDECREF(rv);
        PyErr_SetString(PyExc_TypeError, "read callable must return a bytestring");
        *bytes_read = 0;
        return NULL;
    }

    // Store return value in payload so its reference count can be decremented and
    // return string representation of bytes.
    wrapper_payload->previous_retval = rv;
    *bytes_read = (uint32_t)PyBytes_Size(rv);
    return PyBytes_AsString(rv);
}

static bool parser_progress_callback(TSParseState *state) {
    PyObject *result = PyObject_CallFunction((PyObject *)state->payload, "Ip",
                                             state->current_byte_offset, state->has_error);
    return PyObject_IsTrue(result);
}

PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *source_or_callback;
    PyObject *old_tree_obj = NULL;
    PyObject *encoding_obj = NULL;
    PyObject *progress_callback_obj = NULL;
    bool keep_text = true;
    char *keywords[] = {"", "old_tree", "encoding", "progress_callback", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O!OO:parse", keywords, &source_or_callback,
                                     state->tree_type, &old_tree_obj, &encoding_obj,
                                     &progress_callback_obj)) {
        return NULL;
    }

    const TSTree *old_tree = old_tree_obj ? ((Tree *)old_tree_obj)->tree : NULL;
    TSInputEncoding input_encoding = TSInputEncodingUTF8;
    if (encoding_obj != NULL) {
        if (!PyUnicode_CheckExact(encoding_obj)) {
            PyErr_Format(PyExc_TypeError, "encoding must be str, not %s",
                         encoding_obj->ob_type->tp_name);
            return NULL;
        } else if (PyUnicode_CompareWithASCIIString(encoding_obj, "utf8") == 0) {
            input_encoding = TSInputEncodingUTF8;
        } else if (PyUnicode_CompareWithASCIIString(encoding_obj, "utf16le") == 0) {
            input_encoding = TSInputEncodingUTF16LE;
        } else if (PyUnicode_CompareWithASCIIString(encoding_obj, "utf16be") == 0) {
            input_encoding = TSInputEncodingUTF16BE;
        } else if (PyUnicode_CompareWithASCIIString(encoding_obj, "utf16") == 0) {
            PyObject *byteorder = PySys_GetObject("byteorder");
            bool little_endian = PyUnicode_CompareWithASCIIString(byteorder, "little") == 0;
            input_encoding = little_endian ? TSInputEncodingUTF16LE : TSInputEncodingUTF16BE;
        } else {
            PyErr_Format(PyExc_ValueError,
                         "encoding must be 'utf8', 'utf16', 'utf16le', or 'utf16be', not '%s'",
                         PyUnicode_AsUTF8(encoding_obj));
            return NULL;
        }
    }

    TSTree *new_tree = NULL;
    Py_buffer source_view;
    if (PyObject_GetBuffer(source_or_callback, &source_view, PyBUF_SIMPLE) > -1) {
        if (progress_callback_obj != NULL) {
            const char *warning = "The progress_callback is ignored when parsing a bytestring";
            if (PyErr_WarnEx(PyExc_UserWarning, warning, 1) < 0) {
                return NULL;
            }
        }
        // parse a buffer
        const char *source_bytes = (const char *)source_view.buf;
        uint32_t length = (uint32_t)source_view.len;
        new_tree = ts_parser_parse_string_encoding(self->parser, old_tree, source_bytes, length,
                                                   input_encoding);
        PyBuffer_Release(&source_view);
    } else if (PyCallable_Check(source_or_callback)) {
        // clear the GetBuffer error
        PyErr_Clear();
        // parse a callable
        ReadWrapperPayload payload = {
            .state = state,
            .read_cb = source_or_callback,
            .previous_retval = NULL,
        };
        TSInput input = {
            .payload = &payload,
            .read = parser_read_wrapper,
            .encoding = input_encoding,
            .decode = NULL,
        };
        if (progress_callback_obj == NULL) {
            new_tree = ts_parser_parse(self->parser, old_tree, input);
        } else if (!PyCallable_Check(progress_callback_obj)) {
            PyErr_Format(PyExc_TypeError, "progress_callback must be a callable, not %s",
                         progress_callback_obj->ob_type->tp_name);
            return NULL;
        } else {
            TSParseOptions options = {
                .payload = progress_callback_obj,
                .progress_callback = parser_progress_callback,
            };
            new_tree = ts_parser_parse_with_options(self->parser, old_tree, input, options);
        }
        Py_XDECREF(payload.previous_retval);

    } else {
        PyErr_Format(PyExc_TypeError, "source must be a bytestring or a callable, not %s",
                     source_or_callback->ob_type->tp_name);
        return NULL;
    }

    if (PyErr_Occurred()) {
        return NULL;
    }
    if (!new_tree) {
        PyErr_SetString(PyExc_ValueError, "Parsing failed");
        return NULL;
    }

    Tree *tree = PyObject_New(Tree, state->tree_type);
    if (tree == NULL) {
        return NULL;
    }
    tree->tree = new_tree;
    tree->language = self->language;
    tree->source = keep_text ? source_or_callback : Py_None;
    Py_INCREF(tree->source);
    return PyObject_Init((PyObject *)tree, state->tree_type);
}

PyObject *parser_reset(Parser *self, void *Py_UNUSED(payload)) {
    ts_parser_reset(self->parser);
    Py_RETURN_NONE;
}

PyObject *parser_print_dot_graphs(Parser *self, PyObject *arg) {
    if (arg == Py_None) {
        ts_parser_print_dot_graphs(self->parser, -1);
    } else {
        int fd = PyObject_AsFileDescriptor(arg);
        if (fd < 0) {
            return NULL;
        }
        Py_BEGIN_ALLOW_THREADS
        ts_parser_print_dot_graphs(self->parser, fd);
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

PyObject *parser_get_timeout_micros(Parser *self, void *Py_UNUSED(payload)) {
    if (DEPRECATE("Use the progress_callback in parse()") < 0) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(ts_parser_timeout_micros(self->parser));
}

int parser_set_timeout_micros(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
    if (DEPRECATE("Use the progress_callback in parse()") < 0) {
        return -1;
    }
    if (arg == NULL || arg == Py_None) {
        ts_parser_set_timeout_micros(self->parser, 0);
        return 0;
    }
    if (!PyLong_Check(arg)) {
        PyErr_Format(PyExc_TypeError, "'timeout_micros' must be assigned an int, not %s",
                     arg->ob_type->tp_name);
        return -1;
    }

    ts_parser_set_timeout_micros(self->parser, PyLong_AsSize_t(arg));
    return 0;
}

PyObject *parser_get_included_ranges(Parser *self, void *Py_UNUSED(payload)) {
    uint32_t count;
    const TSRange *ranges = ts_parser_included_ranges(self->parser, &count);
    if (count == 0) {
        return PyList_New(0);
    }

    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *list = PyList_New(count);
    for (uint32_t i = 0; i < count; ++i) {
        Range *range = PyObject_New(Range, state->range_type);
        if (range == NULL) {
            return NULL;
        }
        range->range = ranges[i];
        PyList_SET_ITEM(list, i, PyObject_Init((PyObject *)range, state->range_type));
    }
    return list;
}

int parser_set_included_ranges(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
    if (arg == NULL || arg == Py_None) {
        ts_parser_set_included_ranges(self->parser, NULL, 0);
        return 0;
    }
    if (!PyList_Check(arg)) {
        PyErr_Format(PyExc_TypeError, "'included_ranges' must be assigned a list, not %s",
                     arg->ob_type->tp_name);
        return -1;
    }

    uint32_t length = (uint32_t)PyList_Size(arg);
    TSRange *ranges = PyMem_Calloc(length, sizeof(TSRange));
    if (!ranges) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate memory for ranges of length %u",
                     length);
        return -1;
    }

    ModuleState *state = GET_MODULE_STATE(self);
    for (uint32_t i = 0; i < length; ++i) {
        PyObject *range = PyList_GetItem(arg, i);
        if (!PyObject_IsInstance(range, (PyObject *)state->range_type)) {
            PyErr_Format(PyExc_TypeError, "Item at index %u is not a tree_sitter.Range object", i);
            PyMem_Free(ranges);
            return -1;
        }
        ranges[i] = ((Range *)range)->range;
    }

    if (!ts_parser_set_included_ranges(self->parser, ranges, length)) {
        PyErr_SetString(PyExc_ValueError, "Included ranges cannot overlap");
        PyMem_Free(ranges);
        return -1;
    }

    PyMem_Free(ranges);
    return 0;
}

PyObject *parser_get_language(Parser *self, void *Py_UNUSED(payload)) {
    if (!self->language) {
        Py_RETURN_NONE;
    }
    return Py_NewRef(self->language);
}

PyObject *parser_get_logger(Parser *self, void *Py_UNUSED(payload)) {
    if (!self->logger) {
        Py_RETURN_NONE;
    }
    return Py_NewRef(self->logger);
}

static void log_callback(void *payload, TSLogType log_type, const char *buffer) {
    LoggerPayload *logger_payload = (LoggerPayload *)payload;
    PyObject *log_type_enum =
        PyObject_CallFunction((PyObject *)logger_payload->log_type_type, "i", log_type);
    PyObject_CallFunction(logger_payload->callback, "Os", log_type_enum, buffer);
}

int parser_set_logger(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
    free_logger(self->parser);

    if (arg == NULL || arg == Py_None) {
        Py_XDECREF(self->logger);
        self->logger = NULL;
        TSLogger logger = {NULL, NULL};
        ts_parser_set_logger(self->parser, logger);
        return 0;
    }
    if (!PyCallable_Check(arg)) {
        PyErr_Format(PyExc_TypeError, "logger must be assigned a callable object, not %s",
                     arg->ob_type->tp_name);
        return -1;
    }

    Py_XSETREF(self->logger, Py_NewRef(arg));

    ModuleState *state = GET_MODULE_STATE(self);
    LoggerPayload *payload = PyMem_Malloc(sizeof(LoggerPayload));
    payload->callback = self->logger;
    payload->log_type_type = state->log_type_type;
    TSLogger logger = {payload, log_callback};
    ts_parser_set_logger(self->parser, logger);

    return 0;
}

int parser_set_language(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
    if (arg == NULL || arg == Py_None) {
        self->language = NULL;
        return 0;
    }
    if (!IS_INSTANCE(arg, language_type)) {
        PyErr_Format(PyExc_TypeError,
                     "language must be assigned a tree_sitter.Language object, not %s",
                     arg->ob_type->tp_name);
        return -1;
    }

    Language *language = (Language *)arg;
    if (language->abi_version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
        TREE_SITTER_LANGUAGE_VERSION < language->abi_version) {
        PyErr_Format(PyExc_ValueError,
                     "Incompatible Language version %u. Must be between %u and %u",
                     language->abi_version, TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION,
                     TREE_SITTER_LANGUAGE_VERSION);
        return -1;
    }

    if (!ts_parser_set_language(self->parser, language->language)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set the parser language");
        return -1;
    }

    Py_XSETREF(self->language, Py_NewRef(language));
    return 0;
}

int parser_init(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *language = NULL, *included_ranges = NULL, *timeout_micros = NULL, *logger = NULL;
    char *keywords[] = {"language", "included_ranges", "timeout_micros", "logger", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!$OOO:__init__", keywords,
                                     state->language_type, &language, &included_ranges,
                                     &timeout_micros, &logger)) {
        return -1;
    }

    if (SET_ATTRIBUTE_ERROR(language)) {
        return -1;
    }
    if (SET_ATTRIBUTE_ERROR(included_ranges)) {
        return -1;
    }
    if (SET_ATTRIBUTE_ERROR(timeout_micros)) {
        return -1;
    }
    if (SET_ATTRIBUTE_ERROR(logger)) {
        return -1;
    }
    return 0;
}

PyDoc_STRVAR(
    parser_parse_doc,
    "parse(self, source, /, old_tree=None, encoding=\"utf8\")\n--\n\n"
    "Parse a slice of a bytestring or bytes provided in chunks by a callback.\n\n"
    "The callback function takes a byte offset and position and returns a bytestring starting "
    "at that offset and position. The slices can be of any length. If the given position "
    "is at the end of the text, the callback should return an empty slice." DOC_RETURNS
    "A :class:`Tree` if parsing succeeded or ``None`` if the parser does not have an "
    "assigned language or the timeout expired.");
PyDoc_STRVAR(
    parser_reset_doc,
    "reset(self, /)\n--\n\n"
    "Instruct the parser to start the next parse from the beginning." DOC_NOTE
    "If the parser previously failed because of a timeout, then by default, it will resume where "
    "it left off on the next call to :meth:`parse`.\nIf you don't want to resume, and instead "
    "intend to use this parser to parse some other document, you must call :meth:`reset` first.");
PyDoc_STRVAR(parser_print_dot_graphs_doc,
             "print_dot_graphs(self, /, file)\n--\n\n"
             "Set the file descriptor to which the parser should write debugging "
             "graphs during parsing. The graphs are formatted in the DOT language. "
             "You can turn off this logging by passing ``None``.");

static PyMethodDef parser_methods[] = {
    {
        .ml_name = "parse",
        .ml_meth = (PyCFunction)parser_parse,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc = parser_parse_doc,
    },
    {
        .ml_name = "reset",
        .ml_meth = (PyCFunction)parser_reset,
        .ml_flags = METH_NOARGS,
        .ml_doc = parser_reset_doc,
    },
    {
        .ml_name = "print_dot_graphs",
        .ml_meth = (PyCFunction)parser_print_dot_graphs,
        .ml_flags = METH_O,
        .ml_doc = parser_print_dot_graphs_doc,
    },
    {NULL},
};

static PyGetSetDef parser_accessors[] = {
    {"language", (getter)parser_get_language, (setter)parser_set_language,
     PyDoc_STR("The language that will be used for parsing."), NULL},
    {"included_ranges", (getter)parser_get_included_ranges, (setter)parser_set_included_ranges,
     PyDoc_STR("The ranges of text that the parser will include when parsing."), NULL},
    {"timeout_micros", (getter)parser_get_timeout_micros, (setter)parser_set_timeout_micros,
     PyDoc_STR("The duration in microseconds that parsing is allowed to take."), NULL},
    {"logger", (getter)parser_get_logger, (setter)parser_set_logger,
     PyDoc_STR("The logger that the parser should use during parsing."), NULL},
    {NULL},
};

static PyType_Slot parser_type_slots[] = {
    {Py_tp_doc,
     PyDoc_STR("A class that is used to produce a :class:`Tree` based on some source code.")},
    {Py_tp_new, parser_new},
    {Py_tp_init, parser_init},
    {Py_tp_dealloc, parser_dealloc},
    {Py_tp_methods, parser_methods},
    {Py_tp_getset, parser_accessors},
    {0, NULL},
};

PyType_Spec parser_type_spec = {
    .name = "tree_sitter.Parser",
    .basicsize = sizeof(Parser),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = parser_type_slots,
};
