// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
// longobject.c implementation

#include "cpython-data.h"
#include "cpython-func.h"

#include "api-handle.h"
#include "float-builtins.h"
#include "handles.h"
#include "int-builtins.h"
#include "objects.h"
#include "runtime.h"

// Table of digit values for 8-bit string -> integer conversion.
// '0' maps to 0, ..., '9' maps to 9.
// 'a' and 'A' map to 10, ..., 'z' and 'Z' map to 35.
// All other indices map to 37.
// Note that when converting a base B string, a char c is a legitimate
// base B digit iff _PyLong_DigitValue[Py_CHARMASK(c)] < B.
// clang-format off
const unsigned char _PyLong_DigitValue[256] = { // NOLINT
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
// clang-format on

namespace py {

PY_EXPORT int PyLong_CheckExact_Func(PyObject* obj) {
  RawObject arg = ApiHandle::asObject(ApiHandle::fromPyObject(obj));
  return arg.isSmallInt() || arg.isLargeInt();
}

PY_EXPORT int PyLong_Check_Func(PyObject* obj) {
  return Thread::current()->runtime()->isInstanceOfInt(
      ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
}

// Converting from signed ints.

PY_EXPORT PyObject* PyLong_FromLong(long ival) {
  if (SmallInt::isValid(ival)) {
    return ApiHandle::handleFromImmediate(SmallInt::fromWord(ival));
  }
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReferenceWithManaged(runtime, runtime->newInt(ival));
}

PY_EXPORT PyObject* PyLong_FromLongLong(long long ival) {
  static_assert(sizeof(ival) <= sizeof(long), "Unsupported long long size");
  return PyLong_FromLong(ival);
}

PY_EXPORT PyObject* PyLong_FromPid(pid_t ival) {
  static_assert(sizeof(ival) <= sizeof(long), "Unsupported pid_t size");
  return PyLong_FromLong(ival);
}

PY_EXPORT PyObject* PyLong_FromSsize_t(Py_ssize_t ival) {
  static_assert(sizeof(ival) <= sizeof(long), "Unsupported Py_ssize_t size");
  return PyLong_FromLong(ival);
}

// Converting from unsigned ints.

PY_EXPORT PyObject* PyLong_FromUnsignedLong(unsigned long ival) {
  static_assert(sizeof(ival) <= sizeof(uword),
                "Unsupported unsigned long type");
  if (ival <= static_cast<unsigned long>(SmallInt::kMaxValue)) {
    return ApiHandle::handleFromImmediate(
        SmallInt::fromWord(static_cast<word>(ival)));
  }
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReferenceWithManaged(runtime,
                                            runtime->newIntFromUnsigned(ival));
}

PY_EXPORT PyObject* PyLong_FromUnsignedLongLong(unsigned long long ival) {
  static_assert(sizeof(ival) <= sizeof(unsigned long),
                "Unsupported unsigned long long size");
  return PyLong_FromUnsignedLong(ival);
}

PY_EXPORT PyObject* PyLong_FromSize_t(size_t ival) {
  static_assert(sizeof(ival) <= sizeof(unsigned long),
                "Unsupported size_t size");
  return PyLong_FromUnsignedLong(ival);
}

// Attempt to convert the given PyObject to T. When overflow != nullptr,
// *overflow will be set to -1, 1, or 0 to indicate underflow, overflow, or
// neither, respectively. When under/overflow occurs, -1 is returned; otherwise,
// the value is returned.
//
// When overflow == nullptr, an exception will be raised and -1 is returned if
// the value doesn't fit in T.
template <typename T>
static T asInt(Thread* thread, PyObject* pylong, const char* type_name,
               int* overflow) {
  if (pylong == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }

  HandleScope scope(thread);
  Object long_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylong)));
  if (!thread->runtime()->isInstanceOfInt(*long_obj)) {
    long_obj =
        thread->invokeFunction1(ID(builtins), ID(_index_or_int), long_obj);
    if (long_obj.isError()) {
      return -1;
    }
  }

  Int num(&scope, intUnderlying(*long_obj));
  auto const result = num.asInt<T>();
  if (result.error == CastError::None) {
    if (overflow) *overflow = 0;
    return result.value;
  }

  if (overflow) {
    *overflow = (result.error == CastError::Underflow) ? -1 : 1;
  } else if (result.error == CastError::Underflow &&
             std::is_unsigned<T>::value) {
    thread->raiseWithFmt(LayoutId::kOverflowError,
                         "can't convert negative value to unsigned");
  } else {
    thread->raiseWithFmt(LayoutId::kOverflowError,
                         "Python int too big to convert to C %s", type_name);
  }
  return -1;
}

