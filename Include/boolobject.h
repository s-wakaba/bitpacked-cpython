/* Boolean object interface */

#ifndef Py_BOOLOBJECT_H
#define Py_BOOLOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyBool_Type;

#ifdef BITPACKED
#define PyBool_Check(x) (BITPACKED_TYPEID(x) == BITPACKED_TYPEID_BOOL)
#define Py_False ((PyObject *)(BITPACKED_TYPEID_BOOL))
#define Py_True ((PyObject *)(BITPACKED_TYPEID_BOOL | (1UL << 32)))
#else
#define PyBool_Check(x) (Py_TYPE(x) == &PyBool_Type)

/* Py_False and Py_True are the only two bools in existence.
Don't forget to apply Py_INCREF() when returning either!!! */

/* Don't use these directly */
PyAPI_DATA(struct _longobject) _Py_FalseStruct, _Py_TrueStruct;

/* Use these macros */
#define Py_False ((PyObject *) &_Py_FalseStruct)
#define Py_True ((PyObject *) &_Py_TrueStruct)
#endif

/* Macros for returning Py_True or Py_False, respectively */
#define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#define Py_RETURN_FALSE return Py_INCREF(Py_False), Py_False

/* Function to return a bool from a C long */
PyAPI_FUNC(PyObject *) PyBool_FromLong(long);

#ifdef __cplusplus
}
#endif
#endif /* !Py_BOOLOBJECT_H */
