// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "Python.h"
#include "gtest/gtest.h"

#include "capi-fixture.h"
#include "capi-testing.h"

namespace py {
namespace testing {

using SignalModuleApiTest = ExtensionApi;

TEST_F(SignalModuleApiTest, PyErrSetInterruptTriggersInterrupt) {
  PyErr_SetInterrupt();
  EXPECT_EQ(PyErr_Occurred(), nullptr);
  PyObject* main(PyImport_AddModule("__main__"));
  ASSERT_NE(main, nullptr);
  PyObject* globals(PyModule_GetDict(main));
  PyObjectPtr result(PyRun_String("pass", Py_file_input, globals, globals));
  EXPECT_EQ(result.get(), nullptr);
  EXPECT_NE(PyErr_Occurred(), nullptr);
  EXPECT_TRUE(PyErr_ExceptionMatches(PyExc_KeyboardInterrupt));
  PyErr_Clear();
}

TEST_F(SignalModuleApiTest, PyErrCheckSignalsReturnsZero) {
  EXPECT_EQ(PyErr_CheckSignals(), 0);
}

TEST_F(SignalModuleApiTest, PyErrCheckSignalsRunsSignalHandlers) {
  PyErr_SetInterrupt();
  EXPECT_EQ(PyErr_CheckSignals(), -1);
  EXPECT_NE(PyErr_Occurred(), nullptr);
  EXPECT_TRUE(PyErr_ExceptionMatches(PyExc_KeyboardInterrupt));
  PyErr_Clear();
}

}  // namespace testing
}  // namespace py
