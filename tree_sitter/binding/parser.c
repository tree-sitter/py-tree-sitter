#include <wasm.h>
#include "types.h"

#define SET_ATTRIBUTE_ERROR(name)                                                                  \
    (name != NULL && name != Py_None && parser_set_##name(self, name, NULL) < 0)

typedef struct {
    PyObject *read_cb;
    PyObject *previous_return_value;
    ModuleState *state;
} ReadWrapperPayload;

PyObject *parser_new(PyTypeObject *cls, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
    Parser *self = (Parser *)cls->tp_alloc(cls, 0);
    if (self != NULL) {
        self->parser = ts_parser_new();
        self->language = NULL;
    }
    return (PyObject *)self;
}

void parser_dealloc(Parser *self) {
    ts_parser_delete(self->parser);
    Py_XDECREF(self->language);
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
    Py_XDECREF(wrapper_payload->previous_return_value);
    wrapper_payload->previous_return_value = NULL;

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
    wrapper_payload->previous_return_value = rv;
    *bytes_read = (uint32_t)PyBytes_Size(rv);
    return PyBytes_AsString(rv);
}

PyObject *parser_parse(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *source_or_callback;
    PyObject *old_tree_obj = NULL;
    bool keep_text = true;
    const char *encoding = "utf8";
    Py_ssize_t encoding_len = 4;
    char *keywords[] = {"", "old_tree", "encoding", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O!s#:parse", keywords, &source_or_callback,
                                     state->tree_type, &old_tree_obj, &encoding, &encoding_len)) {
        return NULL;
    }

    const TSTree *old_tree = old_tree_obj ? ((Tree *)old_tree_obj)->tree : NULL;

    TSInputEncoding input_encoding;
    if (strcmp(encoding, "utf8") == 0) {
        input_encoding = TSInputEncodingUTF8;
    } else if (strcmp(encoding, "utf16") == 0) {
        input_encoding = TSInputEncodingUTF16;
    } else {
        // try to normalize the encoding and check again
        PyObject *encodings = PyImport_ImportModule("encodings");
        if (encodings == NULL) {
            goto encoding_error;
        }
        PyObject *normalize_encoding = PyObject_GetAttrString(encodings, "normalize_encoding");
        Py_DECREF(encodings);
        if (normalize_encoding == NULL) {
            goto encoding_error;
        }
        PyObject *arg = PyUnicode_DecodeASCII(encoding, encoding_len, NULL);
        if (arg == NULL) {
            goto encoding_error;
        }
        PyObject *normalized_obj = PyObject_CallOneArg(normalize_encoding, arg);
        Py_DECREF(arg);
        Py_DECREF(normalize_encoding);
        if (normalized_obj == NULL) {
            goto encoding_error;
        }
        const char *normalized_str = PyUnicode_AsUTF8(normalized_obj);
        if (strcmp(normalized_str, "utf8") == 0 || strcmp(normalized_str, "utf_8") == 0) {
            Py_DECREF(normalized_obj);
            input_encoding = TSInputEncodingUTF8;
        } else if (strcmp(normalized_str, "utf16") == 0 || strcmp(normalized_str, "utf_16") == 0) {
            Py_DECREF(normalized_obj);
            input_encoding = TSInputEncodingUTF16;
        } else {
            Py_DECREF(normalized_obj);
            goto encoding_error;
        }
    }

    TSTree *new_tree = NULL;
    Py_buffer source_view;
    if (PyObject_GetBuffer(source_or_callback, &source_view, PyBUF_SIMPLE) > -1) {
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
            .previous_return_value = NULL,
        };
        TSInput input = {
            .payload = &payload,
            .read = parser_read_wrapper,
            .encoding = input_encoding,
        };
        new_tree = ts_parser_parse(self->parser, old_tree, input);
        Py_XDECREF(payload.previous_return_value);

        source_or_callback = Py_None;
        keep_text = false;
    } else {
        PyErr_SetString(PyExc_TypeError, "source must be a bytestring or a callable");
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

encoding_error:
    PyErr_Format(PyExc_ValueError, "encoding must be 'utf8' or 'utf16', not '%s'", encoding);
    return NULL;
}

PyObject *parser_reset(Parser *self, void *Py_UNUSED(payload)) {
    ts_parser_reset(self->parser);
    Py_RETURN_NONE;
}

PyObject *parser_get_timeout_micros(Parser *self, void *Py_UNUSED(payload)) {
    return PyLong_FromUnsignedLong(ts_parser_timeout_micros(self->parser));
}

int parser_set_timeout_micros(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
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
    Py_INCREF(self->language);
    return self->language;
}

int parser_set_language(Parser *self, PyObject *arg, void *Py_UNUSED(payload)) {
    ModuleState *state = GET_MODULE_STATE(self);
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
    if (language->version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
        TREE_SITTER_LANGUAGE_VERSION < language->version) {
        PyErr_Format(PyExc_ValueError,
                     "Incompatible Language version %u. Must be between %u and %u",
                     language->version, TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION,
                     TREE_SITTER_LANGUAGE_VERSION);
        return -1;
    }

    if (state->wasmtime_engine_type != NULL && ts_language_is_wasm(language->language)) {
        TSWasmEngine *engine = language->wasm_engine;
        if (engine == NULL) {
            PyErr_Format(PyExc_RuntimeError, "Language has no WASM engine");
            return -1;
        }
        // Allocate a new wasm store and assign it before loading this wasm language.
        // It would be possible to reuse the existing store if it belonged to the same
        // engine, but tree-sitter does not expose an API to verify that.
        TSWasmError error;
        TSWasmStore *store = ts_wasm_store_new(engine, &error);
        if (store == NULL) {
            PyErr_Format(PyExc_RuntimeError, "Failed to create TSWasmStore: %s", error.message);
            return -1;
        }
        ts_parser_set_wasm_store(self->parser, store);
    }

    if (!ts_parser_set_language(self->parser, language->language)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set the parser language");
        return -1;
    }

    Py_INCREF(language);
    Py_XSETREF(self->language, (PyObject *)language);
    return 0;
}

int parser_init(Parser *self, PyObject *args, PyObject *kwargs) {
    ModuleState *state = GET_MODULE_STATE(self);
    PyObject *language = NULL, *included_ranges = NULL, *timeout_micros = NULL;
    char *keywords[] = {"language", "included_ranges", "timeout_micros", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!$OO:__init__", keywords,
                                     state->language_type, &language, &included_ranges,
                                     &timeout_micros)) {
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
    {NULL},
};

static PyGetSetDef parser_accessors[] = {
    {"language", (getter)parser_get_language, (setter)parser_set_language,
     PyDoc_STR("The language that will be used for parsing."), NULL},
    {"included_ranges", (getter)parser_get_included_ranges, (setter)parser_set_included_ranges,
     PyDoc_STR("The ranges of text that the parser will include when parsing."), NULL},
    {"timeout_micros", (getter)parser_get_timeout_micros, (setter)parser_set_timeout_micros,
     PyDoc_STR("The duration in microseconds that parsing is allowed to take."), NULL},
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
