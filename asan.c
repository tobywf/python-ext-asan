#define PY_SSIZE_T_CLEAN
#include <Python.h>
// #include <stdlib.h>

static PyObject *
asan_test(PyObject *Py_UNUSED(self), PyObject *args, PyObject *kwargs)
{
    int stack_array[100];
    int index = 0;
    int leak = 0; // FALSE
    static char *kwlist[] = {"index", "leak", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|p", kwlist, &index, &leak)) {
        return NULL;
    }

    int result;
    if (leak) {
        int *heap_array = (int *)malloc(100 * sizeof(int));
        heap_array[1] = 1;
        result = heap_array[index] == 1;
        heap_array = NULL;
    } else {
        stack_array[1] = 1;
        result = stack_array[index] == 1;
    }

    if (result) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyMethodDef asan_methods[] = {
    {"test", (PyCFunction)asan_test, METH_VARARGS | METH_KEYWORDS, "TEST ME"},
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef asan_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "asan",
    .m_methods = asan_methods,
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit_asan(void)
{
    return PyModule_Create(&asan_module);
}
