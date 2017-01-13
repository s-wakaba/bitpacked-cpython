/* Long (arbitrary precision) integer object implementation */

/* GNU MP Document => https://gmplib.org/manual/ */

#include "Python.h"
#include "longintrepr.h"

#define mpz_init_set_pylong(mx, x) \
    mpz_roinit_n((mx), ((PyLongObject*)(x))->ob_digit, Py_SIZE(x))
#define mpz_init_set_abs_pylong(mx, x) \
    mpz_roinit_n((mx), ((PyLongObject*)(x))->ob_digit, Py_ABS(Py_SIZE(x)))

#if __GNU_MP_VERSION < 6
static mp_limb_t *
mpz_limbs_read (mpz_ptr x)
{
    return x->_mp_d;
}

static mpz_ptr
mpz_roinit_n (mpz_ptr x, mp_limb_t *p, ssize_t s)
{
    assert(s == 0 || p[Py_ABS(s)-1] > 0);
    x->_mp_alloc = 0;
    x->_mp_size = s;
    x->_mp_d = p;
    return x;
}
#endif

static mpz_t mpz_cached_object;
/* TODO: temporary mpz_t variable mpz_cached_object should be declared
 * as thread local storage
 */ 
#define MPZ_CACHED_RESULT_BEGIN(result)          \
do{                                              \
    mpz_ptr result = mpz_cached_object;          \
    do
#define MPZ_CACHED_RESULT_END                    \
    while(0);                                    \
    {                                            \
        int reused;                              \
        PyObject* p = mpz_eval_as_pylong(        \
                    mpz_cached_object, &reused); \
        if(reused)                               \
            mpz_init2(mpz_cached_object, 1);     \
        return (void*)p;                         \
    }                                            \
}while(0)

#include <float.h>
#include <ctype.h>
#include <stddef.h>

typedef long sdigit; /* dummy of signed variant of digit */

#ifndef NSMALLPOSINTS
#define NSMALLPOSINTS           257
#endif
#ifndef NSMALLNEGINTS
#define NSMALLNEGINTS           5
#endif

#if NSMALLNEGINTS + NSMALLPOSINTS > 0
/* Small integers are preallocated in this array so that they
   can be shared.
   The integers that are preallocated are those in the range
   -NSMALLNEGINTS (inclusive) to NSMALLPOSINTS (not inclusive).
*/
static PyLongObject small_ints[NSMALLNEGINTS + NSMALLPOSINTS];
#ifdef COUNT_ALLOCS
Py_ssize_t quick_int_allocs, quick_neg_int_allocs;
#endif

static PyObject *
get_small_int(sdigit ival)
{
    PyObject *v;
    assert(-NSMALLNEGINTS <= ival && ival < NSMALLPOSINTS);
    v = (PyObject *)&small_ints[ival + NSMALLNEGINTS];
    Py_INCREF(v);
#ifdef COUNT_ALLOCS
    if (ival >= 0)
        quick_int_allocs++;
    else
        quick_neg_int_allocs++;
#endif
    return v;
}
#define CHECK_SMALL_INT(ival) \
    do if (-NSMALLNEGINTS <= ival && ival < NSMALLPOSINTS) { \
        return get_small_int((sdigit)ival); \
    } while(0)
#else
#define CHECK_SMALL_INT(ival)
#endif

#ifndef PyLong_GMP_ALLOC_CHECK
#ifdef Py_DEBUG
#define PyLong_GMP_ALLOC_CHECK 1
#endif
#endif

#define PYLONG_HEADSIZE (offsetof(PyLongObject, ob_digit))
#ifdef PyLong_GMP_ALLOC_CHECK
#define PYLONG_DUMMYSIZE (0x6A3F5EL)
#define PYLONG_DUMMYTYPE ((PyTypeObject*)0x65EA3BL)
#endif
static void *
mpz_custom_malloc(size_t size)
{
    PyLongObject *p = (PyLongObject*)PyObject_Malloc(PYLONG_HEADSIZE + size);
#ifdef PyLong_GMP_ALLOC_CHECK
    Py_SIZE(p) = PYLONG_DUMMYSIZE;
    Py_TYPE(p) = PYLONG_DUMMYTYPE;
#endif
    return (char*)p + PYLONG_HEADSIZE;
}

static void *
mpz_custom_realloc(void *ptr, size_t old_size, size_t new_size)
{
    PyLongObject *p = (PyLongObject*)((char*)ptr - PYLONG_HEADSIZE);
#ifdef PyLong_GMP_ALLOC_CHECK
    assert(Py_SIZE(p) = PYLONG_DUMMYSIZE);
#endif
    p = (PyLongObject*)PyObject_Realloc(p, PYLONG_HEADSIZE + new_size);
#ifdef PyLong_GMP_ALLOC_CHECK
    assert(Py_SIZE(p) = PYLONG_DUMMYSIZE);
#endif
    return (char*)p + PYLONG_HEADSIZE;
}

static void
mpz_custom_free(void *ptr, size_t size)
{
    PyLongObject *p = (PyLongObject*)((char*)ptr - PYLONG_HEADSIZE);
#ifdef PyLong_GMP_ALLOC_CHECK
    assert(Py_SIZE(p) = PYLONG_DUMMYSIZE);
#endif
    PyObject_Free(p);
}

__attribute__((constructor)) static void
mpz_set_custom_allocators(void)
{
    /* FIXME: this function is only functional with GCC and compatible compilers.
     * some APIs (e.g. PyLong_FromVoidPtr) are called before calling _PyLong_Init.
     * So allocators have to set before them...
     */
    mp_set_memory_functions(mpz_custom_malloc, mpz_custom_realloc, mpz_custom_free);
    mpz_init2(mpz_cached_object, 1);
}

static PyObject*
mpz_reuse_as_pylong(const mpz_ptr pp)
{
    PyLongObject *p;
    p = (PyLongObject*)((char*)mpz_limbs_read(pp) - PYLONG_HEADSIZE);
#ifdef PyLong_GMP_ALLOC_CHECK
    assert((Py_SIZE(p) == PYLONG_DUMMYSIZE) && (Py_TYPE(p) == PYLONG_DUMMYTYPE));
#endif
#if 0
    return (PyObject*)PyObject_INIT_VAR(p, &PyLong_Type,
                             mpz_size(pp) * (mpz_sgn(pp) >= 0 ? 1 : -1));
#else
    return (PyObject*)PyObject_INIT_VAR(p, &PyLong_Type, pp->_mp_size);
#endif
}
static PyObject*
mpz_eval_as_pylong(const mpz_ptr pp, int *reused)
{
    *reused = 0;
    if(mpz_fits_slong_p(pp)){
        long ival = mpz_get_si(pp);
        CHECK_SMALL_INT(ival);
    }
    *reused = 1;
    return mpz_reuse_as_pylong(pp);
}


/* _PyLong_FromNbInt: Convert the given object to a PyLongObject
   using the nb_int slot, if available.  Raise TypeError if either the
   nb_int slot is not available or the result of the call to nb_int
   returns something not of type int.
*/
PyLongObject *
_PyLong_FromNbInt(PyObject *integral)
{
    PyNumberMethods *nb;
    PyObject *result;

    /* Fast path for the case that we already have an int. */
    if (PyLong_CheckExact(integral)) {
        Py_INCREF(integral);
        return (PyLongObject *)integral;
    }

    nb = Py_TYPE(integral)->tp_as_number;
    if (nb == NULL || nb->nb_int == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "an integer is required (got type %.200s)",
                     Py_TYPE(integral)->tp_name);
        return NULL;
    }

    /* Convert using the nb_int slot, which should return something
       of exact type int. */
    result = nb->nb_int(integral);
    if (!result || PyLong_CheckExact(result))
        return (PyLongObject *)result;
    if (!PyLong_Check(result)) {
        PyErr_Format(PyExc_TypeError,
                     "__int__ returned non-int (type %.200s)",
                     result->ob_type->tp_name);
        Py_DECREF(result);
        return NULL;
    }
    /* Issue #17576: warn if 'result' not of exact type int. */
    if (PyErr_WarnFormat(PyExc_DeprecationWarning, 1,
            "__int__ returned non-int (type %.200s).  "
            "The ability to return an instance of a strict subclass of int "
            "is deprecated, and may be removed in a future version of Python.",
            result->ob_type->tp_name)) {
        Py_DECREF(result);
        return NULL;
    }
    return (PyLongObject *)result;
}


/* Allocate a new int object with size digits.
   Return NULL and set exception if we run out of memory. */

#define MAX_LONG_DIGITS \
    ((PY_SSIZE_T_MAX - PYLONG_HEADSIZE)/sizeof(digit))

PyLongObject *
_PyLong_New(Py_ssize_t size)
{
    PyLongObject *result;
    /* Number of bytes needed is: offsetof(PyLongObject, ob_digit) +
       sizeof(digit)*size.  Previous incarnations of this code used
       sizeof(PyVarObject) instead of the offsetof, but this risks being
       incorrect in the presence of padding between the PyVarObject header
       and the digits. */
    if (size > (Py_ssize_t)MAX_LONG_DIGITS) {
        PyErr_SetString(PyExc_OverflowError,
                        "too many digits in integer");
        return NULL;
    }
    result = PyObject_MALLOC(PYLONG_HEADSIZE +
                             size*sizeof(digit));
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }
    return (PyLongObject*)PyObject_INIT_VAR(result, &PyLong_Type, size);
}

#define COPY_DIGITS(dst, src) do{                         \
    PyLongObject* dst_ = (PyLongObject*)(dst);            \
    PyLongObject* src_ = (PyLongObject*)(src);            \
    Py_ssize_t i = Py_ABS(Py_SIZE(src_));                 \
    while(i--) dst_->ob_digit[i] = src_->ob_digit[i];     \
    Py_SIZE(dst_) = Py_SIZE(src_);                        \
}while(0)


PyObject *
_PyLong_Copy(PyLongObject *src)
{
    mpz_t msrc, mdst;
    mpz_init_set_pylong(msrc, src);
    if(mpz_fits_sshort_p(msrc)){
        long ival = mpz_get_si(msrc);
        CHECK_SMALL_INT(ival);
    }
    mpz_init_set(mdst, msrc);
    return mpz_reuse_as_pylong(mdst);
}

/* Create a new int object from a C long int */

PyObject *
PyLong_FromLong(long ival)
{
    mpz_t ret;
    CHECK_SMALL_INT(ival);
    mpz_init_set_si(ret, ival);
    return mpz_reuse_as_pylong(ret);
}

/* Create a new int object from a C unsigned long int */

PyObject *
PyLong_FromUnsignedLong(unsigned long ival)
{
    mpz_t ret;
    if(ival < NSMALLPOSINTS) CHECK_SMALL_INT((long)ival);
    mpz_init_set_ui(ret, ival);
    return mpz_reuse_as_pylong(ret);
}

/* Create a new int object from a C double */

PyObject *
PyLong_FromDouble(double dval)
{
    if (Py_IS_INFINITY(dval)) {
        PyErr_SetString(PyExc_OverflowError,
                        "cannot convert float infinity to integer");
        return NULL;
    }
    if (Py_IS_NAN(dval)) {
        PyErr_SetString(PyExc_ValueError,
                        "cannot convert float NaN to integer");
        return NULL;
    }

    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_init_set_d(zzret, dval);
    }MPZ_CACHED_RESULT_END;
}

#define TOO_LARGE(type) \
        PyErr_SetString(PyExc_OverflowError, \
               "Python int too large to convert to " type)
