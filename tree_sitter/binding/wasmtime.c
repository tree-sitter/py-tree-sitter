#include <wasm.h>
#include <wasmtime.h>
#include "types.h"

#define own // Marker used by wasm declarations

typedef int (*func_ptr)();

static func_ptr get_dll_func(PyObject *dll, PyObject *cast, PyObject *c_void_p, const char *name) {
    func_ptr result = NULL;
    PyObject *py_func = NULL, *cast_result = NULL, *value_attr = NULL;

    // Get the function attribute from the dll object
    py_func = PyObject_GetAttrString(dll, name);
    if (py_func == NULL) {
        PyErr_Format(PyExc_AttributeError, "%s", name);
        goto cleanup;
    }

    // Call cast(py_func, c_void_p)
    cast_result = PyObject_CallFunctionObjArgs(cast, py_func, c_void_p, NULL);
    if (cast_result == NULL) {
        PyErr_SetString(PyExc_AttributeError, "Failed cast to c_void_p");
        goto cleanup;
    }

    // Get the 'value' attribute from the cast result
    value_attr = PyObject_GetAttrString(cast_result, "value");
    if (value_attr == NULL) {
        PyErr_SetString(PyExc_AttributeError, "c_void_p has no value");
        goto cleanup;
    }

    // Convert the value attribute to a C pointer
    result = (func_ptr)PyLong_AsVoidPtr(value_attr);

cleanup:
    Py_XDECREF(py_func);
    Py_XDECREF(cast_result);
    Py_XDECREF(value_attr);
    return result;
}

int wasmtime_available = 0;

// Declare placeholders for functions
#define WASMTIME_TRAMPOLINE(type, return, name, args, call) \
    static type (*real_##name) args = NULL; \
    type name args { return (*real_##name) call; }
#include "wasmtime_symbols.txt"
#undef WASMTIME_TRAMPOLINE

/// Loads wasmtime as an optional dependency.
void tsp_load_wasmtime_symbols() {
    PyObject *ctypes = NULL, *cast = NULL, *c_void_p = NULL, *wasmtime = NULL, *_ffi = NULL, *dll = NULL;
    ctypes = PyImport_ImportModule("ctypes");
    if (ctypes == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "ctypes module is missing");
        goto cleanup;
    }
    cast = PyObject_GetAttrString(ctypes, "cast");
    if (cast == NULL) {
        PyErr_SetString(PyExc_AttributeError, "ctypes module has no cast");
        goto cleanup;
    }
    c_void_p = PyObject_GetAttrString(ctypes, "c_void_p");
    if (c_void_p == NULL) {
        PyErr_SetString(PyExc_AttributeError, "ctypes module has no c_void_p");
        goto cleanup;
    }
    wasmtime = PyImport_ImportModule("wasmtime");
    if (wasmtime == NULL) {
        goto cleanup;
    }
    _ffi = PyObject_GetAttrString(wasmtime, "_ffi");
    if (_ffi == NULL) {
        PyErr_SetString(PyExc_AttributeError, "wasmtime module has no _ffi");
        goto cleanup;
    }
    dll = PyObject_GetAttrString(_ffi, "dll");
    if (dll == NULL) {
        PyErr_SetString(PyExc_AttributeError, "wasmtime._ffi module has no dll");
        goto cleanup;
    }

#define WASMTIME_TRAMPOLINE(type, return, name, args, call) do { \
        real_##name = (type (*) args) get_dll_func(dll, cast, c_void_p, #name); \
        if (real_##name == NULL) { \
            goto cleanup; \
        } \
    } while (false);
#include "wasmtime_symbols.txt"
#undef WASMTIME_TRAMPOLINE

    wasmtime_available = 1;

cleanup:
    Py_XDECREF(dll);
    Py_XDECREF(_ffi);
    Py_XDECREF(wasmtime);
    Py_XDECREF(c_void_p);
    Py_XDECREF(cast);
    Py_XDECREF(ctypes);
}
