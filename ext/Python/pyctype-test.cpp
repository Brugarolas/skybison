// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "Python.h"

#include "capi-fixture.h"
#include "capi-testing.h"

namespace py {
namespace testing {

using PyCTypeExtensionApiTest = ExtensionApi;

TEST_F(PyCTypeExtensionApiTest, IsAlnum) {
  EXPECT_TRUE(Py_ISALNUM('a'));
  EXPECT_TRUE(Py_ISALNUM('z'));
  EXPECT_TRUE(Py_ISALNUM('A'));
  EXPECT_TRUE(Py_ISALNUM('Z'));
  EXPECT_TRUE(Py_ISALNUM('0'));
  EXPECT_TRUE(Py_ISALNUM('9'));

  EXPECT_FALSE(Py_ISALNUM('\x00'));
  EXPECT_FALSE(Py_ISALNUM('\t'));
  EXPECT_FALSE(Py_ISALNUM('_'));
  EXPECT_FALSE(Py_ISALNUM('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsAlpha) {
  EXPECT_TRUE(Py_ISALPHA('a'));
  EXPECT_TRUE(Py_ISALPHA('z'));
  EXPECT_TRUE(Py_ISALPHA('A'));
  EXPECT_TRUE(Py_ISALPHA('Z'));

  EXPECT_FALSE(Py_ISALPHA('0'));
  EXPECT_FALSE(Py_ISALPHA('9'));
  EXPECT_FALSE(Py_ISALPHA('\x00'));
  EXPECT_FALSE(Py_ISALPHA('\t'));
  EXPECT_FALSE(Py_ISALPHA('_'));
  EXPECT_FALSE(Py_ISALPHA('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsDigit) {
  EXPECT_TRUE(Py_ISDIGIT('0'));
  EXPECT_TRUE(Py_ISDIGIT('9'));

  EXPECT_FALSE(Py_ISDIGIT('a'));
  EXPECT_FALSE(Py_ISDIGIT('z'));
  EXPECT_FALSE(Py_ISDIGIT('A'));
  EXPECT_FALSE(Py_ISDIGIT('Z'));
  EXPECT_FALSE(Py_ISDIGIT('\x00'));
  EXPECT_FALSE(Py_ISDIGIT('\t'));
  EXPECT_FALSE(Py_ISDIGIT('_'));
  EXPECT_FALSE(Py_ISDIGIT('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsLower) {
  EXPECT_TRUE(Py_ISLOWER('a'));
  EXPECT_TRUE(Py_ISLOWER('z'));

  EXPECT_FALSE(Py_ISLOWER('A'));
  EXPECT_FALSE(Py_ISLOWER('Z'));
  EXPECT_FALSE(Py_ISLOWER('0'));
  EXPECT_FALSE(Py_ISLOWER('9'));
  EXPECT_FALSE(Py_ISLOWER('\x00'));
  EXPECT_FALSE(Py_ISLOWER('\t'));
  EXPECT_FALSE(Py_ISLOWER('_'));
  EXPECT_FALSE(Py_ISLOWER('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsSpace) {
  EXPECT_TRUE(Py_ISSPACE('\t'));
  EXPECT_TRUE(Py_ISSPACE('\n'));
  EXPECT_TRUE(Py_ISSPACE('\v'));
  EXPECT_TRUE(Py_ISSPACE('\f'));
  EXPECT_TRUE(Py_ISSPACE('\r'));
  EXPECT_TRUE(Py_ISSPACE(' '));

  // These are spaces in Unicode but not in pyctype.c
  EXPECT_FALSE(Py_ISSPACE('\x1c'));
  EXPECT_FALSE(Py_ISSPACE('\x1d'));
  EXPECT_FALSE(Py_ISSPACE('\x1e'));
  EXPECT_FALSE(Py_ISSPACE('\x1f'));

  EXPECT_FALSE(Py_ISSPACE('a'));
  EXPECT_FALSE(Py_ISSPACE('z'));
  EXPECT_FALSE(Py_ISSPACE('A'));
  EXPECT_FALSE(Py_ISSPACE('Z'));
  EXPECT_FALSE(Py_ISSPACE('0'));
  EXPECT_FALSE(Py_ISSPACE('9'));
  EXPECT_FALSE(Py_ISSPACE('\x00'));
  EXPECT_FALSE(Py_ISSPACE('_'));
  EXPECT_FALSE(Py_ISSPACE('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsUpper) {
  EXPECT_TRUE(Py_ISUPPER('A'));
  EXPECT_TRUE(Py_ISUPPER('Z'));

  EXPECT_FALSE(Py_ISUPPER('a'));
  EXPECT_FALSE(Py_ISUPPER('z'));
  EXPECT_FALSE(Py_ISUPPER('0'));
  EXPECT_FALSE(Py_ISUPPER('9'));
  EXPECT_FALSE(Py_ISUPPER('\x00'));
  EXPECT_FALSE(Py_ISUPPER('\t'));
  EXPECT_FALSE(Py_ISUPPER('_'));
  EXPECT_FALSE(Py_ISUPPER('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, IsXDigit) {
  EXPECT_TRUE(Py_ISXDIGIT('a'));
  EXPECT_TRUE(Py_ISXDIGIT('f'));
  EXPECT_TRUE(Py_ISXDIGIT('A'));
  EXPECT_TRUE(Py_ISXDIGIT('F'));
  EXPECT_TRUE(Py_ISXDIGIT('0'));
  EXPECT_TRUE(Py_ISXDIGIT('9'));

  EXPECT_FALSE(Py_ISXDIGIT('g'));
  EXPECT_FALSE(Py_ISXDIGIT('z'));
  EXPECT_FALSE(Py_ISXDIGIT('G'));
  EXPECT_FALSE(Py_ISXDIGIT('Z'));
  EXPECT_FALSE(Py_ISXDIGIT('\x00'));
  EXPECT_FALSE(Py_ISXDIGIT('\t'));
  EXPECT_FALSE(Py_ISXDIGIT('_'));
  EXPECT_FALSE(Py_ISXDIGIT('\x80'));
}

TEST_F(PyCTypeExtensionApiTest, ToLower) {
  EXPECT_EQ(Py_TOLOWER('A'), 'a');
  EXPECT_EQ(Py_TOLOWER('Z'), 'z');

  EXPECT_EQ(Py_TOLOWER('a'), 'a');
  EXPECT_EQ(Py_TOLOWER('z'), 'z');
  EXPECT_EQ(Py_TOLOWER('0'), '0');
  EXPECT_EQ(Py_TOLOWER('9'), '9');
  EXPECT_EQ(Py_TOLOWER('\x00'), '\x00');
  EXPECT_EQ(Py_TOLOWER('\t'), '\t');
  EXPECT_EQ(Py_TOLOWER('_'), '_');
  EXPECT_EQ(Py_TOLOWER('\x80'), 0x80);
}

TEST_F(PyCTypeExtensionApiTest, ToUpper) {
  EXPECT_EQ(Py_TOUPPER('a'), 'A');
  EXPECT_EQ(Py_TOUPPER('z'), 'Z');

  EXPECT_EQ(Py_TOUPPER('A'), 'A');
  EXPECT_EQ(Py_TOUPPER('Z'), 'Z');
  EXPECT_EQ(Py_TOUPPER('0'), '0');
  EXPECT_EQ(Py_TOUPPER('9'), '9');
  EXPECT_EQ(Py_TOUPPER('\x00'), '\x00');
  EXPECT_EQ(Py_TOUPPER('\t'), '\t');
  EXPECT_EQ(Py_TOUPPER('_'), '_');
  EXPECT_EQ(Py_TOUPPER('\x80'), 0x80);
}

}  // namespace testing
}  // namespace py
