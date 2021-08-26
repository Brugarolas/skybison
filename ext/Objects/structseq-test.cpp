// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "Python.h"
#include "gtest/gtest.h"

#include "capi-fixture.h"
#include "capi-testing.h"

namespace py {
namespace testing {

using StructSeqExtensionApiTest = ExtensionApi;

PyStructSequence_Field desc_fields[] = {
    {"first", "first field"}, {"second", "second field"},
    {"third", "third field"}, {"fourth", "fourth field"},
    {"fifth", "fifth field"}, {nullptr}};

PyStructSequence_Desc desc = {"foo.bar", "docs", desc_fields, 2};

TEST_F(StructSeqExtensionApiTest, NewTypeCreatesRuntimeType) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr module(PyObject_GetAttrString(type, "__module__"));
  EXPECT_TRUE(isUnicodeEqualsCStr(module, "foo"));
  PyObjectPtr name(PyObject_GetAttrString(type, "__name__"));
  EXPECT_TRUE(isUnicodeEqualsCStr(name, "bar"));
  PyObjectPtr qualname(PyObject_GetAttrString(type, "__qualname__"));
  EXPECT_TRUE(isUnicodeEqualsCStr(qualname, "bar"));

  PyObjectPtr seq_attr1(PyObject_GetAttrString(type, "n_sequence_fields"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_NE(seq_attr1, nullptr);
  EXPECT_EQ(PyLong_AsLong(seq_attr1), 2);

  PyObjectPtr seq_attr2(PyObject_GetAttrString(type, "n_unnamed_fields"));
  ASSERT_NE(seq_attr2, nullptr);
  EXPECT_EQ(PyLong_AsLong(seq_attr2), 0);

  PyObjectPtr seq_attr3(PyObject_GetAttrString(type, "n_fields"));
  ASSERT_NE(seq_attr3, nullptr);
  EXPECT_EQ(PyLong_AsLong(seq_attr3), 5);
}

TEST_F(StructSeqExtensionApiTest, NewTypeWithUnnamedFieldsReturnsType) {
  PyStructSequence_Field fields[] = {
      {const_cast<char*>("foo"), const_cast<char*>("foo docu")},
      {PyStructSequence_UnnamedField, const_cast<char*>("unnamed docu")},
      {const_cast<char*>("bar"), const_cast<char*>("bar docu")},
      {PyStructSequence_UnnamedField, const_cast<char*>("unnamed docu")},
      {const_cast<char*>("baz"), const_cast<char*>("baz docu")},
      {nullptr}};
  PyStructSequence_Desc s_desc = {const_cast<char*>("S"),
                                  const_cast<char*>("S docu"), fields, 4};
  PyObjectPtr type(PyStructSequence_NewType(&s_desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr n_unnamed_fields(
      PyObject_GetAttrString(type, "n_unnamed_fields"));
  EXPECT_TRUE(isLongEqualsLong(n_unnamed_fields, 2));
  PyObjectPtr n_fields(PyObject_GetAttrString(type, "n_fields"));
  EXPECT_TRUE(isLongEqualsLong(n_fields, 5));
  PyObjectPtr n_sequence_fields(
      PyObject_GetAttrString(type, "n_sequence_fields"));
  EXPECT_TRUE(isLongEqualsLong(n_sequence_fields, 4));
}

TEST_F(StructSeqExtensionApiTest, SETITEMOnlyDecrefsOnce) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  PyObjectPtr seq(PyStructSequence_New(type.asTypeObject()));
  PyObject* value = PyUnicode_FromString("my_unique_string");
  Py_ssize_t refcnt = Py_REFCNT(value);
  PyStructSequence_SET_ITEM(seq.get(), 0, value);
  // Pyro will have refcount of 1 less than CPython
  EXPECT_LE(Py_REFCNT(value), refcnt);
}

TEST_F(StructSeqExtensionApiTest, GetItem) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2))
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_TRUE(PyTuple_Check(result));

  PyObject* value = PyStructSequence_GetItem(result, 1);
  ASSERT_TRUE(PyLong_Check(value));
  EXPECT_EQ(PyLong_AsLong(value), 2);

  PyObjectPtr value2(PyObject_GetAttrString(result, "second"));
  ASSERT_TRUE(PyLong_Check(value2));
  EXPECT_EQ(PyLong_AsLong(value2), 2);

  EXPECT_EQ(value, value2);
}

TEST_F(StructSeqExtensionApiTest, GetItemWithIndexReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2))[0]
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyLong_Check(result));
  EXPECT_EQ(PyLong_AsLong(result), 1);
}

TEST_F(StructSeqExtensionApiTest,
       GetItemWithIndexToHiddenValueRaisesException) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  // TODO(T40700664): Use PyRun_String and test for raised exception
  EXPECT_EQ(PyRun_SimpleString(R"(
import sys
sys.excepthook = lambda *args: None
Structseq((1,2,3))[2]
)"),
            -1);
}

TEST_F(StructSeqExtensionApiTest, GetItemWithNameReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2)).first
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyLong_Check(result));
  EXPECT_EQ(PyLong_AsLong(result), 1);
}

TEST_F(StructSeqExtensionApiTest, GetItemWithNameToHiddenValueReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2,3)).third
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyLong_Check(result));
  EXPECT_EQ(PyLong_AsLong(result), 3);
}

TEST_F(StructSeqExtensionApiTest,
       GetItemWithNameToUnsetHiddenValueReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2,3)).fifth
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(result, Py_None);
}