template <typename T>
static T asIntWithoutOverflowCheck(PyObject* pylong) {
  Thread* thread = Thread::current();
  if (pylong == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }

  HandleScope scope(thread);
  Object long_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylong)));
  if (!thread->runtime()->isInstanceOfInt(*long_obj)) {
    long_obj =
        thread->invokeFunction1(ID(builtins), ID(_index_or_int), long_obj);
    if (long_obj.isError()) {
      return -1;
    }
  }

  Int num(&scope, intUnderlying(*long_obj));
  return num.digitAt(0);
}

PY_EXPORT size_t _PyLong_NumBits(PyObject* pylong) {
  DCHECK(pylong != nullptr, "argument to _PyLong_NumBits must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object long_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylong)));
  DCHECK(thread->runtime()->isInstanceOfInt(*long_obj),
         "argument to _PyLong_NumBits must be an int");
  Int obj(&scope, intUnderlying(*long_obj));
  return obj.bitLength();
}

// Converting to signed ints.

PY_EXPORT int _PyLong_AsInt(PyObject* pylong) {
  return asInt<int>(Thread::current(), pylong, "int", nullptr);
}

PY_EXPORT long PyLong_AsLong(PyObject* pylong) {
  return asInt<long>(Thread::current(), pylong, "long", nullptr);
}

PY_EXPORT long long PyLong_AsLongLong(PyObject* val) {
  return asInt<long long>(Thread::current(), val, "long long", nullptr);
}

PY_EXPORT pid_t PyLong_AsPid(PyObject* val) {
  return asInt<pid_t>(Thread::current(), val, "pid_t", nullptr);
}

PY_EXPORT Py_ssize_t PyLong_AsSsize_t(PyObject* val) {
  return asInt<Py_ssize_t>(Thread::current(), val, "ssize_t", nullptr);
}

// Converting to unsigned ints.

PY_EXPORT unsigned long PyLong_AsUnsignedLong(PyObject* val) {
  return asInt<unsigned long>(Thread::current(), val, "unsigned long", nullptr);
}

PY_EXPORT unsigned long long PyLong_AsUnsignedLongLong(PyObject* val) {
  return asInt<unsigned long long>(Thread::current(), val, "unsigned long long",
                                   nullptr);
}

PY_EXPORT size_t PyLong_AsSize_t(PyObject* val) {
  return asInt<size_t>(Thread::current(), val, "size_t", nullptr);
}

PY_EXPORT long PyLong_AsLongAndOverflow(PyObject* pylong, int* overflow) {
  return asInt<long>(Thread::current(), pylong, "", overflow);
}

PY_EXPORT long long PyLong_AsLongLongAndOverflow(PyObject* pylong,
                                                 int* overflow) {
  return asInt<long long>(Thread::current(), pylong, "", overflow);
}

PY_EXPORT PyObject* PyLong_FromDouble(double value) {
  Thread* thread = Thread::current();
  RawObject result = intFromDouble(thread, value);
  if (result.isErrorException()) {
    return nullptr;
  }
  return ApiHandle::newReference(thread->runtime(), result);
}

PY_EXPORT PyObject* PyLong_FromString(const char* str, char** pend, int base) {
  if (pend != nullptr) {
    UNIMPLEMENTED("pend != NULL");
  }
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Str str_obj(&scope, runtime->newStrFromCStr(str));
  Int base_obj(&scope, runtime->newInt(base));
  Type int_cls(&scope, runtime->typeAt(LayoutId::kInt));
  Object result(&scope,
                thread->invokeFunction3(ID(builtins), ID(_int_new_from_str),
                                        int_cls, str_obj, base_obj));
  if (result.isError()) {
    DCHECK(!result.isErrorNotFound(), "could not call _int_new_from_str");
    return nullptr;
  }
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT double PyLong_AsDouble(PyObject* obj) {
  Thread* thread = Thread::current();
  if (obj == nullptr) {
    thread->raiseBadInternalCall();
    return -1.0;
  }
  HandleScope scope(thread);
  Object object(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  if (!thread->runtime()->isInstanceOfInt(*object)) {
    thread->raiseWithFmt(LayoutId::kTypeError, "an integer is required");
    return -1.0;
  }
  Int value(&scope, intUnderlying(*object));
  double result;
  Object err(&scope, convertIntToDouble(thread, value, &result));
  return err.isError() ? -1.0 : result;
}

PY_EXPORT unsigned long long PyLong_AsUnsignedLongLongMask(PyObject* op) {
  return asIntWithoutOverflowCheck<unsigned long long>(op);
}

PY_EXPORT unsigned long PyLong_AsUnsignedLongMask(PyObject* op) {
  return asIntWithoutOverflowCheck<unsigned long>(op);
}

PY_EXPORT void* PyLong_AsVoidPtr(PyObject* pylong) {
  static_assert(kPointerSize >= sizeof(long long),
                "PyLong_AsVoidPtr: sizeof(long long) < sizeof(void*)");
  long long x;
  if (PyLong_Check(pylong) && _PyLong_Sign(pylong) < 0) {
    x = PyLong_AsLongLong(pylong);
  } else {
    x = PyLong_AsUnsignedLongLong(pylong);
  }

  if (x == -1 && PyErr_Occurred()) return nullptr;
  return reinterpret_cast<void*>(x);
}

PY_EXPORT PyObject* PyLong_FromVoidPtr(void* ptr) {
  static_assert(kPointerSize >= sizeof(long long),
                "PyLong_FromVoidPtr: sizeof(long long) < sizeof(void*)");
  return PyLong_FromUnsignedLongLong(reinterpret_cast<unsigned long long>(ptr));
}

PY_EXPORT PyObject* PyLong_GetInfo() { UNIMPLEMENTED("PyLong_GetInfo"); }

PY_EXPORT int _PyLong_AsByteArray(PyLongObject* longobj, unsigned char* dst,
                                  size_t n, int little_endian, int is_signed) {
  DCHECK(longobj != nullptr, "null argument to _PyLong_AsByteArray");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  PyObject* pyobj = reinterpret_cast<PyObject*>(longobj);
  Object self_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pyobj)));
  Int self(&scope, intUnderlying(*self_obj));
  if (!is_signed && self.isNegative()) {
    thread->raiseWithFmt(LayoutId::kOverflowError,
                         "can't convert negative int to unsigned");
    return -1;
  }
  word length = static_cast<word>(n);
  endian endianness = little_endian ? endian::little : endian::big;
  Bytes result(&scope, runtime->intToBytes(thread, self, length, endianness));
  result.copyTo(dst, length);

  // Check for overflow.
  word num_digits = self.numDigits();
  uword high_digit = self.digitAt(num_digits - 1);
  word bit_length =
      num_digits * kBitsPerWord - Utils::numRedundantSignBits(high_digit);
  if (bit_length > length * kBitsPerByte + !is_signed) {
    thread->raiseWithFmt(LayoutId::kOverflowError, "int too big to convert");
    return -1;
  }
  return 0;
}

