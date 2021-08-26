// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "api-handle.h"
#include "runtime.h"

namespace py {

PY_EXPORT int PyODict_DelItem(PyObject* /* d */, PyObject* /* y */) {
  UNIMPLEMENTED("PyODict_DelItem");
}

PY_EXPORT PyObject* PyODict_New() { UNIMPLEMENTED("PyODict_New"); }

PY_EXPORT int PyODict_SetItem(PyObject* /* d */, PyObject* /* y */,
                              PyObject* /* e */) {
  UNIMPLEMENTED("PyODict_SetItem");
}

}  // namespace py
