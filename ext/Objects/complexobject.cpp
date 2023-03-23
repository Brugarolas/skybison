// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include <cerrno>
#include <cmath>

#include "api-handle.h"
#include "float-builtins.h"
#include "runtime.h"
#include "type-builtins.h"

namespace py {

PY_EXPORT double _Py_c_abs(Py_complex x) {
  double result = std::hypot(x.real, x.imag);
  if (std::isfinite(result)) {
    errno = 0;
    return result;
  }
  errno = 0;
  if (std::isinf(x.real)) return std::fabs(x.real);
  if (std::isinf(x.imag)) return std::fabs(x.imag);
  return NAN;
}

PY_EXPORT Py_complex _Py_c_diff(Py_complex x, Py_complex y) {
  return {x.real - y.real, x.imag - y.imag};
}

PY_EXPORT Py_complex _Py_c_neg(Py_complex x) { return {-x.real, -x.imag}; }

PY_EXPORT Py_complex _Py_c_prod(Py_complex x, Py_complex y) {
  double real = x.real * y.real - x.imag * y.imag;
  double imag = x.real * y.imag + x.imag * y.real;
  return {real, imag};
}

PY_EXPORT Py_complex _Py_c_quot(Py_complex x, Py_complex y) {
  double real;
  double imag;
  if (y.imag == 0.0) {
    errno = EDOM;
    real = 0.0;
    imag = 0.0;
  } else if (std::fabs(y.real) >= std::fabs(y.imag)) {
    double ratio = y.imag / y.real;
    double den = y.real + y.imag * ratio;
    real = (x.real + x.imag * ratio) / den;
    imag = (x.imag - x.real * ratio) / den;
  } else if (std::fabs(y.imag) >= std::fabs(y.real)) {
    double ratio = y.real / y.imag;
    double den = y.real * ratio + y.imag;
    real = (x.real * ratio + x.imag) / den;
    imag = (x.imag * ratio - x.real) / den;
  } else {
    real = NAN;
    imag = NAN;
  }
  return {real, imag};
}

PY_EXPORT Py_complex _Py_c_sum(Py_complex x, Py_complex y) {
  return {x.real + y.real, x.imag + y.imag};
}

PY_EXPORT int PyComplex_CheckExact_Func(PyObject* p) {
  return ApiHandle::asObject(ApiHandle::fromPyObject(p)).isComplex();
}

PY_EXPORT int PyComplex_Check_Func(PyObject* p) {
  return Thread::current()->runtime()->isInstanceOfComplex(
      ApiHandle::asObject(ApiHandle::fromPyObject(p)));
}

PY_EXPORT Py_complex PyComplex_AsCComplex(PyObject* pycomplex) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pycomplex)));
  if (runtime->isInstanceOfComplex(*obj)) {
    Complex comp(&scope, complexUnderlying(*obj));
    return {comp.real(), comp.imag()};
  }

  // Try calling __complex__
  Object result(&scope, thread->invokeMethod1(obj, ID(__complex__)));
  if (!result.isError()) {
    if (!runtime->isInstanceOfComplex(*result)) {
      thread->raiseWithFmt(LayoutId::kTypeError,
                           "__complex__ should returns a complex object");
      return {-1.0, 0.0};
    }
    Complex comp(&scope, complexUnderlying(*result));
    return {comp.real(), comp.imag()};
  }
  // If __complex__ is not defined, call __float__
  if (result.isErrorNotFound()) {
    // Use __float__ for the real part and set the imaginary part to 0
    if (!runtime->isInstanceOfFloat(*obj)) {
      obj = thread->invokeFunction1(ID(builtins), ID(_float), obj);
      if (obj.isError()) return {-1.0, 0.0};
    }
    return {floatUnderlying(*obj).value(), 0.0};
  }
  DCHECK(result.isErrorException(), "result should be an exception");
  return {-1.0, 0.0};
}

PY_EXPORT PyObject* PyComplex_FromCComplex(Py_complex cmp) {
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReferenceWithManaged(
      runtime, runtime->newComplex(cmp.real, cmp.imag));
}

PY_EXPORT double PyComplex_ImagAsDouble(PyObject* pycomplex) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pycomplex)));
  if (!runtime->isInstanceOfComplex(*obj)) return 0.0;
  Complex comp(&scope, complexUnderlying(*obj));
  return comp.imag();
}

PY_EXPORT double PyComplex_RealAsDouble(PyObject* pycomplex) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pycomplex)));
  if (runtime->isInstanceOfComplex(*obj)) {
    Complex comp(&scope, complexUnderlying(*obj));
    return comp.real();
  }
  if (!runtime->isInstanceOfFloat(*obj)) {
    obj = thread->invokeFunction1(ID(builtins), ID(_float), obj);
    if (obj.isError()) return -1;
  }
  return floatUnderlying(*obj).value();
}

PY_EXPORT PyTypeObject* PyComplex_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kComplex)));
}

PY_EXPORT PyObject* PyComplex_FromDoubles(double real, double imag) {
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReferenceWithManaged(runtime,
                                            runtime->newComplex(real, imag));
}

}  // namespace py
