// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "capi-testing.h"

#include <cstring>
#include <memory>

#include "Python.h"
#include "gtest/gtest.h"

namespace py {
namespace testing {

Borrowed borrow(PyObject* obj) { return Borrowed(obj); }

void collectGarbage() {
  PyRun_SimpleString(R"(
try:
  from _builtins import _gc
  _gc()
except:
  pass
)");
}

PyObject* mainModuleGet(const char* name) {
  return moduleGet("__main__", name);
}

PyObject* moduleGet(const char* module, const char* name) {
  PyObject* mods = PyImport_GetModuleDict();
  PyObject* module_name = PyUnicode_FromString(module);
  PyObject* mod = PyDict_GetItem(mods, module_name);
  if (mod == nullptr) return nullptr;
  Py_DECREF(module_name);
  return PyObject_GetAttrString(mod, name);
}

int moduleSet(const char* module, const char* name, PyObject* value) {
  PyObject* mods = PyImport_GetModuleDict();
  PyObject* module_name = PyUnicode_FromString(module);
  PyObject* mod = PyDict_GetItem(mods, module_name);
  if (mod == nullptr && strcmp(module, "__main__") == 0) {
    // create __main__ if not yet available
    PyRun_SimpleString("");
    mod = PyDict_GetItem(mods, module_name);
  }
  if (mod == nullptr) return -1;
  Py_DECREF(module_name);

  PyObject* name_obj = PyUnicode_FromString(name);
  int ret = PyObject_SetAttr(mod, name_obj, value);
  Py_DECREF(name_obj);
  return ret;
}

PyObject* importGetModule(PyObject* name) {
  PyObject* modules_dict = PyImport_GetModuleDict();
  PyObject* module = PyDict_GetItem(modules_dict, name);
  Py_XINCREF(module);  // Return a new reference
  return module;
}

template <typename T>
static ::testing::AssertionResult failNullObj(const T& expected,
                                              const char* delim) {
  PyObjectPtr exception(PyErr_Occurred());
  Py_INCREF(exception);
  if (exception != nullptr) {
    PyErr_Clear();
    PyObjectPtr exception_repr(PyObject_Repr(exception));
    if (exception_repr != nullptr) {
      const char* exception_cstr = PyUnicode_AsUTF8(exception_repr);
      if (exception_cstr != nullptr) {
        return ::testing::AssertionFailure()
               << "pending exception: " << exception_cstr;
      }
    }
  }
  return ::testing::AssertionFailure()
         << "nullptr is not equal to " << delim << expected << delim;
}

template <typename T>
static ::testing::AssertionResult failBadValue(PyObject* obj, const T& expected,
                                               const char* delim) {
  PyObjectPtr repr_str(PyObject_Repr(obj));
  const char* repr_cstr = nullptr;
  if (repr_str != nullptr) {
    repr_cstr = PyUnicode_AsUTF8(repr_str);
  }
  repr_cstr = repr_cstr == nullptr ? "NULL" : repr_cstr;
  return ::testing::AssertionFailure()
         << repr_cstr << " is not equal to " << delim << expected << delim;
}

::testing::AssertionResult isBytesEqualsCStr(PyObject* obj, const char* c_str) {
  if (obj == nullptr) return failNullObj(c_str, "'");

  if (!PyBytes_Check(obj) || std::strcmp(PyBytes_AsString(obj), c_str) != 0) {
    return failBadValue(obj, c_str, "'");
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isLongEqualsLong(PyObject* obj, long value) {
  if (obj == nullptr) return failNullObj(value, "");

  if (PyLong_Check(obj)) {
    long longval = PyLong_AsLong(obj);
    if (longval == -1 && PyErr_Occurred() != nullptr) {
      PyErr_Clear();
    } else if (longval == value) {
      return ::testing::AssertionSuccess();
    }
  }

  return failBadValue(obj, value, "");
}

::testing::AssertionResult isUnicodeEqualsCStr(PyObject* obj,
                                               const char* c_str) {
  if (obj == nullptr) return failNullObj(c_str, "'");

  if (!PyUnicode_Check(obj)) {
    return failBadValue(obj, c_str, "'");
  }
  PyObjectPtr expected(PyUnicode_FromString(c_str));
  if (PyUnicode_Compare(obj, expected) != 0) {
    return failBadValue(obj, c_str, "'");
  }
  return ::testing::AssertionSuccess();
}

CaptureStdStreams::CaptureStdStreams() {
  ::testing::internal::CaptureStdout();
  ::testing::internal::CaptureStderr();
}

CaptureStdStreams::~CaptureStdStreams() {
  // Print any unread buffers to their respective streams to assist in
  // debugging.
  if (!restored_stdout_) std::cout << out();
  if (!restored_stderr_) std::cerr << err();
}

std::string CaptureStdStreams::out() {
  assert(!restored_stdout_);
  PyObject *exc, *value, *tb;
  PyErr_Fetch(&exc, &value, &tb);
  PyRun_SimpleString(R"(
import sys
if hasattr(sys, "stdout") and hasattr(sys.stdout, "flush"):
  sys.stdout.flush()
)");
  PyErr_Restore(exc, value, tb);
  restored_stdout_ = true;
  return ::testing::internal::GetCapturedStdout();
}

std::string CaptureStdStreams::err() {
  assert(!restored_stderr_);
  PyObject *exc, *value, *tb;
  PyErr_Fetch(&exc, &value, &tb);
  PyRun_SimpleString(R"(
import sys
if hasattr(sys, "stderr") and hasattr(sys.stderr, "flush"):
  sys.stderr.flush()
)");
  PyErr_Restore(exc, value, tb);
  restored_stderr_ = true;
  return ::testing::internal::GetCapturedStderr();
}

TempDirectory::TempDirectory() : TempDirectory("PYRO_TEST") {}

TempDirectory::TempDirectory(const char* prefix) {
  const char* tmpdir = std::getenv("TMPDIR");
  if (tmpdir == nullptr) {
    tmpdir = "/tmp/";
  }
  const char* format = "%s%s.XXXXXXXX";
  int length = std::snprintf(nullptr, 0, format, tmpdir, prefix);

  std::unique_ptr<char[]> buffer(new char[length]);
  std::snprintf(buffer.get(), length, format, tmpdir, prefix);
  char* result(::mkdtemp(buffer.get()));
  assert(result != nullptr);
  path_ = result;
  assert(!path_.empty());
  if (path_.back() != '/') path_ += "/";
}

TempDirectory::~TempDirectory() {
  std::string cleanup = "rm -rf " + path_;
  int result = system(cleanup.c_str());
  (void)result;
  assert(result == 0);
}

}  // namespace testing
}  // namespace py
