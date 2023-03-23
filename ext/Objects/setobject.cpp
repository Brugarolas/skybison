// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "cpython-func.h"

#include "api-handle.h"
#include "runtime.h"
#include "set-builtins.h"

namespace py {

PY_EXPORT int PyAnySet_Check_Func(PyObject* arg) {
  DCHECK(arg != nullptr, "obj must not be nullptr");
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  RawObject obj = ApiHandle::asObject(ApiHandle::fromPyObject(arg));
  return runtime->isInstanceOfSet(obj) || runtime->isInstanceOfFrozenSet(obj);
}

PY_EXPORT int PyAnySet_CheckExact_Func(PyObject* arg) {
  DCHECK(arg != nullptr, "obj must not be nullptr");
  RawObject obj = ApiHandle::asObject(ApiHandle::fromPyObject(arg));
  return obj.isSet() || obj.isFrozenSet();
}

PY_EXPORT int PyFrozenSet_Check_Func(PyObject* obj) {
  DCHECK(obj != nullptr, "obj must not be nullptr");
  return Thread::current()->runtime()->isInstanceOfFrozenSet(
      ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
}

PY_EXPORT int PyFrozenSet_CheckExact_Func(PyObject* obj) {
  DCHECK(obj != nullptr, "obj must not be nullptr");
  return ApiHandle::asObject(ApiHandle::fromPyObject(obj)).isFrozenSet();
}

PY_EXPORT PyObject* PyFrozenSet_New(PyObject* iterable) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  if (iterable == nullptr) {
    return ApiHandle::newReferenceWithManaged(runtime,
                                              runtime->emptyFrozenSet());
  }
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(iterable)));
  FrozenSet set(&scope, runtime->newFrozenSet());
  Object result(&scope, setUpdate(thread, set, obj));
  if (result.isError()) {
    return nullptr;
  }
  return ApiHandle::newReferenceWithManaged(runtime, *set);
}

PY_EXPORT PyTypeObject* PyFrozenSet_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kFrozenSet)));
}

PY_EXPORT PyTypeObject* PySetIter_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kSetIterator)));
}

PY_EXPORT int PySet_Add(PyObject* anyset, PyObject* key) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(anyset)));

  // TODO(T28454727): add FrozenSet
  if (!runtime->isInstanceOfSet(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }

  Set set(&scope, *set_obj);
  Object key_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(key)));
  Object hash_obj(&scope, Interpreter::hash(thread, key_obj));
  if (hash_obj.isErrorException()) {
    return -1;
  }
  word hash = SmallInt::cast(*hash_obj).value();

  setAdd(thread, set, key_obj, hash);
  return 0;
}

PY_EXPORT int PySet_Check_Func(PyObject* obj) {
  return Thread::current()->runtime()->isInstanceOfSet(
      ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
}

PY_EXPORT int _PySet_NextEntry(PyObject* pyset, Py_ssize_t* ppos,
                               PyObject** pkey, Py_hash_t* phash) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pyset)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfSetBase(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  SetBase set(&scope, *set_obj);
  Object value(&scope, NoneType::object());
  DCHECK(phash != nullptr, "phash must not be null");
  DCHECK(pkey != nullptr, "pkey must not be null");
  if (!setNextItemHash(set, ppos, &value, phash)) {
    return false;
  }
  *pkey = ApiHandle::borrowedReference(runtime, *value);
  return true;
}

PY_EXPORT int PySet_Clear(PyObject* anyset) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(anyset)));
  if (!runtime->isInstanceOfSetBase(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  SetBase set(&scope, *set_obj);
  set.setNumItems(0);
  set.setData(runtime->emptyTuple());
  return 0;
}

PY_EXPORT int PySet_Contains(PyObject* anyset, PyObject* key) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(anyset)));

  if (!runtime->isInstanceOfSetBase(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }

  SetBase set(&scope, *set_obj);
  Object key_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(key)));
  Object hash_obj(&scope, Interpreter::hash(thread, key_obj));
  if (hash_obj.isErrorException()) {
    return -1;
  }
  word hash = SmallInt::cast(*hash_obj).value();
  return setIncludes(thread, set, key_obj, hash);
}

PY_EXPORT int PySet_Discard(PyObject* pyset, PyObject* pykey) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pyset)));
  if (!runtime->isInstanceOfSet(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  Set set(&scope, *set_obj);
  Object key(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pykey)));
  Object hash_obj(&scope, Interpreter::hash(thread, key));
  if (hash_obj.isErrorException()) {
    return -1;
  }
  word hash = SmallInt::cast(*hash_obj).value();
  return setRemove(thread, set, key, hash);
}

PY_EXPORT PyObject* PySet_New(PyObject* iterable) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  if (iterable == nullptr) {
    return ApiHandle::newReferenceWithManaged(runtime, runtime->newSet());
  }

  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(iterable)));
  Set set(&scope, runtime->newSet());

  Object result(&scope, setUpdate(thread, set, obj));
  if (result.isError()) {
    return nullptr;
  }

  return ApiHandle::newReferenceWithManaged(runtime, *set);
}

PY_EXPORT PyObject* PySet_Pop(PyObject* pyset) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pyset)));
  if (!runtime->isInstanceOfSet(*set_obj)) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  Set set(&scope, *set_obj);
  Object result(&scope, setPop(thread, set));
  if (result.isErrorException()) return nullptr;
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT Py_ssize_t PySet_Size(PyObject* anyset) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  Object set_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(anyset)));
  if (!runtime->isInstanceOfSetBase(*set_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }

  SetBase set(&scope, *set_obj);
  return set.numItems();
}

PY_EXPORT PyTypeObject* PySet_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(
      ApiHandle::borrowedReference(runtime, runtime->typeAt(LayoutId::kSet)));
}

}  // namespace py