TEST_F(StructSeqExtensionApiTest, GetItemWithDictAndInvalidFieldReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = Structseq((1,2), {"badattr": 3}).first
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_TRUE(PyLong_Check(result));
  EXPECT_EQ(PyLong_AsLong(result), 1);
}

TEST_F(StructSeqExtensionApiTest,
       GetItemFromDictWithInvalidFieldRaisesException) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  // TODO(T40700664): Use PyRun_String and test for raised exception
  EXPECT_EQ(PyRun_SimpleString(R"(
import sys
sys.excepthook = lambda *args: None
Structseq((1,2), {"badattr": 3}).badattr
)"),
            -1);
}

TEST_F(StructSeqExtensionApiTest, LenReturnsVisibleSize) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
result = len(Structseq((1,2,3)))
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyLong_Check(result));
  EXPECT_EQ(PyLong_AsLong(result), 2);
}

TEST_F(StructSeqExtensionApiTest, IterReturnsVisibleItems) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  PyRun_SimpleString(R"(
structseq = Structseq((1,2,3,4,5))
result = [x for x in structseq]
)");
  PyObjectPtr result(mainModuleGet("result"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyList_Check(result));
  EXPECT_EQ(PyList_Size(result), 2);
  EXPECT_EQ(PyLong_AsLong(PyList_GetItem(result, 0)), 1);
  EXPECT_EQ(PyLong_AsLong(PyList_GetItem(result, 1)), 2);
}

TEST_F(StructSeqExtensionApiTest, SetItemRaisesException) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  ASSERT_EQ(moduleSet("__main__", "Structseq", type), 0);
  // TODO(T40700664): Use PyRun_String and test for raised exception
  EXPECT_EQ(PyRun_SimpleString(R"(
import sys
sys.excepthook = lambda *args: None
structseq = Structseq((1,2,3))
structseq.first = 4
)"),
            -1);
}

TEST_F(StructSeqExtensionApiTest, TupleSizeReturnsVisibleSize) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));
  EXPECT_EQ(PyTuple_Size(instance), 2);
}

TEST_F(StructSeqExtensionApiTest, GetItemReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));

  PyObject* value = PyLong_FromLong(123);  // reference will be stolen
  EXPECT_EQ(PyStructSequence_SET_ITEM(instance.get(), 0, value), value);
  ASSERT_EQ(PyErr_Occurred(), nullptr);

  PyObject* result = PyStructSequence_GET_ITEM(instance.get(), 0);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(PyLong_AsLong(result), 123);
}

TEST_F(StructSeqExtensionApiTest,
       GetItemFromUninitializedFieldReturnsNonePyro) {
  // Pyro only test as CPython initializes these to nullptr
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));

  PyObject* result = PyStructSequence_GET_ITEM(instance.get(), 0);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(result, Py_None);
}

TEST_F(StructSeqExtensionApiTest, GetItemHiddenFieldReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));

  PyStructSequence_SetItem(instance, 4, PyLong_FromLong(123));
  ASSERT_EQ(PyErr_Occurred(), nullptr);

  PyObject* result = PyStructSequence_GetItem(instance.get(), 4);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(PyLong_AsLong(result), 123);
}

TEST_F(StructSeqExtensionApiTest, GetNamedItemReturnsValue) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));

  PyStructSequence_SetItem(instance, 0, PyLong_FromLong(123));
  ASSERT_EQ(PyErr_Occurred(), nullptr);

  PyObjectPtr result(PyObject_GetAttrString(instance, "first"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(PyLong_AsLong(result), 123);
}

TEST_F(StructSeqExtensionApiTest,
       GetNamedItemFromUninitializedFieldReturnsNone) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  ASSERT_TRUE(PyType_CheckExact(type));

  PyObjectPtr instance(PyStructSequence_New(type.asTypeObject()));
  ASSERT_TRUE(PyTuple_Check(instance));

  PyObjectPtr result(PyObject_GetAttrString(instance, "first"));
  ASSERT_EQ(PyErr_Occurred(), nullptr);
  EXPECT_EQ(result, Py_None);
}

TEST_F(StructSeqExtensionApiTest, GetSlotNewOnStructSeqReturnsSlot) {
  PyObjectPtr type(PyStructSequence_NewType(&desc));
  ASSERT_NE(type, nullptr);

  auto slot_new =
      reinterpret_cast<newfunc>(PyType_GetSlot(type.asTypeObject(), Py_tp_new));
  ASSERT_NE(slot_new, nullptr);
  PyObjectPtr tuple(PyTuple_New(3));
  PyTuple_SetItem(tuple, 0, PyLong_FromLong(111));
  PyTuple_SetItem(tuple, 1, PyLong_FromLong(222));
  PyTuple_SetItem(tuple, 2, PyLong_FromLong(333));
  PyObjectPtr args(PyTuple_Pack(1, tuple.get()));
  PyObjectPtr seq(slot_new(type.asTypeObject(), args, nullptr));
  ASSERT_NE(seq, nullptr);
  ASSERT_EQ(PyObject_IsInstance(seq, type), 1);
  EXPECT_TRUE(isLongEqualsLong(PyStructSequence_GetItem(seq, 0), 111));
  EXPECT_TRUE(isLongEqualsLong(PyStructSequence_GetItem(seq, 1), 222));
  PyObjectPtr third(PyObject_GetAttrString(seq, "third"));
  EXPECT_TRUE(isLongEqualsLong(third, 333));
}

}  // namespace testing
}  // namespace py