#define ASSURE_PYLONG_OBJECT_BEGIN(v, vv)               \
do{                                                     \
    PyLongObject *v, *_temporary_long_object = NULL;    \
    if(vv == NULL){                                     \
        PyErr_BadInternalCall();                        \
        return -1;                                      \
    }                                                   \
    if(PyLong_Check(vv)){                               \
        v = (PyLongObject*)vv;                          \
    }else{                                              \
        v = _PyLong_FromNbInt(vv);                      \
        if (v == NULL)                                  \
            return -1;                                  \
        _temporary_long_object = v;                     \
    }                                                   \
    do
#define ASSURE_PYLONG_OBJECT_END                        \
    while(0);                                           \
    if (_temporary_long_object)                         \
        Py_DECREF(_temporary_long_object);              \
}while(0)
#define CONFIRM_PYLONG_OBJECT(vv) do{                   \
    if (vv == NULL) {                                   \
        PyErr_BadInternalCall();                        \
        return -1;                                      \
    }                                                   \
    if (!PyLong_Check(vv)) {                            \
        PyErr_SetString(PyExc_TypeError, "an integer is required"); \
        return -1;                                      \
    }                                                   \
} while(0)






/* Get a C long int from an int object or any object that has an __int__
   method.

   On overflow, return -1 and set *overflow to 1 or -1 depending on the sign of
   the result.  Otherwise *overflow is 0.

   For other errors (e.g., TypeError), return -1 and set an error condition.
   In this case *overflow will be 0.
*/

long
PyLong_AsLongAndOverflow(PyObject *vv, int *overflow)
{
    long res = -1;
    *overflow = 0;
    ASSURE_PYLONG_OBJECT_BEGIN(v, vv){
        mpz_t zz;
        mpz_init_set_pylong(zz, v);
        if(mpz_fits_slong_p(zz)){
            res = mpz_get_si(zz);
        } else {
            *overflow = mpz_sgn(zz) > 0 ? 1 : -1;
        }
    }ASSURE_PYLONG_OBJECT_END;
    return res;
}

/* Get a C long int from an int object or any object that has an __int__
   method.  Return -1 and set an error if overflow occurs. */

long
PyLong_AsLong(PyObject *obj)
{
    int overflow;
    long result = PyLong_AsLongAndOverflow(obj, &overflow);
    if (overflow) {
        /* XXX: could be cute and give a different
           message for overflow == -1 */
        TOO_LARGE("C long");
    }
    return result;
}

/* Get a C int from an int object or any object that has an __int__
   method.  Return -1 and set an error if overflow occurs. */

int
_PyLong_AsInt(PyObject *obj)
{
    int overflow;
    long result = PyLong_AsLongAndOverflow(obj, &overflow);
    if (overflow || result > INT_MAX || result < INT_MIN) {
        /* XXX: could be cute and give a different
           message for overflow == -1 */
        TOO_LARGE("C int");
        return -1;
    }
    return (int)result;
}

/* Get a Py_ssize_t from an int object.
   Returns -1 and sets an error condition if overflow occurs. */

Py_ssize_t
PyLong_AsSsize_t(PyObject *vv) {
    Py_ssize_t ret;
    CONFIRM_PYLONG_OBJECT(vv);

    if(_PyLong_AsByteArray((PyLongObject *)vv, (unsigned char*)&ret,
                           sizeof(ret), PY_LITTLE_ENDIAN, 1) < 0){
        assert(PyErr_Occurred());
        PyErr_Clear();
        TOO_LARGE("C ssize_t");
        ret = -1;
    }
    return ret;
}

/* Get a C unsigned long int from an int object.
   Returns -1 and sets an error condition if overflow occurs. */

unsigned long
PyLong_AsUnsignedLong(PyObject *vv)
{
    unsigned long x;
    mpz_t zz;
    CONFIRM_PYLONG_OBJECT(vv);

    x = -1;
    mpz_init_set_pylong(zz, vv);
    if(mpz_fits_ulong_p(zz)){
        x = mpz_get_ui(zz);
    }else if(mpz_sgn(zz) < 0){
        PyErr_SetString(PyExc_OverflowError,
                        "can't convert negative value to unsigned int");
    }else{
        TOO_LARGE("C unsigned long");
    }

    return x;
}

/* Get a C size_t from an int object. Returns (size_t)-1 and sets
   an error condition if overflow occurs. */

size_t
PyLong_AsSize_t(PyObject *vv)
{
    size_t ret;

    CONFIRM_PYLONG_OBJECT(vv);

    if (Py_SIZE(vv) < 0) {
        PyErr_SetString(PyExc_OverflowError,
                   "can't convert negative value to size_t");
        return (size_t) -1;
    }

    if(_PyLong_AsByteArray((PyLongObject *)vv, (unsigned char*)&ret,
                           sizeof(ret), PY_LITTLE_ENDIAN, 0) < 0){
        assert(PyErr_Occurred());
        PyErr_Clear();
        TOO_LARGE("C size_t");
        ret = -1;
    }
    return ret;
}

/* Get a C unsigned long int from an int object, ignoring the high bits.
   Returns -1 and sets an error condition if an error occurs. */

unsigned long
PyLong_AsUnsignedLongMask(PyObject *op)
{
    unsigned long val = -1;
    ASSURE_PYLONG_OBJECT_BEGIN(v, op){
        Py_ssize_t i = Py_SIZE(v);
        int sign = 1;
        mpz_t zz, mv;
        size_t n = 0;
        val = 0;
        if (i < 0) {
            sign = -1;
            i = -i;
        }
        mpz_init(zz);
        mpz_fdiv_r_2exp(zz, mpz_init_set_abs_pylong(mv, v), sizeof(val) * 8);
        mpz_export(&val, &n, -1, sizeof(val), 0, 0, zz);
        assert(n <= 1);
        mpz_clear(zz);
        val = val * sign;
    }ASSURE_PYLONG_OBJECT_END;
    return val;
}

int
_PyLong_Sign(PyObject *vv)
{
    PyLongObject *v = (PyLongObject *)vv;

    assert(v != NULL);
    assert(PyLong_Check(v));

    return Py_SIZE(v) == 0 ? 0 : (Py_SIZE(v) < 0 ? -1 : 1);
}

size_t
_PyLong_NumBits(PyObject *vv)
{
    PyLongObject *v = (PyLongObject *)vv;
    size_t result = 0;

    assert(v != NULL);
    assert(PyLong_Check(v));
    if (Py_SIZE(v) != 0) {
        mpz_t zz;
        result = mpz_sizeinbase(mpz_init_set_pylong(zz, v), 2);
    }
    return result;
}

PyObject *
_PyLong_FromByteArray(const unsigned char* bytes, size_t n,
                      int little_endian, int is_signed)
{
#define BYTE_DIGIT(i) (little_endian ? (i) : n-1-(i))
    if (n == 0)
        return PyLong_FromLong(0L);

    MPZ_CACHED_RESULT_BEGIN(zzret){
        if(is_signed && (bytes[BYTE_DIGIT(n-1)] & 0x80U)){
            unsigned char *buf;
            unsigned long x = 1;
            size_t i;
            buf = PyObject_Malloc(n);
            if(buf == NULL) abort();
            for(i = 0; i < n; ++i) {
                x += (~bytes[BYTE_DIGIT(i)]) & 0xFFU;
                buf[i] = x & 0xFFU;
                x >>= 8;
            }
            mpz_import(zzret, n, -1, 1, 0, 0, buf);
            PyObject_Free(buf);
            mpz_neg(zzret, zzret);
        }else{
            mpz_import(zzret, n, little_endian ? -1 : 1, 1, 0, 0, bytes);
        }
    }MPZ_CACHED_RESULT_END;
#undef BYTE_DIGIT
}

int
_PyLong_AsByteArray(PyLongObject* v,
                    unsigned char* bytes, size_t n,
                    int little_endian, int is_signed)
{
    mpz_t zz;
    int ret = -1;
    int sgn, toobig = 0;
    size_t n2 = 0, i;

    assert(v != NULL && PyLong_Check(v));

    mpz_init_set_pylong(zz, v);
    sgn = mpz_sgn(zz);
    if ((sgn < 0) && !is_signed) {
        PyErr_SetString(PyExc_OverflowError,
                        "can't convert negative int to unsigned");
        goto exit_pylong_asbytearry;
    }
    if((n == 0) && (sgn == 0)){
        ret = 0;
        goto exit_pylong_asbytearry;
    }
    if(n * 2 < mpz_sizeinbase(zz, 16)){
        toobig = 1;
        goto exit_pylong_asbytearry;
    }
    mpz_export(bytes, &n2, little_endian ? -1 : 1, 1, 0, 0, zz);
    assert(n >= n2);
    if(little_endian){
        for(i = n2; i < n; ++i){
            bytes[i] = 0;
            n2 += 1;
        }
    }else{
        if(n2 < n){
            for(i = 1; i <= n2; ++i){
                bytes[n-i] = bytes[n2-i];
            }
            for(i = n2+1; i <= n; ++i){
                bytes[n-i] = 0;
            }
        }
    }

    if(is_signed){
#define BYTE_DIGIT(i) (little_endian ? (i) : n-1-(i))
        if(sgn < 0){
            size_t i;
            unsigned long x = 1;
            for(i=0; i<n; ++i){
                x += (~bytes[BYTE_DIGIT(i)]) & 0xFFU;
                bytes[BYTE_DIGIT(i)] = x & 0xFFU;
                x >>= 8;
            }
            if(x || ((bytes[BYTE_DIGIT(n-1)] & 0x80U) == 0)){
                toobig = 1;
                goto exit_pylong_asbytearry;
            }
        }else{
            if((bytes[BYTE_DIGIT(n-1)] & 0x80U) != 0){
                toobig = 1;
                goto exit_pylong_asbytearry;
            }
        }
#undef BYTE_DIGIT
    }
    ret = 0;
  exit_pylong_asbytearry:
    if(toobig)
        PyErr_SetString(PyExc_OverflowError, "int too big to convert");
    return ret;
}

/* Create a new int object from a C pointer */

PyObject *
PyLong_FromVoidPtr(void *p)
{
#if SIZEOF_VOID_P <= SIZEOF_LONG
    return PyLong_FromUnsignedLong((unsigned long)(uintptr_t)p);
#else

#if SIZEOF_LONG_LONG < SIZEOF_VOID_P
#   error "PyLong_FromVoidPtr: sizeof(long long) < sizeof(void*)"
#endif
    return PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)p);
#endif /* SIZEOF_VOID_P <= SIZEOF_LONG */

}

/* Get a C pointer from an int object. */

void *
PyLong_AsVoidPtr(PyObject *vv)
{
#if SIZEOF_VOID_P <= SIZEOF_LONG
    long x;

    if (PyLong_Check(vv) && _PyLong_Sign(vv) < 0)
        x = PyLong_AsLong(vv);
    else
        x = PyLong_AsUnsignedLong(vv);
#else

#if SIZEOF_LONG_LONG < SIZEOF_VOID_P
#   error "PyLong_AsVoidPtr: sizeof(long long) < sizeof(void*)"
#endif
    long long x;

    if (PyLong_Check(vv) && _PyLong_Sign(vv) < 0)
        x = PyLong_AsLongLong(vv);
    else
        x = PyLong_AsUnsignedLongLong(vv);

#endif /* SIZEOF_VOID_P <= SIZEOF_LONG */

    if (x == -1 && PyErr_Occurred())
        return NULL;
    return (void *)x;
}

/* Initial long long support by Chris Herborth (chrish@qnx.com), later
 * rewritten to use the newer PyLong_{As,From}ByteArray API.
 */