PY_EXPORT PyTypeObject* PyLong_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(
      ApiHandle::borrowedReference(runtime, runtime->typeAt(LayoutId::kInt)));
}

PY_EXPORT PyObject* _PyLong_Copy(PyLongObject* longobj) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  PyObject* pyobj = reinterpret_cast<PyObject*>(longobj);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pyobj)));
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfInt(*obj), "_PyLong_Copy requires an int");
  return ApiHandle::newReference(runtime, intUnderlying(*obj));
}

PY_EXPORT PyObject* _PyLong_DivmodNear(PyObject* a, PyObject* b) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object dividend_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(a)));
  Object divisor_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(b)));
  if (!runtime->isInstanceOfInt(*dividend_obj) ||
      !runtime->isInstanceOfInt(*divisor_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError,
                         "non-integer arguments in division");
    return nullptr;
  }

  Int dividend(&scope, intUnderlying(*dividend_obj));
  Int divisor(&scope, intUnderlying(*divisor_obj));
  Object quotient(&scope, NoneType::object());
  Object remainder(&scope, NoneType::object());
  if (!intDivmodNear(thread, dividend, divisor, &quotient, &remainder)) {
    thread->raiseWithFmt(LayoutId::kZeroDivisionError,
                         "integer division or modulo by zero");
    return nullptr;
  }
  return ApiHandle::newReference(runtime,
                                 runtime->newTupleWith2(quotient, remainder));
}

PY_EXPORT double _PyLong_Frexp(PyLongObject*, Py_ssize_t*) {
  UNIMPLEMENTED("_PyLong_Frexp");
}

PY_EXPORT PyObject* _PyLong_FromByteArray(const unsigned char* bytes, size_t n,
                                          int little_endian, int is_signed) {
  if (n == 0) return PyLong_FromLong(0);
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  // This copies the bytes an extra time, but it is more important for the
  // runtime to accommodate int.from_bytes(), so allow the extra copy.
  Bytes source(&scope, runtime->newBytesWithAll(
                           View<byte>(bytes, static_cast<word>(n))));
  endian endianness = little_endian ? endian::little : endian::big;
  Object result(&scope,
                runtime->bytesToInt(thread, source, endianness, is_signed));
  return result.isError() ? nullptr : ApiHandle::newReference(runtime, *result);
}

