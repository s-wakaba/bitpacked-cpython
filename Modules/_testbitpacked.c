/* C Extension module to test all aspects of PEP-3118.
   Written by Stefan Krah. */

#define PY_SSIZE_T_CLEAN

#include "Python.h"

#define MODE_BITPACKED 1L
#define MODE_NOREFCNT 2L
#define MODE_NOERRDETECT 4L
#define MODE_PYDEBUG 8L
#define MODE_PYTRACEREFS 16L
#define MODE_PYREFDEBUG 32L


static PyObject *
get_mode(PyObject *self)
{
    long mode = 0;
#ifdef BITPACKED
    mode |= MODE_BITPACKED;
#if BITPACKED_NOREFCNT
    mode |= MODE_NOREFCNT;
#endif
#if BITPACKED_NOERRDETECT
    mode |= MODE_NOERRDETECT;
#endif
#endif
#ifdef Py_DEBUG
    mode |= MODE_PYDEBUG;
#endif
#ifdef Py_TRACE_REFS
    mode |= MODE_PYTRACEREFS;
#endif
#ifdef Py_REF_DEBUG
    mode |= MODE_PYREFDEBUG;
#endif

    return PyLong_FromLong(mode);
}

static PyObject *
print_bitpacked_configuration(PyObject *self)
{
    PyObject *modeobj = get_mode(self);
    long mode = PyLong_AsLong(modeobj);
    FILE *fp = stderr;
    Py_DECREF(modeobj);

#define MODE_CHECK(M) (mode & (M) ? "ON" : "OFF")
    
    fprintf(fp, "\n");
    fprintf(fp, "BitPacked Mode: %s\n", MODE_CHECK(MODE_BITPACKED));
    fprintf(fp, "BitPacked No-RefCount Mode: %s\n", MODE_CHECK(MODE_NOREFCNT));
    fprintf(fp, "BitPacked No-Error-Detection Mode: %s\n", MODE_CHECK(MODE_NOERRDETECT));
    fprintf(fp, "Python Debug Mode: %s\n", MODE_CHECK(MODE_PYDEBUG));
    fprintf(fp, "Python Trace-Refs Mode: %s\n", MODE_CHECK(MODE_PYTRACEREFS));
    fprintf(fp, "Python Refs-Debug Mode: %s\n", MODE_CHECK(MODE_PYREFDEBUG));

    Py_RETURN_NONE;
}

static PyObject *
get_typetable(PyObject *self)
{
#ifdef BITPACKED
    Py_ssize_t i, n = sizeof(bitpacked_types)/sizeof(bitpacked_types[0]);
    PyObject *res = PyList_New(n);
    if(res){
        for(i = 0; i < n; ++i){
            PyObject *p = (PyObject*)(bitpacked_types[i]);
            if(!p) p = Py_None;
            Py_INCREF(p);
            PyList_SET_ITEM(res, i, p);
        }
    }
    return res;
#else
    Py_RETURN_NONE;
#endif
}

#define N_SIDEEFFECT 10
static PyObject *
test_macro_sideeffect(PyObject *self)
{
    PyObject * ret = NULL;
    PyObject * list1 = NULL;
    PyObject * list2 = NULL;
    PyObject * list3 = NULL;

    int i, j;
    PyObject *tmp;

    list1 = PyList_New(0);
    if(!list1) goto exit;
    list2 = PyList_New(N_SIDEEFFECT);
    if(!list2) goto exit;
    list3 = PyList_New(N_SIDEEFFECT);
    if(!list3) goto exit;

    for(i = 0; i < N_SIDEEFFECT; ++i) {
        tmp = PyUnicode_FromFormat("Object No.%d", i);
        if(!tmp) goto exit;
        for(j = 0; j < i; ++j) {
            if(PyList_Append(list1, tmp) == -1) goto exit;
        }
        PyList_SET_ITEM(list2, i, tmp);

        tmp = PyFloat_FromDouble(i * 1.0);
        if(!tmp) goto exit;
        PyList_SET_ITEM(list3, i, tmp);
    }
    {
        int refcnt1, refcnt2;
        i = 5;
        refcnt1 = Py_REFCNT(PyList_GetItem(list2, ++i));
        i = 5;
        tmp = PyList_GetItem(list2, ++i);
        refcnt2 = Py_REFCNT(tmp);
        fprintf(stderr, "refcnt1=%d, refcnt2=%d\n", refcnt1, refcnt2);
    }
    {
        double fval1, fval2;
        i = 5;
        tmp = PyList_GetItem(list3, ++i);
        fval1 = PyFloat_AS_DOUBLE(tmp);
        i = 5;
        fval2 = PyFloat_AS_DOUBLE(PyList_GetItem(list3, ++i));
        fprintf(stderr, "fval1=%f, fval2=%f\n", fval1, fval2);
    }

    ret = Py_None;
    Py_INCREF(ret);
  exit:
    Py_XDECREF(list1);
    Py_XDECREF(list2);
    Py_XDECREF(list3);
    return ret;
}

static struct PyMethodDef _testbitpacked_functions[] = {
    {"test_macro_sideeffect", (PyCFunction)test_macro_sideeffect, METH_NOARGS, NULL},
    {"get_mode", (PyCFunction)get_mode, METH_NOARGS, NULL},
    {"get_typetable", (PyCFunction)get_typetable, METH_NOARGS, NULL},
    {"print_bitpacked_configuration", (PyCFunction)print_bitpacked_configuration, METH_NOARGS, NULL},
    {NULL, NULL}
};

static struct PyModuleDef _testbitpackedmodule = {
    PyModuleDef_HEAD_INIT,
    "_testbitpacked",
    NULL,
    -1,
    _testbitpacked_functions,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit__testbitpacked(void)
{
    PyObject *m;

    m = PyModule_Create(&_testbitpackedmodule);
    if (m == NULL)
        return NULL;

    PyModule_AddIntMacro(m, MODE_BITPACKED);
    PyModule_AddIntMacro(m, MODE_NOREFCNT);
    PyModule_AddIntMacro(m, MODE_NOERRDETECT);
    PyModule_AddIntMacro(m, MODE_PYDEBUG);
    PyModule_AddIntMacro(m, MODE_PYTRACEREFS);
    PyModule_AddIntMacro(m, MODE_PYREFDEBUG);
#ifdef BITPACKED
    PyModule_AddIntMacro(m, BITPACKED_DUMMY_REFCNT);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_LONG);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NONE);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTIMPL);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_FLOAT);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_RANGE);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTUSED_0E);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_BOOL);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTUSED_14);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTUSED_16);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_FLOAT_RSV);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTUSED_1C);
    PyModule_AddIntMacro(m, BITPACKED_TYPEID_NOTUSED_1E);
#endif

    return m;
}