#define PY_ABS_LLONG_MIN (0-(unsigned long long)PY_LLONG_MIN)

/* Create a new int object from a C long long int. */

PyObject *
PyLong_FromLongLong(long long ival)
{
#if SIZEOF_LONG_LONG == SIZEOF_LONG
    return PyLong_FromLong(ival);
#else
    return _PyLong_FromByteArray((unsigned char*)&ival, sizeof(ival),
                                 PY_LITTLE_ENDIAN, 1);
#endif
}

/* Create a new int object from a C unsigned long long int. */

PyObject *
PyLong_FromUnsignedLongLong(unsigned long long ival)
{
#if SIZEOF_LONG_LONG == SIZEOF_LONG
    return PyLong_FromUnsignedLong(ival);
#else
    return _PyLong_FromByteArray((unsigned char*)&ival, sizeof(ival),
                                 PY_LITTLE_ENDIAN, 0);
#endif
}

/* Create a new int object from a C Py_ssize_t. */

PyObject *
PyLong_FromSsize_t(Py_ssize_t ival)
{
#if SIZEOF_SIZE_T == SIZEOF_LONG
    return PyLong_FromLong(ival);
#else
    return _PyLong_FromByteArray((unsigned char*)&ival, sizeof(ival),
                                 PY_LITTLE_ENDIAN, 1);
#endif
}

/* Create a new int object from a C size_t. */

PyObject *
PyLong_FromSize_t(size_t ival)
{
#if SIZEOF_SIZE_T == SIZEOF_LONG
    return PyLong_FromUnsignedLong(ival);
#else
    return _PyLong_FromByteArray((unsigned char*)&ival, sizeof(ival),
                                 PY_LITTLE_ENDIAN, 0);
#endif
}

/* Get a C long long int from an int object or any object that has an
   __int__ method.  Return -1 and set an error if overflow occurs. */

long long
PyLong_AsLongLong(PyObject *vv)
{
    long long bytes;
    ASSURE_PYLONG_OBJECT_BEGIN(v, vv){
        if(_PyLong_AsByteArray(v, (unsigned char*)&bytes, sizeof(bytes),
                               PY_LITTLE_ENDIAN, 1) < 0){
            assert(PyErr_Occurred());
            bytes = (long long)-1;
        }
    }ASSURE_PYLONG_OBJECT_END;
    /* Plan 9 can't handle long long in ? : expressions */
    return bytes;
}

/* Get a C unsigned long long int from an int object.
   Return -1 and set an error if overflow occurs. */

unsigned long long
PyLong_AsUnsignedLongLong(PyObject *vv)
{
    unsigned long long bytes;
    int res;

    CONFIRM_PYLONG_OBJECT(vv);
    res = _PyLong_AsByteArray((PyLongObject *)vv, (unsigned char *)&bytes,
                              SIZEOF_LONG_LONG, PY_LITTLE_ENDIAN, 0);

    /* Plan 9 can't handle long long in ? : expressions */
    if (res < 0)
        return (unsigned long long)res;
    else
        return bytes;
}

/* Get a C unsigned long int from an int object, ignoring the high bits.
   Returns -1 and sets an error condition if an error occurs. */

unsigned long long
PyLong_AsUnsignedLongLongMask(PyObject *op)
{
    unsigned long long val = -1;
    ASSURE_PYLONG_OBJECT_BEGIN(v, op){
        Py_ssize_t i = Py_SIZE(v);
        int sign = 1;
        mpz_t zz, mv;
        size_t n = 0;
        val = 0;
        if (i < 0) {
            sign = -1;
            i = -i;
        }
        mpz_init(zz);
        mpz_fdiv_r_2exp(zz, mpz_init_set_abs_pylong(mv, v), sizeof(val) * 8);
        mpz_export(&val, &n, -1, sizeof(val), 0, 0, zz);
        assert(n <= 1);
        mpz_clear(zz);
        val = val * sign;
    }ASSURE_PYLONG_OBJECT_END;
    return val;
}

/* Get a C long long int from an int object or any object that has an
   __int__ method.

   On overflow, return -1 and set *overflow to 1 or -1 depending on the sign of
   the result.  Otherwise *overflow is 0.

   For other errors (e.g., TypeError), return -1 and set an error condition.
   In this case *overflow will be 0.
*/

long long
PyLong_AsLongLongAndOverflow(PyObject *vv, int *overflow)
{
    /* This version by Tim Peters */
    long long res;
    *overflow = 0;
    ASSURE_PYLONG_OBJECT_BEGIN(v, vv){
        if(_PyLong_AsByteArray(v, (unsigned char*)&res, sizeof(res),
                               PY_LITTLE_ENDIAN, 1) < 0){
            assert(PyErr_Occurred());
            PyErr_Clear();
            res = -1;
            *overflow = (Py_SIZE(v) < 0 ? -1 : 1);
        }
    }ASSURE_PYLONG_OBJECT_END;
    return res;
}

#define CHECK_BINOP(v,w)                                \
    do {                                                \
        if (!PyLong_Check(v) || !PyLong_Check(w))       \
            Py_RETURN_NOTIMPLEMENTED;                   \
    } while(0)


/* Convert an int object to a string, using a given conversion base,
   which should be one of 2, 8, 10 or 16.  Return a string object.
   If base is 2, 8 or 16, add the proper prefix '0b', '0o' or '0x'
   if alternate is nonzero. */

static int
long_format_binary(PyObject *aa, int base, int alternate,
                   PyObject **p_output, _PyUnicodeWriter *writer,
                   _PyBytesWriter *bytes_writer, char **bytes_str)
{
    PyLongObject *a = (PyLongObject *)aa;
    PyObject *v = NULL;
    Py_ssize_t sz;
    enum PyUnicode_Kind kind;
    mpz_t ma;
    char *s = NULL;
    int result = -1;
    static const char base_symbol[] = {0,0,'b',0,0,0,0,0,'o',0,0,0,0,0,0,0,'x'};

    assert(base == 2 || base == 8 || base == 10 || base == 16);
    assert(!((base == 10)&&(alternate != 0)));
    if (a == NULL || !PyLong_Check(a)) {
        PyErr_BadInternalCall();
        goto exit_long_format_binary;
    }

    s = mpz_get_str(NULL, base, mpz_init_set_pylong(ma, a));

    sz = strlen(s);
    if (alternate) {
        /* 2 characters for prefix  */
        sz += 2;
    }

    if (writer) {
        if (_PyUnicodeWriter_Prepare(writer, sz, 'x') == -1)
            goto exit_long_format_binary;
        kind = writer->kind;
    }
    else if (bytes_writer) {
        *bytes_str = _PyBytesWriter_Prepare(bytes_writer, *bytes_str, sz);
        if (*bytes_str == NULL)
            goto exit_long_format_binary;
    }
    else {
        v = PyUnicode_New(sz, 'x');
        if (v == NULL)
            goto exit_long_format_binary;
        kind = PyUnicode_KIND(v);
    }

#define WRITE_DIGITS_X(p)                                               \
    do {                                                                \
        char *c = s;                                                    \
        if(*c == '-')                                                   \
            *p++ = *c++;                                                \
        if (alternate) {                                                \
            *p++ = '0';                                                 \
            *p++ = base_symbol[base];                                   \
        }                                                               \
        while(*c)                                                       \
            *p++ = *c++;                                                \
    } while (0)
#define WRITE_UNICODE_DIGITS_X(TYPE)                                    \
    do {                                                                \
        TYPE *p;                                                        \
        if (writer)                                                     \
            p = (TYPE*)PyUnicode_DATA(writer->buffer) + writer->pos;    \
        else                                                            \
            p = (TYPE*)PyUnicode_DATA(v);                               \
        WRITE_DIGITS_X(p);                                              \
        if (writer)                                                     \
            assert(p == ((TYPE*)PyUnicode_DATA(writer->buffer)          \
                             + writer->pos + sz));                      \
        else                                                            \
            assert(p == ((TYPE*)PyUnicode_DATA(v) + sz));               \
    } while (0)
    if (bytes_writer) {
        char *p = *bytes_str;
        WRITE_DIGITS_X(p);
        assert(p == *bytes_str + sz);
    }
    else if (kind == PyUnicode_1BYTE_KIND) {
        WRITE_UNICODE_DIGITS_X(Py_UCS1);
    }
    else if (kind == PyUnicode_2BYTE_KIND) {
        WRITE_UNICODE_DIGITS_X(Py_UCS2);
    }
    else {
        assert (kind == PyUnicode_4BYTE_KIND);
        WRITE_UNICODE_DIGITS_X(Py_UCS4);
    }
#undef WRITE_DIGITS_X
#undef WRITE_UNICODE_DIGITS_X
    if (writer) {
        writer->pos += sz;
    }
    else if (bytes_writer) {
        (*bytes_str) += sz;
    }
    else {
        assert(_PyUnicode_CheckConsistency(v, 1));
        *p_output = v;
    }
    result = 0;
  exit_long_format_binary:
    mpz_custom_free(s, strlen(s)+1);
    return result;
}

static PyObject *
long_to_decimal_string(PyObject *aa)
{
    PyObject *v;
    if (long_format_binary(aa, 10, 0, &v, NULL, NULL, NULL) == -1)
        return NULL;
    return v;
}

PyObject *
_PyLong_Format(PyObject *obj, int base)
{
    PyObject *str;
    if (long_format_binary(obj, base, (base != 10), &str, NULL,
                           NULL, NULL) == -1)
        return NULL;
    return str;
}

int
_PyLong_FormatWriter(_PyUnicodeWriter *writer,
                     PyObject *obj,
                     int base, int alternate)
{
    if (base == 10)
        alternate = 0;
    return long_format_binary(obj, base, alternate, NULL, writer,
                              NULL, NULL);
}

char*
_PyLong_FormatBytesWriter(_PyBytesWriter *writer, char *str,
                          PyObject *obj,
                          int base, int alternate)
{
    char *str2;
    int res;
    str2 = str;
    if (base == 10)
        alternate = 0;
    res = long_format_binary(obj, base, alternate, NULL, NULL,
                             writer, &str2);
    if (res < 0)
        return NULL;
    assert(str2 != NULL);
    return str2;
}

/* Table of digit values for 8-bit string -> integer conversion.
 * '0' maps to 0, ..., '9' maps to 9.
 * 'a' and 'A' map to 10, ..., 'z' and 'Z' map to 35.
 * All other indices map to 37.
 * Note that when converting a base B string, a char c is a legitimate
 * base B digit iff _PyLong_DigitValue[Py_CHARPyLong_MASK(c)] < B.
 */
unsigned char _PyLong_DigitValue[256] = {
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  37, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
};

/* *str points to the first digit in a string of base `base` digits.  base
 * is a power of 2 (2, 4, 8, 16, or 32).  *str is set to point to the first
 * non-digit (which may be *str!).  A normalized int is returned.
 * The point to this routine is that it takes time linear in the number of
 * string characters.
 */