PY_EXPORT PyObject* _PyLong_GCD(PyObject* a, PyObject* b) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object dividend_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(a)));
  Object divisor_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(b)));
  Runtime* runtime = thread->runtime();

  DCHECK(runtime->isInstanceOfInt(*dividend_obj),
         "dividend argument to _PyLong_GCD must be an int");
  DCHECK(runtime->isInstanceOfInt(*divisor_obj),
         "divisor argument to _PyLong_GCD must be an int");
  Int dividend(&scope, intUnderlying(*dividend_obj));
  Int divisor(&scope, intUnderlying(*divisor_obj));

  return ApiHandle::newReference(runtime, intGCD(thread, dividend, divisor));
}

PY_EXPORT PyLongObject* _PyLong_FromNbInt(PyObject*) {
  UNIMPLEMENTED("_PyLong_FromNbInt");
}

PY_EXPORT PyObject* _PyLong_Lshift(PyObject* a, size_t shiftby) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(a)));
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfInt(*obj), "_PyLong_Lshift requires an int");
  Int num(&scope, intUnderlying(*obj));
  if (num.isZero()) {
    return ApiHandle::newReference(runtime, SmallInt::fromWord(0));
  }
  Int shift(&scope, runtime->newIntFromUnsigned(shiftby));
  return ApiHandle::newReference(runtime,
                                 runtime->intBinaryLshift(thread, num, shift));
}

PY_EXPORT PyObject* _PyLong_One_Ptr() {
  return ApiHandle::borrowedReference(Thread::current()->runtime(),
                                      SmallInt::fromWord(1));
}

PY_EXPORT PyObject* _PyLong_Rshift(PyObject* a, size_t shiftby) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(a)));
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfInt(*obj), "_PyLong_Rshift requires an int");
  Int num(&scope, intUnderlying(*obj));
  if (num.isZero()) {
    return ApiHandle::newReference(runtime, SmallInt::fromWord(0));
  }
  Int shift(&scope, runtime->newIntFromUnsigned(shiftby));
  return ApiHandle::newReference(runtime,
                                 runtime->intBinaryRshift(thread, num, shift));
}

PY_EXPORT int _PyLong_Sign(PyObject* vv) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(vv)));
  DCHECK(thread->runtime()->isInstanceOfInt(*obj), "requires an integer");
  Int value(&scope, intUnderlying(*obj));
  return value.isZero() ? 0 : (value.isNegative() ? -1 : 1);
}

template <typename T>
static T unsignedConverter(PyObject* obj, void* ptr, const char* type_name) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object object(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  if (thread->runtime()->isInstanceOfInt(*object)) {
    Int num(&scope, intUnderlying(*object));
    if (num.isNegative()) {
      thread->raiseWithFmt(LayoutId::kValueError, "value must be positive");
      return 0;
    }
  }
  T result = asInt<T>(thread, obj, type_name, nullptr);
  if (result == static_cast<T>(-1) && thread->hasPendingException()) {
    return 0;
  }
  *reinterpret_cast<T*>(ptr) = result;
  return 1;
}

PY_EXPORT int _PyLong_Size_t_Converter(PyObject* obj, void* ptr) {
  return unsignedConverter<size_t>(obj, ptr, "size_t");
}

PY_EXPORT int _PyLong_UnsignedInt_Converter(PyObject* obj, void* ptr) {
  return unsignedConverter<unsigned int>(obj, ptr, "unsigned int");
}

PY_EXPORT int _PyLong_UnsignedLong_Converter(PyObject* obj, void* ptr) {
  return unsignedConverter<unsigned long>(obj, ptr, "unsigned long");
}

PY_EXPORT int _PyLong_UnsignedLongLong_Converter(PyObject* obj, void* ptr) {
  return unsignedConverter<unsigned long long>(obj, ptr, "unsigned long long");
}

PY_EXPORT int _PyLong_UnsignedShort_Converter(PyObject* obj, void* ptr) {
  return unsignedConverter<unsigned short>(obj, ptr, "unsigned short");
}

PY_EXPORT PyObject* _PyLong_Zero_Ptr() {
  return ApiHandle::borrowedReference(Thread::current()->runtime(),
                                      SmallInt::fromWord(0));
}

}  // namespace py
