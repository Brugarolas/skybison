// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "cpython-data.h"

#include "api-handle.h"
#include "runtime.h"

namespace py {

PY_EXPORT int PyMemoryView_Check_Func(PyObject* obj) {
  return ApiHandle::asObject(ApiHandle::fromPyObject(obj)).isMemoryView();
}

PY_EXPORT PyObject* PyMemoryView_FromMemory(char* memory, Py_ssize_t size,
                                            int flags) {
  DCHECK(memory != nullptr, "memory must not be null");
  DCHECK(flags == PyBUF_READ || flags == PyBUF_WRITE,
         "flags must be either PyBUF_READ or PyBUF_WRITE");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object none(&scope, NoneType::object());
  return ApiHandle::newReferenceWithManaged(
      runtime,
      runtime->newMemoryViewFromCPtr(
          thread, none, memory, size,
          flags == PyBUF_READ ? ReadOnly::ReadOnly : ReadOnly::ReadWrite));
}

PY_EXPORT PyObject* PyMemoryView_FromObject(PyObject* obj) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object object(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  Object result(&scope,
                thread->invokeFunction1(ID(builtins), ID(memoryview), object));
  if (result.isError()) {
    return nullptr;
  }
  return ApiHandle::newReferenceWithManaged(thread->runtime(), *result);
}

PY_EXPORT PyObject* PyMemoryView_GetContiguous(PyObject* /* j */, int /* e */,
                                               char /* r */) {
  UNIMPLEMENTED("PyMemoryView_GetContiguous");
}

PY_EXPORT PyTypeObject* PyMemoryView_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kMemoryView)));
}

}  // namespace py