static PyLongObject *
long_from_binary_base(const char **str, int base, int sign)
{
    const char *p = *str;
    const char *start = p;
    double bits_per_char;
    Py_ssize_t n;
    ssize_t i;
    char *c, *s;
    char sbuf[32];
    ssize_t buflen;
    static double bits_per_char_table[36] = {0,};

    if(bits_per_char_table[2] == 0.0){
        for(i = 2; i < 36; ++i)
            bits_per_char_table[i] = log2((double)i);
    }

    assert(base >= 2 && base <= 36);
    assert(Py_ABS(sign) == 1);
    bits_per_char = bits_per_char_table[base];
    /* n <- total # of bits needed, while setting p to end-of-string */
    while (_PyLong_DigitValue[Py_CHARMASK(*p)] < base)
        ++p;
    *str = p;
    /* n <- # of Python digits needed, = ceiling(n/PyLong_SHIFT). */
    n = (Py_ssize_t)ceil((p - start) * bits_per_char) + (sizeof(digit)*8) - 1;
    if (floor(n / bits_per_char) < p - start) {
        PyErr_SetString(PyExc_ValueError,
                        "int string too large to convert");
        return NULL;
    }

    buflen = (sign == -1 ? 1 : 0) + (p - start) + 1;
    if(buflen > 32)
        s = PyObject_Malloc((sign == -1 ? 1 : 0) + (p - start) + 1);
    else
        s = sbuf;
    if(!s) abort(); /* FIXME */

    c = s;
    if(sign == -1)
        *c++ = '-';
    for(i = 0; i < (p - start); ++i)
        *c++ = start[i];
    *c = '\0';

    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_set_str(zzret, s, base);
        if(s != sbuf) PyObject_Free(s);
    }MPZ_CACHED_RESULT_END;
}

/* Parses an int from a bytestring. Leading and trailing whitespace will be
 * ignored.
 *
 * If successful, a PyLong object will be returned and 'pend' will be pointing
 * to the first unused byte unless it's NULL.
 *
 * If unsuccessful, NULL will be returned.
 */
PyObject *
PyLong_FromString(const char *str, char **pend, int base)
{
    int sign = 1, error_if_nonzero = 0;
    const char *start, *orig_str = str;
    PyLongObject *z = NULL;
    PyObject *strobj;
    Py_ssize_t slen;

    if ((base != 0 && base < 2) || base > 36) {
        PyErr_SetString(PyExc_ValueError,
                        "int() arg 2 must be >= 2 and <= 36");
        return NULL;
    }
    while (*str != '\0' && Py_ISSPACE(Py_CHARMASK(*str))) {
        str++;
    }
    if (*str == '+') {
        ++str;
    }
    else if (*str == '-') {
        ++str;
        sign = -1;
    }
    if (base == 0) {
        if (str[0] != '0') {
            base = 10;
        }
        else if (str[1] == 'x' || str[1] == 'X') {
            base = 16;
        }
        else if (str[1] == 'o' || str[1] == 'O') {
            base = 8;
        }
        else if (str[1] == 'b' || str[1] == 'B') {
            base = 2;
        }
        else {
            /* "old" (C-style) octal literal, now invalid.
               it might still be zero though */
            error_if_nonzero = 1;
            base = 10;
        }
    }
    if (str[0] == '0' &&
        ((base == 16 && (str[1] == 'x' || str[1] == 'X')) ||
         (base == 8  && (str[1] == 'o' || str[1] == 'O')) ||
         (base == 2  && (str[1] == 'b' || str[1] == 'B')))) {
        str += 2;
        /* One underscore allowed here. */
        if (*str == '_') {
            ++str;
        }
    }
    if (str[0] == '_') {
	    /* May not start with underscores. */
	    goto onError;
    }

    start = str;

    z = long_from_binary_base(&str, base, sign);

    if (z == NULL) {
        return NULL;
    }
    if (error_if_nonzero) {
        /* reset the base to 0, else the exception message
           doesn't make too much sense */
        base = 0;
        if (Py_SIZE(z) != 0) {
            goto onError;
        }
        /* there might still be other problems, therefore base
           remains zero here for the same reason */
    }
    if (str == start) {
        goto onError;
    }
    while (*str && Py_ISSPACE(Py_CHARMASK(*str))) {
        str++;
    }
    if (*str != '\0') {
        goto onError;
    }
    if (pend != NULL) {
        *pend = (char *)str;
    }
    return (PyObject *) z;

  onError:
    if (pend != NULL) {
        *pend = (char *)str;
    }
    Py_XDECREF(z);
    slen = strlen(orig_str) < 200 ? strlen(orig_str) : 200;
    strobj = PyUnicode_FromStringAndSize(orig_str, slen);
    if (strobj == NULL) {
        return NULL;
    }
    PyErr_Format(PyExc_ValueError,
                 "invalid literal for int() with base %d: %.200R",
                 base, strobj);
    Py_DECREF(strobj);
    return NULL;
}

/* Since PyLong_FromString doesn't have a length parameter,
 * check here for possible NULs in the string.
 *
 * Reports an invalid literal as a bytes object.
 */
PyObject *
_PyLong_FromBytes(const char *s, Py_ssize_t len, int base)
{
    PyObject *result, *strobj;
    char *end = NULL;

    result = PyLong_FromString(s, &end, base);
    if (end == NULL || (result != NULL && end == s + len))
        return result;
    Py_XDECREF(result);
    strobj = PyBytes_FromStringAndSize(s, Py_MIN(len, 200));
    if (strobj != NULL) {
        PyErr_Format(PyExc_ValueError,
                     "invalid literal for int() with base %d: %.200R",
                     base, strobj);
        Py_DECREF(strobj);
    }
    return NULL;
}

PyObject *
PyLong_FromUnicode(Py_UNICODE *u, Py_ssize_t length, int base)
{
    PyObject *v, *unicode = PyUnicode_FromUnicode(u, length);
    if (unicode == NULL)
        return NULL;
    v = PyLong_FromUnicodeObject(unicode, base);
    Py_DECREF(unicode);
    return v;
}

PyObject *
PyLong_FromUnicodeObject(PyObject *u, int base)
{
    PyObject *result, *asciidig;
    char *buffer, *end = NULL;
    Py_ssize_t buflen;

    asciidig = _PyUnicode_TransformDecimalAndSpaceToASCII(u);
    if (asciidig == NULL)
        return NULL;
    buffer = PyUnicode_AsUTF8AndSize(asciidig, &buflen);
    if (buffer == NULL) {
        Py_DECREF(asciidig);
        if (!PyErr_ExceptionMatches(PyExc_UnicodeEncodeError))
            return NULL;
    }
    else {
        result = PyLong_FromString(buffer, &end, base);
        if (end == NULL || (result != NULL && end == buffer + buflen)) {
            Py_DECREF(asciidig);
            return result;
        }
        Py_DECREF(asciidig);
        Py_XDECREF(result);
    }
    PyErr_Format(PyExc_ValueError,
                 "invalid literal for int() with base %d: %.200R",
                 base, u);
    return NULL;
}

/* forward */
static PyObject *long_long(PyObject *v);

/* For a nonzero PyLong a, express a in the form x * 2**e, with 0.5 <=
   abs(x) < 1.0 and e >= 0; return x and put e in *e.  Here x is
   rounded to DBL_MANT_DIG significant bits using round-half-to-even.
   If a == 0, return 0.0 and set *e = 0.  If the resulting exponent
   e is larger than PY_SSIZE_T_MAX, raise OverflowError and return
   -1.0. */

/* attempt to define 2.0**DBL_MANT_DIG as a compile-time constant */
#if DBL_MANT_DIG == 53
#define EXP2_DBL_MANT_DIG 9007199254740992.0
#else
#define EXP2_DBL_MANT_DIG (ldexp(1.0, DBL_MANT_DIG))
#endif

double
_PyLong_Frexp(PyLongObject *a, Py_ssize_t *e)
{
    Py_ssize_t a_size, a_bits;
    /* See below for why x_digits is always large enough. */
    //digit rem, x_digits[2 + (DBL_MANT_DIG + 1) / PyLong_SHIFT];
    double dx;
    /* Correction term for round-half-to-even rounding.  For a digit x,
       "x + half_even_correction[x & 7]" gives x rounded to the nearest
       multiple of 4, rounding ties to a multiple of 8. */
    static const int half_even_correction[8] = {0, -1, -2, 1, 0, -1, 2, 1};

    a_size = Py_ABS(Py_SIZE(a));
    if (a_size == 0) {
        /* Special case for 0: significand 0.0, exponent 0. */
        *e = 0;
        return 0.0;
    }
    a_bits = _PyLong_NumBits((PyObject*)a);
    if(a_bits > PY_SSIZE_T_MAX)
        goto overflow;

    /* Shift the first DBL_MANT_DIG + 2 bits of a into x_digits[0:x_size]
       (shifting left if a_bits <= DBL_MANT_DIG + 2).

       Number of digits needed for result: write // for floor division.
       Then if shifting left, we end up using

         1 + a_size + (DBL_MANT_DIG + 2 - a_bits) // PyLong_SHIFT

       digits.  If shifting right, we use

         a_size - (a_bits - DBL_MANT_DIG - 2) // PyLong_SHIFT

       digits.  Using a_size = 1 + (a_bits - 1) // PyLong_SHIFT along with
       the inequalities

         m // PyLong_SHIFT + n // PyLong_SHIFT <= (m + n) // PyLong_SHIFT
         m // PyLong_SHIFT - n // PyLong_SHIFT <=
                                          1 + (m - n - 1) // PyLong_SHIFT,

       valid for any integers m and n, we find that x_size satisfies

         x_size <= 2 + (DBL_MANT_DIG + 1) // PyLong_SHIFT

       in both cases.
    */
{
    mpz_t ma, mx;
    int heven;
    ssize_t xshift = (a_bits - DBL_MANT_DIG - 2);
    mpz_init_set_abs_pylong(ma, a);
    mpz_init(mx);
    if (xshift <= 0) {
        mpz_mul_2exp(mx, ma, -xshift);
    } else {
        int rem;
        mpz_fdiv_r_2exp(mx, ma, xshift);
        rem = (mpz_sgn(mx) != 0);
        mpz_fdiv_q_2exp(mx, ma, xshift);
        if(rem) mpz_setbit(mx, 0);
    }
    //assert(1 <= x_size && x_size <= (Py_ssize_t)Py_ARRAY_LENGTH(x_digits));

    /* Round, and convert to double. */
    heven = half_even_correction[mpz_get_ui(mx) & 7];
    if(heven >= 0) {
        mpz_add_ui(mx, mx, heven);
    } else {
        mpz_sub_ui(mx, mx, -heven);
    }
    dx = mpz_get_d(mx);

    mpz_clear(mx);
}

    /* Rescale;  make correction if result is 1.0. */
    dx /= 4.0 * EXP2_DBL_MANT_DIG;
    if (dx == 1.0) {
        if (a_bits == PY_SSIZE_T_MAX)
            goto overflow;
        dx = 0.5;
        a_bits += 1;
    }

    *e = a_bits;
    return Py_SIZE(a) < 0 ? -dx : dx;

  overflow:
    /* exponent > PY_SSIZE_T_MAX */
    PyErr_SetString(PyExc_OverflowError,
                    "huge integer: number of bits overflows a Py_ssize_t");
    *e = 0;
    return -1.0;
}

/* Get a C double from an int object.  Rounds to the nearest double,
   using the round-half-to-even rule in the case of a tie. */

double
PyLong_AsDouble(PyObject *v)
{
    Py_ssize_t exponent;
    double x;

    CONFIRM_PYLONG_OBJECT(v);
    x = _PyLong_Frexp((PyLongObject *)v, &exponent);
    if ((x == -1.0 && PyErr_Occurred()) || exponent > DBL_MAX_EXP) {
        TOO_LARGE("float");
#if 0
        PyErr_SetString(PyExc_OverflowError,
                        "int too large to convert to float");
#endif
        return -1.0;
    }
    return ldexp(x, (int)exponent);
}

/* Methods */

static void
long_dealloc(PyObject *v)
{
    Py_TYPE(v)->tp_free(v);
}

