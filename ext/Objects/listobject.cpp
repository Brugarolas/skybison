// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
// listobject.c implementation

#include "cpython-func.h"

#include "api-handle.h"
#include "handles.h"
#include "list-builtins.h"
#include "objects.h"
#include "runtime.h"

namespace py {

PY_EXPORT PyTypeObject* PyListIter_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kListIterator)));
}

PY_EXPORT PyObject* PyList_New(Py_ssize_t size) {
  Thread* thread = Thread::current();
  if (size < 0) {
    thread->raiseBadInternalCall();
    return nullptr;
  }

  Runtime* runtime = thread->runtime();
  if (size == 0) {
    return ApiHandle::newReferenceWithManaged(runtime, runtime->newList());
  }

  HandleScope scope(thread);
  List list(&scope, runtime->newList());
  list.setItems(runtime->newMutableTuple(size));
  list.setNumItems(size);
  return ApiHandle::newReferenceWithManaged(runtime, *list);
}

PY_EXPORT PyTypeObject* PyList_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(
      ApiHandle::borrowedReference(runtime, runtime->typeAt(LayoutId::kList)));
}

PY_EXPORT int PyList_CheckExact_Func(PyObject* obj) {
  return ApiHandle::asObject(ApiHandle::fromPyObject(obj)).isList();
}

PY_EXPORT int PyList_Check_Func(PyObject* obj) {
  return Thread::current()->runtime()->isInstanceOfList(
      ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
}

PY_EXPORT PyObject* PyList_AsTuple(PyObject* pylist) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  if (pylist == nullptr) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  List list(&scope, *list_obj);
  word length = list.numItems();
  if (length == 0) {
    return ApiHandle::newReferenceWithManaged(runtime, runtime->emptyTuple());
  }
  MutableTuple result(&scope, runtime->newMutableTuple(length));
  result.replaceFromWith(0, Tuple::cast(list.items()), length);
  return ApiHandle::newReferenceWithManaged(runtime, result.becomeImmutable());
}

PY_EXPORT PyObject* PyList_GetItem(PyObject* pylist, Py_ssize_t i) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  List list(&scope, *list_obj);
  if (i >= list.numItems()) {
    thread->raiseWithFmt(LayoutId::kIndexError,
                         "index out of bounds in PyList_GetItem");
    return nullptr;
  }
  return ApiHandle::borrowedReference(runtime, list.at(i));
}

PY_EXPORT int PyList_Reverse(PyObject* pylist) {
  Thread* thread = Thread::current();
  if (pylist == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  List list(&scope, *list_obj);
  listReverse(thread, list);
  return 0;
}

PY_EXPORT int PyList_SET_ITEM_Func(PyObject* pylist, Py_ssize_t i,
                                   PyObject* item) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  DCHECK(runtime->isInstanceOfList(*list_obj), "pylist must be a list");
  List list(&scope, *list_obj);
  DCHECK_INDEX(i, list.numItems());
  list.atPut(i, item == nullptr ? NoneType::object()
                                : ApiHandle::stealReference(item));
  return 0;
}

PY_EXPORT int PyList_SetItem(PyObject* pylist, Py_ssize_t i, PyObject* item) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  Object newitem(&scope, item == nullptr ? NoneType::object()
                                         : ApiHandle::stealReference(item));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  List list(&scope, *list_obj);
  if (i >= list.numItems()) {
    thread->raiseWithFmt(LayoutId::kIndexError,
                         "index out of bounds in PyList_SetItem");
    return -1;
  }
  list.atPut(i, *newitem);
  return 0;
}

PY_EXPORT int PyList_Append(PyObject* op, PyObject* newitem) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  if (newitem == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }
  Object value(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(newitem)));

  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(op)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  List list(&scope, *list_obj);

  runtime->listAdd(thread, list, value);
  return 0;
}

PY_EXPORT PyObject* PyList_GetSlice(PyObject* pylist, Py_ssize_t low,
                                    Py_ssize_t high) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  List list(&scope, *list_obj);
  word length = list.numItems();
  if (low < 0) {
    low = 0;
  } else if (low > length) {
    low = length;
  }
  if (high < low) {
    high = low;
  } else if (high > length) {
    high = length;
  }
  return ApiHandle::newReferenceWithManaged(
      runtime, listSlice(thread, list, low, high, 1));
}

PY_EXPORT int PyList_Insert(PyObject* pylist, Py_ssize_t where,
                            PyObject* item) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  if (item == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  List list(&scope, *list_obj);
  if (list.numItems() == kMaxWord) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "cannot add more objects to list");
    return -1;
  }
  Object item_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(item)));
  listInsert(thread, list, item_obj, where);
  return 0;
}

PY_EXPORT int PyList_SetSlice(PyObject* list, Py_ssize_t low, Py_ssize_t high,
                              PyObject* items) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(list)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }

  List self(&scope, *list_obj);
  word length = self.numItems();
  if (low < 0) {
    low = 0;
  } else if (low > length) {
    low = length;
  }
  if (high < low) {
    high = low;
  } else if (high > length) {
    high = length;
  }

  Object start(&scope, SmallInt::fromWord(low));
  Object stop(&scope, SmallInt::fromWord(high));
  Object step(&scope, NoneType::object());
  Slice slice(&scope, runtime->newSlice(start, stop, step));
  Object result(&scope, NoneType::object());
  if (items == nullptr) {
    // Equivalent to deleting slice
    result = thread->invokeMethodStatic2(LayoutId::kList, ID(__delitem__),
                                         list_obj, slice);
  } else {
    Object items_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(items)));
    result = thread->invokeMethodStatic3(LayoutId::kList, ID(__setitem__),
                                         list_obj, slice, items_obj);
  }
  return result.isError() ? -1 : 0;
}

PY_EXPORT Py_ssize_t PyList_Size(PyObject* p) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(p)));
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }

  List list(&scope, *list_obj);
  return list.numItems();
}

PY_EXPORT int PyList_Sort(PyObject* pylist) {
  Thread* thread = Thread::current();
  if (pylist == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }
  HandleScope scope(thread);
  Object list_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pylist)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfList(*list_obj)) {
    thread->raiseBadInternalCall();
    return -1;
  }
  List list(&scope, *list_obj);
  if (listSort(thread, list).isError()) {
    return -1;
  }
  return 0;
}

}  // namespace py
