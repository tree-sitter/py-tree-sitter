#include "point.h"

PyObject *point_new(TSPoint point) {
    PyObject *row = PyLong_FromSize_t((size_t)point.row);
    PyObject *column = PyLong_FromSize_t((size_t)point.column);
    if (!row || !column) {
        Py_XDECREF(row);
        Py_XDECREF(column);
        return NULL;
    }

    PyObject *obj = PyTuple_Pack(2, row, column);
    Py_XDECREF(row);
    Py_XDECREF(column);
    return obj;
}