static int
long_compare(PyLongObject *a, PyLongObject *b)
{
    int result;
    mpz_t ma, mb;

    result = mpz_cmp(mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    return result ? (result > 0 ? 1 : -1) : 0;
}

#define TEST_COND(cond) \
    ((cond) ? Py_True : Py_False)

static PyObject *
long_richcompare(PyObject *self, PyObject *other, int op)
{
    int result;
    PyObject *v;
    CHECK_BINOP(self, other);
    if (self == other)
        result = 0;
    else
        result = long_compare((PyLongObject*)self, (PyLongObject*)other);
    /* Convert the return value to a Boolean */
    switch (op) {
    case Py_EQ:
        v = TEST_COND(result == 0);
        break;
    case Py_NE:
        v = TEST_COND(result != 0);
        break;
    case Py_LE:
        v = TEST_COND(result <= 0);
        break;
    case Py_GE:
        v = TEST_COND(result >= 0);
        break;
    case Py_LT:
        v = TEST_COND(result == -1);
        break;
    case Py_GT:
        v = TEST_COND(result == 1);
        break;
    default:
        PyErr_BadArgument();
        return NULL;
    }
    Py_INCREF(v);
    return v;
}

static Py_hash_t
long_hash(PyLongObject *v)
{
    Py_uhash_t x;
    mpz_t zz, mv;
    mpz_init(zz);
    mpz_tdiv_r_ui(zz, mpz_init_set_pylong(mv, v), _PyHASH_MODULUS);
    x = mpz_get_si(zz);
    mpz_clear(zz);
    if (x == (Py_uhash_t)-1)
        x = (Py_uhash_t)-2;
    return (Py_hash_t)x;
}

static PyObject *
long_add(PyLongObject *a, PyLongObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_add(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_sub(PyLongObject *a, PyLongObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_sub(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_mul(PyLongObject *a, PyLongObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_mul(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

/* The / and % operators are now defined in terms of divmod().
   The expression a mod b has the value a - b*floor(a/b).
   The long_divrem function gives the remainder after division of
   |a| by |b|, with the sign of a.  This is also expressed
   as a - b*trunc(a/b), if trunc truncates towards zero.
   Some examples:
     a           b      a rem b         a mod b
     13          10      3               3
    -13          10     -3               7
     13         -10      3              -7
    -13         -10     -3              -3
   So, to get from rem to mod, we have to add b if a and b
   have different signs.  We then subtract one from the 'div'
   part of the outcome to keep the invariant intact. */

/* Compute
 *     *pdiv, *pmod = divmod(v, w)
 * NULL can be passed for pdiv or pmod, in which case that part of
 * the result is simply thrown away.  The caller owns a reference to
 * each of these it requests (does not pass NULL for).
 */
static int
l_divmod(PyLongObject *v, PyLongObject *w,
         PyLongObject **pdiv, PyLongObject **pmod)
{
    mpz_t mv, mw, mdiv, mmod;
    int mdiv_reused=0, mmod_reused=0;

    if(pdiv) *pdiv = NULL;
    if(pmod) *pmod = NULL;

    if(Py_SIZE(w) == 0) {
        PyErr_SetString(PyExc_ZeroDivisionError,
                        "integer division or modulo by zero");
        return -1;
    }

    mpz_init(mdiv);
    mpz_init(mmod);
    mpz_fdiv_qr(mdiv, mmod, mpz_init_set_pylong(mv, v), mpz_init_set_pylong(mw, w));

    if(pdiv) *pdiv = (PyLongObject*)mpz_eval_as_pylong(mdiv, &mdiv_reused);
    if(pmod) *pmod = (PyLongObject*)mpz_eval_as_pylong(mmod, &mmod_reused);
    if(!mmod_reused) mpz_clear(mmod);
    if(!mdiv_reused) mpz_clear(mdiv);
    return 0;
}

static PyObject *
long_div(PyObject *a, PyObject *b)
{
    PyLongObject *div;

    CHECK_BINOP(a, b);
    if (l_divmod((PyLongObject*)a, (PyLongObject*)b, &div, NULL) < 0)
        div = NULL;
    return (PyObject *)div;
}

/* PyLong/PyLong -> float, with correctly rounded result. */

static PyObject *
long_true_divide(PyObject *v, PyObject *w)
{
    PyLongObject *a, *b;//, *x;
    Py_ssize_t a_size, b_size, shift, diff, /*x_size,*/ x_bits;
    int inexact = 0, negate;
    double dx, result;

    CHECK_BINOP(v, w);
    a = (PyLongObject *)v;
    b = (PyLongObject *)w;

    /*
       Method in a nutshell:

         0. reduce to case a, b > 0; filter out obvious underflow/overflow
         1. choose a suitable integer 'shift'
         2. use integer arithmetic to compute x = floor(2**-shift*a/b)
         3. adjust x for correct rounding
         4. convert x to a double dx with the same value
         5. return ldexp(dx, shift).

       In more detail:

       0. For any a, a/0 raises ZeroDivisionError; for nonzero b, 0/b
       returns either 0.0 or -0.0, depending on the sign of b.  For a and
       b both nonzero, ignore signs of a and b, and add the sign back in
       at the end.  Now write a_bits and b_bits for the bit lengths of a
       and b respectively (that is, a_bits = 1 + floor(log_2(a)); likewise
       for b).  Then

          2**(a_bits - b_bits - 1) < a/b < 2**(a_bits - b_bits + 1).

       So if a_bits - b_bits > DBL_MAX_EXP then a/b > 2**DBL_MAX_EXP and
       so overflows.  Similarly, if a_bits - b_bits < DBL_MIN_EXP -
       DBL_MANT_DIG - 1 then a/b underflows to 0.  With these cases out of
       the way, we can assume that

          DBL_MIN_EXP - DBL_MANT_DIG - 1 <= a_bits - b_bits <= DBL_MAX_EXP.

       1. The integer 'shift' is chosen so that x has the right number of
       bits for a double, plus two or three extra bits that will be used
       in the rounding decisions.  Writing a_bits and b_bits for the
       number of significant bits in a and b respectively, a
       straightforward formula for shift is:

          shift = a_bits - b_bits - DBL_MANT_DIG - 2

       This is fine in the usual case, but if a/b is smaller than the
       smallest normal float then it can lead to double rounding on an
       IEEE 754 platform, giving incorrectly rounded results.  So we
       adjust the formula slightly.  The actual formula used is:

           shift = MAX(a_bits - b_bits, DBL_MIN_EXP) - DBL_MANT_DIG - 2

       2. The quantity x is computed by first shifting a (left -shift bits
       if shift <= 0, right shift bits if shift > 0) and then dividing by
       b.  For both the shift and the division, we keep track of whether
       the result is inexact, in a flag 'inexact'; this information is
       needed at the rounding stage.

       With the choice of shift above, together with our assumption that
       a_bits - b_bits >= DBL_MIN_EXP - DBL_MANT_DIG - 1, it follows
       that x >= 1.

       3. Now x * 2**shift <= a/b < (x+1) * 2**shift.  We want to replace
       this with an exactly representable float of the form

          round(x/2**extra_bits) * 2**(extra_bits+shift).

       For float representability, we need x/2**extra_bits <
       2**DBL_MANT_DIG and extra_bits + shift >= DBL_MIN_EXP -
       DBL_MANT_DIG.  This translates to the condition:

          extra_bits >= MAX(x_bits, DBL_MIN_EXP - shift) - DBL_MANT_DIG

       To round, we just modify the bottom digit of x in-place; this can
       end up giving a digit with value > PyLONG_MASK, but that's not a
       problem since digits can hold values up to 2*PyLONG_MASK+1.

       With the original choices for shift above, extra_bits will always
       be 2 or 3.  Then rounding under the round-half-to-even rule, we
       round up iff the most significant of the extra bits is 1, and
       either: (a) the computation of x in step 2 had an inexact result,
       or (b) at least one other of the extra bits is 1, or (c) the least
       significant bit of x (above those to be rounded) is 1.

       4. Conversion to a double is straightforward; all floating-point
       operations involved in the conversion are exact, so there's no
       danger of rounding errors.

       5. Use ldexp(x, shift) to compute x*2**shift, the final result.
       The result will always be exactly representable as a double, except
       in the case that it overflows.  To avoid dependence on the exact
       behaviour of ldexp on overflow, we check for overflow before
       applying ldexp.  The result of ldexp is adjusted for sign before
       returning.
    */

    /* Reduce to case where a and b are both positive. */
    a_size = Py_ABS(Py_SIZE(a));
    b_size = Py_ABS(Py_SIZE(b));
    negate = (Py_SIZE(a) < 0) ^ (Py_SIZE(b) < 0);
    if (b_size == 0) {
        PyErr_SetString(PyExc_ZeroDivisionError,
                        "division by zero");
        goto error;
    }
    if (a_size == 0)
        goto underflow_or_zero;

{
    int a_is_small, b_is_small;
    /* Fast path for a and b small (exactly representable in a double).
       Relies on floating-point division being correctly rounded; results
       may be subject to double rounding on x86 machines that operate with
       the x87 FPU set to 64-bit precision. */
    a_is_small = _PyLong_NumBits((PyObject*)a) <= DBL_MANT_DIG;
    b_is_small = _PyLong_NumBits((PyObject*)b) <= DBL_MANT_DIG;
    if (a_is_small && b_is_small) {
        mpz_t za, zb;
        result = mpz_get_d(mpz_init_set_pylong(za, a)) 
                     / mpz_get_d(mpz_init_set_pylong(zb, b));
        return PyFloat_FromDouble(result);
    }
}
    diff = _PyLong_NumBits((PyObject*)a) - _PyLong_NumBits((PyObject*)b);
    /* Now diff = a_bits - b_bits. */
    if (diff > DBL_MAX_EXP)
        goto overflow;
    else if (diff < DBL_MIN_EXP - DBL_MANT_DIG - 1)
        goto underflow_or_zero;

    /* Choose value for shift; see comments for step 1 above. */
    shift = Py_MAX(diff, DBL_MIN_EXP) - DBL_MANT_DIG - 2;

{
    mpz_t ma, mb, mx, mrem;
    Py_ssize_t extra_bits;
    digit mask, low, *uu;
    size_t uusize = 0;

    mpz_init_set_abs_pylong(ma, a);
    mpz_init_set_abs_pylong(mb, b);
    mpz_init(mx);
    mpz_init(mrem);

    diff = mpz_sizeinbase(ma, 2) - mpz_sizeinbase(mb, 2);
    shift = Py_MAX(diff, DBL_MIN_EXP) - DBL_MANT_DIG - 2;

    /* x = abs(a * 2**-shift) */
    if (shift <= 0) {
        if (a_size >= PY_SSIZE_T_MAX - 1 - (-shift / (sizeof(digit)*8))) {
            /* In practice, it's probably impossible to end up
               here.  Both a and b would have to be enormous,
               using close to SIZE_T_MAX bytes of memory each. */
            PyErr_SetString(PyExc_OverflowError,
                            "intermediate overflow during division");
            /* TODO: reconstruct where mx and ma are cleared? */
            mpz_clear(mrem);
            mpz_clear(mx);
            //mpz_clear(ma);
            goto error;
        }
        mpz_mul_2exp(mx, ma, -shift);
    }
    else {
        mpz_fdiv_r_2exp(mrem, ma, shift);
        if(mpz_sgn(mrem) != 0) inexact = 1;
        mpz_fdiv_q_2exp(mx, ma, shift);
    }

    /* x //= b. If the remainder is nonzero, set inexact.  We own the only
       reference to x, so it's safe to modify it in-place. */
    mpz_fdiv_qr(mx, mrem, mx, mb);
    if(mpz_sgn(mrem) != 0) inexact = 1;

    x_bits = mpz_sizeinbase(mx, 2);

    /* The number of extra bits that have to be rounded away. */
    extra_bits = Py_MAX(x_bits, DBL_MIN_EXP - shift) - DBL_MANT_DIG;
    assert(extra_bits == 2 || extra_bits == 3);

    uu = mpz_export(NULL, &uusize, -1, sizeof(digit), 0, 0, mx);
    assert(uu);
    assert(uusize > 0); /* result of division is never zero */

    /* Round by directly modifying the low digit of x. */
    mask = (digit)1 << (extra_bits - 1);
    low = uu[0] | inexact;
    if ((low & mask) && (low & (3U*mask-1U)))
        low += mask;
    uu[0] = low & ~(2U*mask-1U);

    /* Convert x to a double dx; the conversion is exact. */
    dx = uu[--uusize];
    while (uusize > 0)
        dx = ldexp(dx, (sizeof(digit)*8)) + uu[--uusize];

    mpz_custom_free(uu, uusize*sizeof(digit));

    mpz_clear(mrem);
    mpz_clear(mx);
}

    /* Check whether ldexp result will overflow a double. */
    if (shift + x_bits >= DBL_MAX_EXP &&
        (shift + x_bits > DBL_MAX_EXP || dx == ldexp(1.0, (int)x_bits)))
        goto overflow;
    result = ldexp(dx, (int)shift);
    return PyFloat_FromDouble(negate ? -result : result);

  underflow_or_zero:
    return PyFloat_FromDouble(negate ? -0.0 : 0.0);

  overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "integer division result too large for a float");
  error:
    return NULL;
}

static PyObject *
long_mod(PyObject *a, PyObject *b)
{
    PyLongObject *mod;

    CHECK_BINOP(a, b);

    if (l_divmod((PyLongObject*)a, (PyLongObject*)b, NULL, &mod) < 0)
        mod = NULL;
    return (PyObject *)mod;
}

static PyObject *
long_divmod(PyObject *a, PyObject *b)
{
    PyLongObject *div, *mod;
    PyObject *z;

    CHECK_BINOP(a, b);

    if (l_divmod((PyLongObject*)a, (PyLongObject*)b, &div, &mod) < 0) {
        return NULL;
    }
    z = PyTuple_New(2);
    if (z != NULL) {
        PyTuple_SetItem(z, 0, (PyObject *) div);
        PyTuple_SetItem(z, 1, (PyObject *) mod);
    }
    else {
        Py_DECREF(div);
        Py_DECREF(mod);
    }
    return z;
}

/* pow(v, w, x) */
static PyObject *
long_pow(PyObject *v, PyObject *w, PyObject *x)
{
    PyLongObject *a, *b, *c; /* a,b,c = v,w,x */

    CHECK_BINOP(v, w);
    a = (PyLongObject*)v;
    b = (PyLongObject*)w;
    if (PyLong_Check(x)) {
        c = (PyLongObject *)x;
    }
    else if (x == Py_None)
        c = NULL;
    else {
        Py_RETURN_NOTIMPLEMENTED;
    }

    if (Py_SIZE(b) < 0) {  /* if exponent is negative */
        if (c) {
            PyErr_SetString(PyExc_ValueError, "pow() 2nd argument "
                            "cannot be negative when 3rd argument specified");
            return NULL;
        }
        else {
            /* else return a float.  This works because we know
               that this calls float_pow() which converts its
               arguments to double. */
            return PyFloat_Type.tp_as_number->nb_power(v, w, x);
        }
    }

    if (c) {
        mpz_t ma, mb, mc, mc2, mz;
        int mz_reused=0;
        int negativeOutput = 0;  /* if x<0 return negative output */
        PyObject *z = NULL;
        mpz_init_set(mc, mpz_init_set_pylong(mc2, c)); //gmp_int_from_pylong(c, &mc);
        mpz_init(mz);
        /* if modulus == 0:
               raise ValueError() */
        if (mpz_sgn(mc) == 0) {
            PyErr_SetString(PyExc_ValueError,
                            "pow() 3rd argument cannot be 0");
            goto exit_long_pow_3args;
        }
        if (mpz_cmpabs_ui(mc, 1) == 0){
            z = PyLong_FromLong(0L);
            goto exit_long_pow_3args;
        }
        if (mpz_sgn(mc) < 0) {
            negativeOutput = 1;
            mpz_neg(mc, mc);
        }
        mpz_powm(mz, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b), mc);
        if(negativeOutput && (mpz_sgn(mz) != 0)){
            mpz_sub(mz, mz, mc);
        }
        z = mpz_eval_as_pylong(mz, &mz_reused);
      exit_long_pow_3args:
        if(!mz_reused) mpz_clear(mz);
        mpz_clear(mc);
        return z;
    }else{
        unsigned long xb;
        xb = PyLong_AsUnsignedLong((PyObject*)b);
        if((xb == (unsigned long)-1) && PyErr_Occurred()){
            return NULL;
        }
        MPZ_CACHED_RESULT_BEGIN(zzret){
            mpz_t ma;
            mpz_pow_ui (zzret, mpz_init_set_pylong(ma, a), xb);
        }MPZ_CACHED_RESULT_END;
    }
}

static PyObject *
long_invert(PyLongObject *v)
{
    mpz_t msrc, mdst;
    mpz_init_set_pylong(msrc, v);
    if(mpz_fits_sshort_p(msrc)){
        long ival = mpz_get_si(msrc);
        CHECK_SMALL_INT(~ival);
    }
    mpz_init(mdst);
    mpz_com(mdst, msrc);
    return mpz_reuse_as_pylong(mdst);
#if 0
    /* Implement ~x as -(x+1) */
    if (Py_SIZE(v) == 0)
        return PyLong_FromLong(-1);
    if (Py_ABS(Py_SIZE(v)) <=1 && v->ob_digit[0] < 0x1000000UL)
        return PyLong_FromLong(-(PyLong_AsLong((PyObject*)v)+1));
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t mv;
        mpz_com(zzret, mpz_init_set_pylong(mv, v));
    }MPZ_CACHED_RESULT_END;
#endif
}

static PyObject *
long_neg(PyLongObject *v)
{
    mpz_t msrc, mdst;
    mpz_init_set_pylong(msrc, v);
    if(mpz_fits_sshort_p(msrc)){
        long ival = mpz_get_si(msrc);
        CHECK_SMALL_INT(-ival);
    }
    mpz_init(mdst);
    mpz_neg(mdst, msrc);
    return mpz_reuse_as_pylong(mdst);
}

static PyObject *
long_abs(PyLongObject *v)
{
    if (Py_SIZE(v) < 0)
        return long_neg(v);
    else
        return long_long((PyObject *)v);
}

static int
long_bool(PyLongObject *v)
{
    return Py_SIZE(v) != 0;
}

static PyObject *
long_rshift(PyLongObject *a, PyLongObject *b)
{
    Py_ssize_t shiftby;
    CHECK_BINOP(a, b);

    shiftby = PyLong_AsSsize_t((PyObject *)b);
    if (shiftby == -1L && PyErr_Occurred())
        return NULL;
    if (shiftby < 0) {
        PyErr_SetString(PyExc_ValueError, "negative shift count");
        return NULL;
    }

    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma;
        mpz_fdiv_q_2exp(zzret, mpz_init_set_pylong(ma, a), shiftby); // cdiv, fdiv or tdiv?
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_lshift(PyObject *v, PyObject *w)
{
    Py_ssize_t shiftby;
    CHECK_BINOP(v, w);

    shiftby = PyLong_AsSsize_t(w);
    if (shiftby == -1L && PyErr_Occurred())
        return NULL;
    if (shiftby < 0) {
        PyErr_SetString(PyExc_ValueError, "negative shift count");
        return NULL;
    }

    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma;
        mpz_mul_2exp(zzret, mpz_init_set_pylong(ma, v), shiftby);
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_and(PyObject *a, PyObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_and(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_xor(PyObject *a, PyObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_xor(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_or(PyObject *a, PyObject *b)
{
    CHECK_BINOP(a, b);
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_ior(zzret, mpz_init_set_pylong(ma, a), mpz_init_set_pylong(mb, b));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_long(PyObject *v)
{
    if (PyLong_CheckExact(v))
        Py_INCREF(v);
    else
        v = _PyLong_Copy((PyLongObject *)v);
    return v;
}

PyObject *
_PyLong_GCD(PyObject *aarg, PyObject *barg)
{
    MPZ_CACHED_RESULT_BEGIN(zzret){
        mpz_t ma, mb;
        mpz_gcd(zzret, mpz_init_set_pylong(ma, aarg), mpz_init_set_pylong(mb, barg));
    }MPZ_CACHED_RESULT_END;
}

static PyObject *
long_float(PyObject *v)
{
    double result;
    result = PyLong_AsDouble(v);
    if (result == -1.0 && PyErr_Occurred())
        return NULL;
    return PyFloat_FromDouble(result);
}

static PyObject *
long_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
long_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *obase = NULL, *x = NULL;
    Py_ssize_t base;
    static char *kwlist[] = {"x", "base", 0};

    if (type != &PyLong_Type)
        return long_subtype_new(type, args, kwds); /* Wimp out */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OO:int", kwlist,
                                     &x, &obase))
        return NULL;
    if (x == NULL) {
        if (obase != NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "int() missing string argument");
            return NULL;
        }
        return PyLong_FromLong(0L);
    }
    if (obase == NULL)
        return PyNumber_Long(x);

    base = PyNumber_AsSsize_t(obase, NULL);
    if (base == -1 && PyErr_Occurred())
        return NULL;
    if ((base != 0 && base < 2) || base > 36) {
        PyErr_SetString(PyExc_ValueError,
                        "int() base must be >= 2 and <= 36");
        return NULL;
    }

    if (PyUnicode_Check(x))
        return PyLong_FromUnicodeObject(x, (int)base);
    else if (PyByteArray_Check(x) || PyBytes_Check(x)) {
        char *string;
        if (PyByteArray_Check(x))
            string = PyByteArray_AS_STRING(x);
        else
            string = PyBytes_AS_STRING(x);
        return _PyLong_FromBytes(string, Py_SIZE(x), (int)base);
    }
    else {
        PyErr_SetString(PyExc_TypeError,
                        "int() can't convert non-string with explicit base");
        return NULL;
    }
}

/* Wimpy, slow approach to tp_new calls for subtypes of int:
   first create a regular int from whatever arguments we got,
   then allocate a subtype instance and initialize it from
   the regular int.  The regular int is then thrown away.
*/
static PyObject *
long_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyLongObject *tmp, *newobj;
    Py_ssize_t n;

    assert(PyType_IsSubtype(type, &PyLong_Type));
    tmp = (PyLongObject *)long_new(&PyLong_Type, args, kwds);
    if (tmp == NULL)
        return NULL;
    assert(PyLong_Check(tmp));
    n = Py_SIZE(tmp);
    if (n < 0)
        n = -n;
    newobj = (PyLongObject *)type->tp_alloc(type, n);
    if (newobj == NULL) {
        Py_DECREF(tmp);
        return NULL;
    }
    assert(PyLong_Check(newobj));
    COPY_DIGITS(newobj, tmp);
    Py_DECREF(tmp);
    return (PyObject *)newobj;
}

static PyObject *
long_getnewargs(PyLongObject *v)
{
    return Py_BuildValue("(N)", _PyLong_Copy(v));
}

static PyObject *
long_get0(PyLongObject *v, void *context) {
    return PyLong_FromLong(0L);
}

static PyObject *
long_get1(PyLongObject *v, void *context) {
    return PyLong_FromLong(1L);
}

static PyObject *
long__format__(PyObject *self, PyObject *args)
{
    PyObject *format_spec;
    _PyUnicodeWriter writer;
    int ret;

    if (!PyArg_ParseTuple(args, "U:__format__", &format_spec))
        return NULL;

    _PyUnicodeWriter_Init(&writer);
    ret = _PyLong_FormatAdvancedWriter(
        &writer,
        self,
        format_spec, 0, PyUnicode_GET_LENGTH(format_spec));
    if (ret == -1) {
        _PyUnicodeWriter_Dealloc(&writer);
        return NULL;
    }
    return _PyUnicodeWriter_Finish(&writer);
}

/* Return a pair (q, r) such that a = b * q + r, and
   abs(r) <= abs(b)/2, with equality possible only if q is even.
   In other words, q == a / b, rounded to the nearest integer using
   round-half-to-even. */

PyObject *
_PyLong_DivmodNear(PyObject *a, PyObject *b)
{
    PyObject *quo = NULL, *rem = NULL;
    PyObject *result = NULL;
    int cmp, quo_is_odd, quo_is_neg;
    mpz_t ma, mb, mquo, mrem, mtwice_rem;
    int mquo_reused = 0, mrem_reused = 0;

    /* Equivalent Python code:

       def divmod_near(a, b):
           q, r = divmod(a, b)
           # round up if either r / b > 0.5, or r / b == 0.5 and q is odd.
           # The expression r / b > 0.5 is equivalent to 2 * r > b if b is
           # positive, 2 * r < b if b negative.
           greater_than_half = 2*r > b if b > 0 else 2*r < b
           exactly_half = 2*r == b
           if greater_than_half or exactly_half and q % 2 == 1:
               q += 1
               r -= b
           return q, r

    */
    if (!PyLong_Check(a) || !PyLong_Check(b)) {
        PyErr_SetString(PyExc_TypeError,
                        "non-integer arguments in division");
        return NULL;
    }

    mpz_init_set_pylong(ma, a);
    mpz_init_set_pylong(mb, b);
    mpz_init(mquo);
    mpz_init(mrem);
    mpz_init(mtwice_rem);

    /* Do a and b have different signs?  If so, quotient is negative. */
    quo_is_neg = (mpz_sgn(ma) < 0) != (mpz_sgn(mb) < 0);

    if(mpz_sgn(mb) == 0){
        PyErr_SetString(PyExc_ZeroDivisionError,
                        "integer division or modulo by zero");
        goto exit_divmodnear;
    }

    mpz_tdiv_qr(mquo, mrem, ma, mb);
    mpz_mul_2exp(mtwice_rem, mrem, 1);
    if (quo_is_neg) {
        mpz_neg(mtwice_rem, mtwice_rem);
    }
    cmp = mpz_cmp(mtwice_rem, mb);
    quo_is_odd = !mpz_divisible_2exp_p(mquo, 1);

    /* compare twice the remainder with the divisor, to see
       if we need to adjust the quotient and remainder */
    if ((Py_SIZE(b) < 0 ? cmp < 0 : cmp > 0) || (cmp == 0 && quo_is_odd)) {
        if (quo_is_neg)
            mpz_sub_ui(mquo, mquo, 1);
        else
            mpz_add_ui(mquo, mquo, 1);
        if (quo_is_neg)
            mpz_add(mrem, mrem, mb);
        else
            mpz_sub(mrem, mrem, mb);
    }

    result = PyTuple_New(2);
    if (result == NULL){
        goto exit_divmodnear;
    }

    /* PyTuple_SET_ITEM steals references */
    quo = mpz_eval_as_pylong(mquo, &mquo_reused);
    rem = mpz_eval_as_pylong(mrem, &mrem_reused);
    PyTuple_SET_ITEM(result, 0, quo);
    PyTuple_SET_ITEM(result, 1, rem);

  exit_divmodnear:
    mpz_clear(mtwice_rem);
    if(!mrem_reused) mpz_clear(mrem);
    if(!mquo_reused) mpz_clear(mquo);
    return result;
}

static PyObject *
long_round(PyObject *self, PyObject *args)
{
    PyObject *o_ndigits=NULL, *temp, *result, *ndigits;

    /* To round an integer m to the nearest 10**n (n positive), we make use of
     * the divmod_near operation, defined by:
     *
     *   divmod_near(a, b) = (q, r)
     *
     * where q is the nearest integer to the quotient a / b (the
     * nearest even integer in the case of a tie) and r == a - q * b.
     * Hence q * b = a - r is the nearest multiple of b to a,
     * preferring even multiples in the case of a tie.
     *
     * So the nearest multiple of 10**n to m is:
     *
     *   m - divmod_near(m, 10**n)[1].
     */
    if (!PyArg_ParseTuple(args, "|O", &o_ndigits))
        return NULL;
    if (o_ndigits == NULL)
        return long_long(self);

    ndigits = PyNumber_Index(o_ndigits);
    if (ndigits == NULL)
        return NULL;

    /* if ndigits >= 0 then no rounding is necessary; return self unchanged */
    if (Py_SIZE(ndigits) >= 0) {
        Py_DECREF(ndigits);
        return long_long(self);
    }

    /* result = self - divmod_near(self, 10 ** -ndigits)[1] */
    temp = long_neg((PyLongObject*)ndigits);
    Py_DECREF(ndigits);
    ndigits = temp;
    if (ndigits == NULL)
        return NULL;

    result = PyLong_FromLong(10L);
    if (result == NULL) {
        Py_DECREF(ndigits);
        return NULL;
    }

    temp = long_pow(result, ndigits, Py_None);
    Py_DECREF(ndigits);
    Py_DECREF(result);
    result = temp;
    if (result == NULL)
        return NULL;

    temp = _PyLong_DivmodNear(self, result);
    Py_DECREF(result);
    result = temp;
    if (result == NULL)
        return NULL;

    temp = long_sub((PyLongObject *)self,
                    (PyLongObject *)PyTuple_GET_ITEM(result, 1));
    Py_DECREF(result);
    result = temp;

    return result;
}

static PyObject *
long_sizeof(PyLongObject *v)
{
    Py_ssize_t res;

    res = PYLONG_HEADSIZE + Py_ABS(Py_SIZE(v))*sizeof(digit);
    return PyLong_FromSsize_t(res);
}

static PyObject *
long_bit_length(PyLongObject *v)
{
    return PyLong_FromSize_t(_PyLong_NumBits((PyObject*)v));
}

PyDoc_STRVAR(long_bit_length_doc,
"int.bit_length() -> int\n\
\n\
Number of bits necessary to represent self in binary.\n\
>>> bin(37)\n\
'0b100101'\n\
>>> (37).bit_length()\n\
6");

#if 0
static PyObject *
long_is_finite(PyObject *v)
{
    Py_RETURN_TRUE;
}
#endif


static PyObject *
long_to_bytes(PyLongObject *v, PyObject *args, PyObject *kwds)
{
    PyObject *byteorder_str;
    PyObject *is_signed_obj = NULL;
    Py_ssize_t length;
    int little_endian;
    int is_signed;
    PyObject *bytes;
    static char *kwlist[] = {"length", "byteorder", "signed", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "nU|O:to_bytes", kwlist,
                                     &length, &byteorder_str,
                                     &is_signed_obj))
        return NULL;

    if (args != NULL && Py_SIZE(args) > 2) {
        PyErr_SetString(PyExc_TypeError,
            "'signed' is a keyword-only argument");
        return NULL;
    }

    if (_PyUnicode_EqualToASCIIString(byteorder_str, "little"))
        little_endian = 1;
    else if (_PyUnicode_EqualToASCIIString(byteorder_str, "big"))
        little_endian = 0;
    else {
        PyErr_SetString(PyExc_ValueError,
            "byteorder must be either 'little' or 'big'");
        return NULL;
    }

    if (is_signed_obj != NULL) {
        int cmp = PyObject_IsTrue(is_signed_obj);
        if (cmp < 0)
            return NULL;
        is_signed = cmp ? 1 : 0;
    }
    else {
        /* If the signed argument was omitted, use False as the
           default. */
        is_signed = 0;
    }

    if (length < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "length argument must be non-negative");
        return NULL;
    }

    bytes = PyBytes_FromStringAndSize(NULL, length);
    if (bytes == NULL)
        return NULL;

    if (_PyLong_AsByteArray(v, (unsigned char *)PyBytes_AS_STRING(bytes),
                            length, little_endian, is_signed) < 0) {
        Py_DECREF(bytes);
        return NULL;
    }

    return bytes;
}

PyDoc_STRVAR(long_to_bytes_doc,
"int.to_bytes(length, byteorder, *, signed=False) -> bytes\n\
\n\
Return an array of bytes representing an integer.\n\
\n\
The integer is represented using length bytes.  An OverflowError is\n\
raised if the integer is not representable with the given number of\n\
bytes.\n\
\n\
The byteorder argument determines the byte order used to represent the\n\
integer.  If byteorder is 'big', the most significant byte is at the\n\
beginning of the byte array.  If byteorder is 'little', the most\n\
significant byte is at the end of the byte array.  To request the native\n\
byte order of the host system, use `sys.byteorder' as the byte order value.\n\
\n\
The signed keyword-only argument determines whether two's complement is\n\
used to represent the integer.  If signed is False and a negative integer\n\
is given, an OverflowError is raised.");

static PyObject *
long_from_bytes(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *byteorder_str;
    PyObject *is_signed_obj = NULL;
    int little_endian;
    int is_signed;
    PyObject *obj;
    PyObject *bytes;
    PyObject *long_obj;
    static char *kwlist[] = {"bytes", "byteorder", "signed", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OU|O:from_bytes", kwlist,
                                     &obj, &byteorder_str,
                                     &is_signed_obj))
        return NULL;

    if (args != NULL && Py_SIZE(args) > 2) {
        PyErr_SetString(PyExc_TypeError,
            "'signed' is a keyword-only argument");
        return NULL;
    }

    if (_PyUnicode_EqualToASCIIString(byteorder_str, "little"))
        little_endian = 1;
    else if (_PyUnicode_EqualToASCIIString(byteorder_str, "big"))
        little_endian = 0;
    else {
        PyErr_SetString(PyExc_ValueError,
            "byteorder must be either 'little' or 'big'");
        return NULL;
    }

    if (is_signed_obj != NULL) {
        int cmp = PyObject_IsTrue(is_signed_obj);
        if (cmp < 0)
            return NULL;
        is_signed = cmp ? 1 : 0;
    }
    else {
        /* If the signed argument was omitted, use False as the
           default. */
        is_signed = 0;
    }

    bytes = PyObject_Bytes(obj);
    if (bytes == NULL)
        return NULL;

    long_obj = _PyLong_FromByteArray(
        (unsigned char *)PyBytes_AS_STRING(bytes), Py_SIZE(bytes),
        little_endian, is_signed);
    Py_DECREF(bytes);

    /* If from_bytes() was used on subclass, allocate new subclass
     * instance, initialize it with decoded int value and return it.
     */
    if (type != &PyLong_Type && PyType_IsSubtype(type, &PyLong_Type)) {
        PyObject *newobj = type->tp_alloc(type, Py_ABS(Py_SIZE(long_obj)));
        if (newobj != NULL) {
            assert(PyLong_Check(newobj));
            COPY_DIGITS(newobj, long_obj);
        }
        Py_DECREF(long_obj);
        return newobj;
    }

    return long_obj;
}

PyDoc_STRVAR(long_from_bytes_doc,
"int.from_bytes(bytes, byteorder, *, signed=False) -> int\n\
\n\
Return the integer represented by the given array of bytes.\n\
\n\
The bytes argument must be a bytes-like object (e.g. bytes or bytearray).\n\
\n\
The byteorder argument determines the byte order used to represent the\n\
integer.  If byteorder is 'big', the most significant byte is at the\n\
beginning of the byte array.  If byteorder is 'little', the most\n\
significant byte is at the end of the byte array.  To request the native\n\
byte order of the host system, use `sys.byteorder' as the byte order value.\n\
\n\
The signed keyword-only argument indicates whether two's complement is\n\
used to represent the integer.");


void *
_PyLong_Export(ssize_t *countp, size_t size, size_t nails, PyLongObject *ob)
{
    /* this function is used in
     * - Modules/_decimal/_decimal.c
     * - Python/marshal.c
     */
    /* this function allocates and returns memory with PyObject_Malloc().
     * the caller has to free the memory with PyObject_Free().
     */
    mpz_t mob;
    void *rop;
    size_t x_bits;

    mpz_init_set_pylong(mob, ob);
    x_bits = mpz_sizeinbase(mob, 2);
    assert(x_bits > 0);
    *countp = (x_bits-1)/(size*8-nails)+1;
    rop = PyObject_Malloc(size * *countp);
    if(!rop) abort(); /* FIXME */
    mpz_export(rop, (size_t*)countp, -1, size, 0, nails, mob);
    if(mpz_sgn(mob) < 0) *countp *= -1;
    return rop;
}

PyLongObject*
_PyLong_Import(ssize_t count, size_t size, size_t nails, const void *op)
{
    /* this function is used in
     * - Modules/_decimal/_decimal.c
     * - Python/marshal.c
     */
    MPZ_CACHED_RESULT_BEGIN(rop){
        mpz_import(rop, Py_ABS(count), -1, size, 0, nails, op);
        if(count < 0) mpz_neg(rop, rop);
    }MPZ_CACHED_RESULT_END;
}

static PyMethodDef long_methods[] = {
    {"conjugate",       (PyCFunction)long_long, METH_NOARGS,
     "Returns self, the complex conjugate of any int."},
    {"bit_length",      (PyCFunction)long_bit_length, METH_NOARGS,
     long_bit_length_doc},
#if 0
    {"is_finite",       (PyCFunction)long_is_finite,    METH_NOARGS,
     "Returns always True."},
#endif
    {"to_bytes",        (PyCFunction)long_to_bytes,
     METH_VARARGS|METH_KEYWORDS, long_to_bytes_doc},
    {"from_bytes",      (PyCFunction)long_from_bytes,
     METH_VARARGS|METH_KEYWORDS|METH_CLASS, long_from_bytes_doc},
    {"__trunc__",       (PyCFunction)long_long, METH_NOARGS,
     "Truncating an Integral returns itself."},
    {"__floor__",       (PyCFunction)long_long, METH_NOARGS,
     "Flooring an Integral returns itself."},
    {"__ceil__",        (PyCFunction)long_long, METH_NOARGS,
     "Ceiling of an Integral returns itself."},
    {"__round__",       (PyCFunction)long_round, METH_VARARGS,
     "Rounding an Integral returns itself.\n"
     "Rounding with an ndigits argument also returns an integer."},
    {"__getnewargs__",          (PyCFunction)long_getnewargs,   METH_NOARGS},
    {"__format__", (PyCFunction)long__format__, METH_VARARGS},
    {"__sizeof__",      (PyCFunction)long_sizeof, METH_NOARGS,
     "Returns size in memory, in bytes"},
    {NULL,              NULL}           /* sentinel */
};

static PyGetSetDef long_getset[] = {
    {"real",
     (getter)long_long, (setter)NULL,
     "the real part of a complex number",
     NULL},
    {"imag",
     (getter)long_get0, (setter)NULL,
     "the imaginary part of a complex number",
     NULL},
    {"numerator",
     (getter)long_long, (setter)NULL,
     "the numerator of a rational number in lowest terms",
     NULL},
    {"denominator",
     (getter)long_get1, (setter)NULL,
     "the denominator of a rational number in lowest terms",
     NULL},
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(long_doc,
"int(x=0) -> integer\n\
int(x, base=10) -> integer\n\
\n\
Convert a number or string to an integer, or return 0 if no arguments\n\
are given.  If x is a number, return x.__int__().  For floating point\n\
numbers, this truncates towards zero.\n\
\n\
If x is not a number or if base is given, then x must be a string,\n\
bytes, or bytearray instance representing an integer literal in the\n\
given base.  The literal can be preceded by '+' or '-' and be surrounded\n\
by whitespace.  The base defaults to 10.  Valid bases are 0 and 2-36.\n\
Base 0 means to interpret the base from the string as an integer literal.\n\
>>> int('0b100', base=0)\n\
4");

static PyNumberMethods long_as_number = {
    (binaryfunc)long_add,       /*nb_add*/
    (binaryfunc)long_sub,       /*nb_subtract*/
    (binaryfunc)long_mul,       /*nb_multiply*/
    long_mod,                   /*nb_remainder*/
    long_divmod,                /*nb_divmod*/
    long_pow,                   /*nb_power*/
    (unaryfunc)long_neg,        /*nb_negative*/
    (unaryfunc)long_long,       /*tp_positive*/
    (unaryfunc)long_abs,        /*tp_absolute*/
    (inquiry)long_bool,         /*tp_bool*/
    (unaryfunc)long_invert,     /*nb_invert*/
    long_lshift,                /*nb_lshift*/
    (binaryfunc)long_rshift,    /*nb_rshift*/
    long_and,                   /*nb_and*/
    long_xor,                   /*nb_xor*/
    long_or,                    /*nb_or*/
    long_long,                  /*nb_int*/
    0,                          /*nb_reserved*/
    long_float,                 /*nb_float*/
    0,                          /* nb_inplace_add */
    0,                          /* nb_inplace_subtract */
    0,                          /* nb_inplace_multiply */
    0,                          /* nb_inplace_remainder */
    0,                          /* nb_inplace_power */
    0,                          /* nb_inplace_lshift */
    0,                          /* nb_inplace_rshift */
    0,                          /* nb_inplace_and */
    0,                          /* nb_inplace_xor */
    0,                          /* nb_inplace_or */
    long_div,                   /* nb_floor_divide */
    long_true_divide,           /* nb_true_divide */
    0,                          /* nb_inplace_floor_divide */
    0,                          /* nb_inplace_true_divide */
    long_long,                  /* nb_index */
};

PyTypeObject PyLong_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "int",                                      /* tp_name */
    offsetof(PyLongObject, ob_digit),           /* tp_basicsize */
    sizeof(digit),                              /* tp_itemsize */
    long_dealloc,                               /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    long_to_decimal_string,                     /* tp_repr */
    &long_as_number,                            /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    (hashfunc)long_hash,                        /* tp_hash */
    0,                                          /* tp_call */
    long_to_decimal_string,                     /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_LONG_SUBCLASS,               /* tp_flags */
    long_doc,                                   /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    long_richcompare,                           /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    long_methods,                               /* tp_methods */
    0,                                          /* tp_members */
    long_getset,                                /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    long_new,                                   /* tp_new */
    PyObject_Del,                               /* tp_free */
};

static PyTypeObject Int_InfoType;

PyDoc_STRVAR(int_info__doc__,
"sys.int_info\n\
\n\
A struct sequence that holds information about Python's\n\
internal representation of integers.  The attributes are read only.");

static PyStructSequence_Field int_info_fields[] = {
    {"bits_per_digit", "size of a digit in bits"},
    {"sizeof_digit", "size in bytes of the C type used to represent a digit"},
    {"gmp_version", "version tuple of GNU MP library"},
#ifdef PyLong_GMP_BACKEND_MPIR
    {"mpir_version", "version tuple of MPIR library"},
#endif
    {NULL, NULL}
};

static PyStructSequence_Desc int_info_desc = {
    "sys.int_info",   /* name */
    int_info__doc__,  /* doc */
    int_info_fields,  /* fields */
#ifdef PyLong_GMP_BACKEND_MPIR
    4                 /* number of fields */
#else
    3                 /* number of fields */
#endif
};

PyObject *
PyLong_GetInfo(void)
{
    PyObject* int_info;
    int field = 0;
    int_info = PyStructSequence_New(&Int_InfoType);
    if (int_info == NULL)
        return NULL;
    PyStructSequence_SET_ITEM(int_info, field++,
                              PyLong_FromLong((sizeof(digit)*8)));
    PyStructSequence_SET_ITEM(int_info, field++,
                              PyLong_FromLong(sizeof(digit)));
    PyStructSequence_SET_ITEM(int_info, field++,
                              Py_BuildValue("iii",
                                  __GNU_MP_VERSION,
                                  __GNU_MP_VERSION_MINOR,
                                  __GNU_MP_VERSION_PATCHLEVEL));
#ifdef PyLong_GMP_BACKEND_MPIR
    PyStructSequence_SET_ITEM(int_info, field++,
                              Py_BuildValue("iii",
                                  __MPIR_VERSION,
                                  __MPIR_VERSION_MINOR,
                                  __MPIR_VERSION_PATCHLEVEL));
#endif
    if (PyErr_Occurred()) {
        Py_CLEAR(int_info);
        return NULL;
    }
    return int_info;
}

int
_PyLong_Init(void)
{
    //fprintf(stderr, "_PyLong_Init(void)\n");
#if NSMALLNEGINTS + NSMALLPOSINTS > 0
    int ival, size;
    PyLongObject *v = small_ints;

    for (ival = -NSMALLNEGINTS; ival <  NSMALLPOSINTS; ival++, v++) {
        size = (ival < 0) ? -1 : ((ival == 0) ? 0 : 1);
        if (Py_TYPE(v) == &PyLong_Type) {
            /* The element is already initialized, most likely
             * the Python interpreter was initialized before.
             */
            Py_ssize_t refcnt;
            PyObject* op = (PyObject*)v;

            refcnt = Py_REFCNT(op) < 0 ? 0 : Py_REFCNT(op);
            _Py_NewReference(op);
            /* _Py_NewReference sets the ref count to 1 but
             * the ref count might be larger. Set the refcnt
             * to the original refcnt + 1 */
            Py_REFCNT(op) = refcnt + 1;
            assert(Py_SIZE(op) == size);
            assert(v->ob_digit[0] == (digit)abs(ival));
        }
        else {
            (void)PyObject_INIT(v, &PyLong_Type);
        }
        Py_SIZE(v) = size;
        v->ob_digit[0] = (digit)abs(ival);
    }
#endif

    /* initialize int_info */
    if (Int_InfoType.tp_name == NULL) {
        if (PyStructSequence_InitType2(&Int_InfoType, &int_info_desc) < 0)
            return 0;
    }

    return 1;
}

void
PyLong_Fini(void)
{
    /* Integers are currently statically allocated. Py_DECREF is not
       needed, but Python must forget about the reference or multiple
       reinitializations will fail. */
#if NSMALLNEGINTS + NSMALLPOSINTS > 0
    int i;
    PyLongObject *v = small_ints;
    for (i = 0; i < NSMALLNEGINTS + NSMALLPOSINTS; i++, v++) {
        _Py_DEC_REFTOTAL;
        _Py_ForgetReference((PyObject*)v);
    }
#endif
}
