// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "float-builtins.h"

#include <limits>

#include "gtest/gtest.h"

#include "builtins.h"
#include "handles.h"
#include "int-builtins.h"
#include "objects.h"
#include "runtime.h"
#include "test-utils.h"

namespace py {
namespace testing {

using FloatBuiltinsTest = testing::RuntimeFixture;

TEST(FloatBuiltinsTestNoFixture,
     DecodeDoubleWithPositiveDoubleReturnsIsNegFalse) {
  double input = 100.0;
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(input, &is_neg, &exp, &mantissa);
  EXPECT_EQ(is_neg, false);
}

TEST(FloatBuiltinsTestNoFixture,
     DecodeDoubleWithNegativeDoubleReturnsIsNegTrue) {
  double input = -100.0;
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(input, &is_neg, &exp, &mantissa);
  EXPECT_EQ(is_neg, true);
}

TEST(FloatBuiltinsTestNoFixture,
     DecodeDoubleWithMaximumExponentReturnsCorrectValue) {
  double input = std::strtod("0x1.0p+1024", nullptr);
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(input, &is_neg, &exp, &mantissa);
  EXPECT_EQ(exp, 1024);
}

TEST(FloatBuiltinsTestNoFixture,
     DecodeDoubleWithMinimumExponentReturnsCorrectValue) {
  double input = std::strtod("0x1.0p-1023", nullptr);
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(input, &is_neg, &exp, &mantissa);
  EXPECT_EQ(exp, -1023);
}

TEST(FloatBuiltinsTestNoFixture, DecodeDoubleWithMantissaReturnsCorrectValue) {
  double input = std::strtod("0x1.29ef685b3f6fbp+52", nullptr);
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(input, &is_neg, &exp, &mantissa);
  EXPECT_EQ(mantissa, 0x29ef685b3f6fb);
}

TEST_F(FloatBuiltinsTest, DunderMulWithDoubleReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(2.0));
  Float right(&scope, runtime_->newFloat(1.5));
  Object result(&scope, runBuiltin(METH(float, __mul__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 3.0));
}

TEST_F(FloatBuiltinsTest, DunderMulWithSmallIntReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(2.5));
  Int right(&scope, runtime_->newInt(1));
  Object result(&scope, runBuiltin(METH(float, __mul__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 2.5));
}

TEST_F(FloatBuiltinsTest, DunderMulWithNonFloatSelfRaisesTypeError) {
  HandleScope scope(thread_);
  Object left(&scope, NoneType::object());
  Float right(&scope, runtime_->newFloat(1.0));
  Object result(&scope, runBuiltin(METH(float, __mul__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kTypeError));
}

TEST_F(FloatBuiltinsTest, DunderMulWithNonFloatOtherReturnsNotImplemented) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Object right(&scope, NoneType::object());
  Object result(&scope, runBuiltin(METH(float, __mul__), left, right));
  EXPECT_TRUE(result.isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderNeWithInequalFloatsReturnsTrue) {
  ASSERT_FALSE(
      runFromCStr(runtime_, "result = float.__ne__(12.2, 2.12)").isError());
  EXPECT_EQ(mainModuleAt(runtime_, "result"), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderNeWithEqualFloatIntReturnsFalse) {
  ASSERT_FALSE(
      runFromCStr(runtime_, "result = float.__ne__(34.0, 34)").isError());
  EXPECT_EQ(mainModuleAt(runtime_, "result"), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderNeWithStringReturnsNotImplemented) {
  ASSERT_FALSE(
      runFromCStr(runtime_, "result = float.__ne__(5.5, '')").isError());
  EXPECT_TRUE(mainModuleAt(runtime_, "result").isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderAbsZeroReturnsZero) {
  HandleScope scope(thread_);
  Float self(&scope, runtime_->newFloat(0.0));
  Object result(&scope, runBuiltin(METH(float, __abs__), self));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 0.0));
}

TEST_F(FloatBuiltinsTest, DunderAbsNegativeReturnsPositive) {
  HandleScope scope(thread_);
  Float self(&scope, runtime_->newFloat(-1234.0));
  Object result(&scope, runBuiltin(METH(float, __abs__), self));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 1234.0));
}

TEST_F(FloatBuiltinsTest, DunderAbsPositiveReturnsPositive) {
  HandleScope scope(thread_);
  Float self(&scope, runtime_->newFloat(5678.0));
  Object result(&scope, runBuiltin(METH(float, __abs__), self));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 5678.0));
}

TEST_F(FloatBuiltinsTest, BinaryAddDouble) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
a = 2.0
b = 1.5
c = a + b
)")
                   .isError());

  Object c(&scope, mainModuleAt(runtime_, "c"));
  EXPECT_TRUE(isFloatEqualsDouble(*c, 3.5));
}

TEST_F(FloatBuiltinsTest, BinaryAddSmallInt) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
a = 2.5
b = 1
c = a + b
)")
                   .isError());

  Object c(&scope, mainModuleAt(runtime_, "c"));
  EXPECT_TRUE(isFloatEqualsDouble(*c, 3.5));
}

TEST_F(FloatBuiltinsTest, AddWithNonFloatOtherRaisesTypeError) {
  const char* src = R"(
1.0 + None
)";
  EXPECT_TRUE(raisedWithStr(runFromCStr(runtime_, src), LayoutId::kTypeError,
                            "float.__add__(NoneType) is not supported"));
}

TEST_F(FloatBuiltinsTest, DunderAddWithFloatSubclassReturnsFloatSum) {
  HandleScope scope(thread_);
  EXPECT_FALSE(runFromCStr(runtime_, R"(
class SubFloat(float):
  pass

left = SubFloat(1.0)
right = SubFloat(2.0)
)")
                   .isError());
  Object left(&scope, mainModuleAt(runtime_, "left"));
  Object right(&scope, mainModuleAt(runtime_, "right"));
  Object result(&scope, runBuiltin(METH(float, __add__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 3.0));
}

TEST_F(FloatBuiltinsTest, DunderBoolWithZeroReturnsFalse) {
  HandleScope scope(thread_);
  Float self(&scope, runtime_->newFloat(0.0));
  Object result(&scope, runBuiltin(METH(float, __bool__), self));
  EXPECT_EQ(*result, Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderBoolWithNonZeroReturnsTrue) {
  HandleScope scope(thread_);
  Float self(&scope, runtime_->newFloat(1234.0));
  Object result(&scope, runBuiltin(METH(float, __bool__), self));
  EXPECT_EQ(*result, Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithDoubleReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(3.0));
  Float right(&scope, runtime_->newFloat(2.0));
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 1.5));
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithSmallIntReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(3.0));
  Int right(&scope, runtime_->newInt(2));
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 1.5));
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithNonFloatSelfRaisesTypeError) {
  HandleScope scope(thread_);
  Object left(&scope, NoneType::object());
  Float right(&scope, runtime_->newFloat(1.0));
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kTypeError));
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithNonFloatOtherReturnsNotImplemented) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Object right(&scope, NoneType::object());
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(result.isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithZeroFloatRaisesZeroDivisionError) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Float right(&scope, runtime_->newFloat(0.0));
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kZeroDivisionError));
}

TEST_F(FloatBuiltinsTest,
       DunderTrueDivWithZeroSmallIntRaisesZeroDivisionError) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Int right(&scope, runtime_->newInt(0));
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kZeroDivisionError));
}

TEST_F(FloatBuiltinsTest, DunderTrueDivWithZeroBoolRaisesZeroDivisionError) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Bool right(&scope, RawBool::falseObj());
  Object result(&scope, runBuiltin(METH(float, __truediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kZeroDivisionError));
}

TEST_F(FloatBuiltinsTest, DunderRtruedivWithDoubleReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(2.0));
  Float right(&scope, runtime_->newFloat(3.0));
  Object result(&scope, runBuiltin(METH(float, __rtruediv__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 1.5));
}

TEST_F(FloatBuiltinsTest, DunderRtruedivWithSmallIntReturnsDouble) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(2.0));
  Int right(&scope, runtime_->newInt(3));
  Object result(&scope, runBuiltin(METH(float, __rtruediv__), left, right));
  EXPECT_TRUE(isFloatEqualsDouble(*result, 1.5));
}

TEST_F(FloatBuiltinsTest, DunderRtruedivWithNonFloatSelfRaisesTypeError) {
  HandleScope scope(thread_);
  Object left(&scope, NoneType::object());
  Float right(&scope, runtime_->newFloat(1.0));
  Object result(&scope, runBuiltin(METH(float, __rtruediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kTypeError));
}

TEST_F(FloatBuiltinsTest,
       DunderRtruedivWithNonFloatOtherReturnsNotImplemented) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(1.0));
  Object right(&scope, NoneType::object());
  Object result(&scope, runBuiltin(METH(float, __rtruediv__), left, right));
  EXPECT_TRUE(result.isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderRtruedivWithZeroFloatRaisesZeroDivisionError) {
  HandleScope scope(thread_);
  Float left(&scope, runtime_->newFloat(0.0));
  Float right(&scope, runtime_->newFloat(1.0));
  Object result(&scope, runBuiltin(METH(float, __rtruediv__), left, right));
  EXPECT_TRUE(raised(*result, LayoutId::kZeroDivisionError));
}

TEST_F(FloatBuiltinsTest, BinarySubtractDouble) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
a = 2.0
b = 1.5
c = a - b
)")
                   .isError());

  Object c(&scope, mainModuleAt(runtime_, "c"));
  EXPECT_TRUE(isFloatEqualsDouble(*c, 0.5));
}

TEST_F(FloatBuiltinsTest, BinarySubtractSmallInt) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
a = 2.5
b = 1
c = a - b
)")
                   .isError());

  Object c(&scope, mainModuleAt(runtime_, "c"));
  EXPECT_TRUE(isFloatEqualsDouble(*c, 1.5));
}

TEST_F(FloatBuiltinsTest, FloatSubclassKeepsFloatInMro) {
  const char* src = R"(
class Test(float):
  pass
)";
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, src).isError());
  Object value(&scope, mainModuleAt(runtime_, "Test"));
  ASSERT_TRUE(value.isType());

  Type type(&scope, *value);
  ASSERT_TRUE(type.mro().isTuple());

  Tuple mro(&scope, type.mro());
  ASSERT_EQ(mro.length(), 3);
  EXPECT_EQ(mro.at(0), *type);
  EXPECT_EQ(mro.at(1), runtime_->typeAt(LayoutId::kFloat));
  EXPECT_EQ(mro.at(2), runtime_->typeAt(LayoutId::kObject));
}

TEST_F(FloatBuiltinsTest, PowFloatAndFloat) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
base = 2.0
x = base ** 4.0
)")
                   .isError());
  EXPECT_TRUE(isFloatEqualsDouble(mainModuleAt(runtime_, "x"), 16.0));
}

TEST_F(FloatBuiltinsTest, PowFloatAndInt) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
base = 2.0
x = base ** 4
)")
                   .isError());
  EXPECT_TRUE(isFloatEqualsDouble(mainModuleAt(runtime_, "x"), 16.0));
}

TEST_F(FloatBuiltinsTest, InplacePowFloatAndFloat) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
x = 2.0
x **= 4.0
)")
                   .isError());
  EXPECT_TRUE(isFloatEqualsDouble(mainModuleAt(runtime_, "x"), 16.0));
}

TEST_F(FloatBuiltinsTest, InplacePowFloatAndInt) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
x = 2.0
x **= 4
)")
                   .isError());
  EXPECT_TRUE(isFloatEqualsDouble(mainModuleAt(runtime_, "x"), 16.0));
}

TEST_F(FloatBuiltinsTest, SubWithNonFloatOtherRaisesTypeError) {
  const char* src = R"(
1.0 - None
)";
  EXPECT_TRUE(raisedWithStr(runFromCStr(runtime_, src), LayoutId::kTypeError,
                            "float.__sub__(NoneType) is not supported"));
}

TEST_F(FloatBuiltinsTest, DunderEqWithFloatsReturnsBool) {
  HandleScope scope(thread_);
  Object nan(&scope, runtime_->newFloat(kDoubleNaN));
  Object f0(&scope, runtime_->newFloat(1.0));
  Object f1(&scope, runtime_->newFloat(-42.5));
  Object zero(&scope, runtime_->newFloat(0.0));
  Object neg_zero(&scope, runtime_->newFloat(-0.0));
  Object null(&scope, runtime_->newInt(0));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), f0, f0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), f0, f1), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), nan, nan), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), zero, neg_zero), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), neg_zero, null), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithIntSubclassReturnsBool) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C(int): pass
zero = C()
one = C(1)
two = C(2)
)")
                   .isError());
  Object self(&scope, runtime_->newFloat(1.0));
  Object zero(&scope, mainModuleAt(runtime_, "zero"));
  Object one(&scope, mainModuleAt(runtime_, "one"));
  Object two(&scope, mainModuleAt(runtime_, "two"));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), self, zero), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), self, one), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), self, two), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithSmallIntExactReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(31.0));
  Object float1(&scope, runtime_->newFloat(31.125));
  Object int0(&scope, runtime_->newFloat(31));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), float0, int0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), float1, int0), Bool::falseObj());
  word mantissa_max = (word{1} << (kDoubleMantissaBits + 1)) - 1;
  Object max_float(&scope,
                   runtime_->newFloat(static_cast<double>(mantissa_max)));
  Object max_int(&scope, runtime_->newFloat(mantissa_max));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), max_float, max_int),
            Bool::trueObj());
  Object neg_max_float(&scope,
                       runtime_->newFloat(static_cast<double>(-mantissa_max)));
  Object neg_max_int(&scope, runtime_->newInt(-mantissa_max));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), neg_max_float, neg_max_int),
            Bool::trueObj());

  word big0 = word(1) << (kDoubleMantissaBits + 2);
  ASSERT_EQ(static_cast<double>(big0), static_cast<double>(big0) + 1.0);
  Object big0_float(&scope, runtime_->newFloat(static_cast<double>(big0)));
  Int big0_int(&scope, runtime_->newInt(big0));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), big0_float, big0_int),
            Bool::trueObj());

  word big1 = (word(1) << (kDoubleMantissaBits + 1)) | (word(1) << 11);
  ASSERT_EQ(static_cast<double>(big1), static_cast<double>(big1) + 1.0);
  Object big1_float(&scope, runtime_->newFloat(static_cast<double>(big1)));
  Int big1_int(&scope, runtime_->newInt(big1));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), big1_float, big1_int),
            Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithSmallIntInexactReturnsFalse) {
  HandleScope scope(thread_);
  word big = (word(1) << (kDoubleMantissaBits + 4)) + 3;
  ASSERT_EQ(static_cast<double>(big), static_cast<double>(big) + 3.0);
  Object big_float(&scope, runtime_->newFloat(static_cast<double>(big)));
  Int big_int(&scope, runtime_->newInt(big));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), big_float, big_int),
            Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithLargeIntExactReturnsTrue) {
  HandleScope scope(thread_);
  const uword digits[] = {0, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), float0, int0), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithLargeIntInexactReturnsFalse) {
  HandleScope scope(thread_);
  const uword digits[] = {0x800, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  ASSERT_EQ(Float::cast(runBuiltin(METH(int, __float__), int0)).value(),
            Float::cast(*float0).value());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), float0, int0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithNonFiniteFloatIntReturnsFalse) {
  HandleScope scope(thread_);
  Object nan(&scope, runtime_->newFloat(kDoubleNaN));
  Object inf(&scope,
             runtime_->newFloat(std::numeric_limits<double>::infinity()));
  Object int0(&scope, runtime_->newInt(7));
  uword digits[100] = {};
  digits[99] = 1;
  Object int1(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), nan, int0), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), inf, int0), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), nan, int1), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __eq__), inf, int1), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderEqWithFloatOverflowingIntReturnsFalse) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(8.25));
  uword digits[100] = {};
  digits[99] = 1;
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __eq__), float0, int0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderFloatWithFloatLiteralReturnsSameObject) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, "a = (7.0).__float__()").isError());
  Object a(&scope, mainModuleAt(runtime_, "a"));
  EXPECT_TRUE(isFloatEqualsDouble(*a, 7.0));
}

TEST_F(FloatBuiltinsTest, DunderFloatFromFloatClassReturnsSameValue) {
  HandleScope scope(thread_);

  Float a_float(&scope, runtime_->newFloat(7.0));
  Object a(&scope, runBuiltin(METH(float, __float__), a_float));
  EXPECT_TRUE(isFloatEqualsDouble(*a, 7.0));
}

TEST_F(FloatBuiltinsTest, DunderFloatWithFloatSubclassReturnsSameValue) {
  HandleScope scope(thread_);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
class FloatSub(float):
  pass
a = FloatSub(1.0).__float__())")
                   .isError());
  Object a(&scope, mainModuleAt(runtime_, "a"));
  EXPECT_TRUE(isFloatEqualsDouble(*a, 1.0));
}

TEST_F(FloatBuiltinsTest, DunderFloatWithNonFloatReturnsError) {
  HandleScope scope(thread_);

  Int i(&scope, runtime_->newInt(1));
  Object i_res(&scope, runBuiltin(METH(float, __float__), i));
  EXPECT_TRUE(raised(*i_res, LayoutId::kTypeError));
}

TEST_F(FloatBuiltinsTest, DunderGeWithFloatReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(1.7));
  Object float1(&scope, runtime_->newFloat(0.2));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, float1), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, float0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float1, float0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithIntSelfNanReturnsFalse) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(kDoubleNaN));
  const uword digits[] = {0, 1};
  Object right(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), left, right), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithNonFloatReturnsNotImplemented) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(0.0));
  Object right(&scope, Str::empty());
  EXPECT_TRUE(
      runBuiltin(METH(float, __ge__), left, right).isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderGeWithSmallIntReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(5.0));
  Object int0(&scope, runtime_->newInt(4));
  Object int1(&scope, runtime_->newInt(5));
  Object int2(&scope, runtime_->newInt(6));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int1), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int2), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithSmallIntExactReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(44.));
  Object int0(&scope, runtime_->newInt(44));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::trueObj());
  Object float1(&scope, runtime_->newFloat(-3.));
  Object int1(&scope, runtime_->newInt(1));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float1, int1), Bool::falseObj());

  Object float2(&scope, runtime_->newFloat(0x20000000000000));
  Object int2(&scope, runtime_->newInt(0x20000000000000));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float2, int2), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithSmallIntInexactReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(0x20000000000001));
  Object int0(&scope, runtime_->newInt(0x20000000000001));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::falseObj());
  Object float1(&scope, runtime_->newFloat(0x20000000000003));
  Object int1(&scope, runtime_->newInt(0x20000000000003));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float1, int1), Bool::trueObj());
  Object float2(&scope, runtime_->newFloat(0x100000000000011));
  Object int2(&scope, runtime_->newInt(0x100000000000011));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float2, int2), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithLargeIntDifferingSignReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(-1.0));
  const uword digits0[] = {0, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits0));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::falseObj());
  Object float1(&scope, runtime_->newFloat(1.0));
  const uword digits1[] = {0, kMaxUword};
  Object int1(&scope, runtime_->newLargeIntWithDigits(digits1));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float1, int1), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithLargeIntExactEqualsReturnsTrue) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {0, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithLargeIntRoundingDownReturnsFalse) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {1, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  ASSERT_EQ(Float::cast(runBuiltin(METH(int, __float__), int0)).value(),
            Float::cast(*float0).value());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithLargeIntRoundingUpReturnsTrue) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {kMaxUword, 0};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  ASSERT_EQ(Float::cast(runBuiltin(METH(int, __float__), int0)).value(),
            Float::cast(*float0).value());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), float0, int0), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderGeWithIntSubclassReturnsBool) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C(int): pass
zero = C()
one = C(1)
two = C(2)
)")
                   .isError());
  Object self(&scope, runtime_->newFloat(1.0));
  Object zero(&scope, mainModuleAt(runtime_, "zero"));
  Object one(&scope, mainModuleAt(runtime_, "one"));
  Object two(&scope, mainModuleAt(runtime_, "two"));
  EXPECT_EQ(runBuiltin(METH(float, __ge__), self, zero), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), self, one), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __ge__), self, two), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGtWithFloatReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(8.3));
  Object float1(&scope, runtime_->newFloat(1.7));
  EXPECT_EQ(runBuiltin(METH(float, __gt__), float0, float1), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __gt__), float0, float0), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __gt__), float1, float0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGtWithIntSelfNanReturnsFalse) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(kDoubleNaN));
  const uword digits[] = {0, 1};
  Object right(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __gt__), left, right), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGtWithNonFloatReturnsNotImplemented) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(0.0));
  Object right(&scope, Str::empty());
  EXPECT_TRUE(
      runBuiltin(METH(float, __gt__), left, right).isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderGtWithSmallIntReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(5.0));
  Object int0(&scope, runtime_->newInt(4));
  Object int1(&scope, runtime_->newInt(5));
  EXPECT_EQ(runBuiltin(METH(float, __gt__), float0, int0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __gt__), float0, int1), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderGtWithIntSubclassReturnsBool) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C(int): pass
zero = C()
one = C(1)
two = C(2)
)")
                   .isError());
  Object self(&scope, runtime_->newFloat(1.0));
  Object zero(&scope, mainModuleAt(runtime_, "zero"));
  Object one(&scope, mainModuleAt(runtime_, "one"));
  Object two(&scope, mainModuleAt(runtime_, "two"));
  EXPECT_EQ(runBuiltin(METH(float, __gt__), self, zero), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __gt__), self, one), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __gt__), self, two), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderIntWithInfinityRaisesOverflowError) {
  HandleScope scope(thread_);
  Object input_obj(&scope,
                   runtime_->newFloat(std::numeric_limits<double>::infinity()));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(raisedWithStr(*result_obj, LayoutId::kOverflowError,
                            "cannot convert float infinity to integer"));
}

TEST_F(FloatBuiltinsTest, DunderIntWithNaNRaisesOverflowError) {
  HandleScope scope(thread_);
  Object input_obj(&scope, runtime_->newFloat(kDoubleNaN));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(raisedWithStr(*result_obj, LayoutId::kValueError,
                            "cannot convert float NaN to integer"));
}

TEST_F(FloatBuiltinsTest, DunderIntWithZeroReturnsSmallInt) {
  HandleScope scope(thread_);
  Object input_obj(&scope, runtime_->newFloat(0.0));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  ASSERT_TRUE(result_obj.isSmallInt());
  SmallInt result(&scope, *result_obj);
  EXPECT_EQ(result.value(), 0);
}

TEST_F(
    FloatBuiltinsTest,
    DunderIntWithNegativeNumOfGreatestMagnitudeFitInWordReturnsLargeIntOfSingleWord) {
  HandleScope scope(thread_);
  double input_value = std::strtod("-0x1.0000000000000p+63", nullptr);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  ASSERT_TRUE(result_obj.isLargeInt());
  LargeInt result(&scope, *result_obj);
  EXPECT_TRUE(result.isNegative());
  const uword expected_digits[] = {uword{0x8000000000000000}};
  EXPECT_TRUE(isIntEqualsDigits(*result, expected_digits));
}

TEST_F(FloatBuiltinsTest, DunderIntWithSmallIntMinValueReturnsSmallInt) {
  HandleScope scope(thread_);
  double input_value = double{SmallInt::kMinValue};
  // Make sure that the converted double value can fit in SmallInt if
  // it gets converted back to word.
  EXPECT_EQ(static_cast<word>(input_value), SmallInt::kMinValue);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(result_obj.isSmallInt());
  SmallInt result(&scope, *result_obj);
  EXPECT_EQ(result.value(), static_cast<word>(input_value));
}

TEST_F(FloatBuiltinsTest,
       DunderIntWithValueLessThanSmallIntMinValueReturnsLargeInt) {
  HandleScope scope(thread_);
  // Due to the truncation error, static_cast<double>(SmallInt::kMinValue - i)
  // == SmallInt::kMinValue for i ranging from 0 to 512.
  ASSERT_EQ(static_cast<word>(static_cast<double>(SmallInt::kMinValue - 512)),
            SmallInt::kMinValue);
  ASSERT_LT(static_cast<word>(static_cast<double>(SmallInt::kMinValue - 513)),
            SmallInt::kMinValue - 1);
  double input_value = static_cast<double>(SmallInt::kMinValue) - 513;
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(result_obj.isLargeInt());
}

TEST_F(FloatBuiltinsTest, DunderIntWithSmallIntMaxValueReturnsSmallInt) {
  HandleScope scope(thread_);
  // Due to the truncation error, static_cast<double>(SmallInt::kMaxValue) -i)
  // == SmallInt::kMaxValue + 1 for i ranging from 0 to 255, which makes them
  // not fit in SmallInt.
  ASSERT_EQ(static_cast<word>(static_cast<double>(SmallInt::kMaxValue - 255)),
            SmallInt::kMaxValue + 1);
  double input_value = static_cast<double>(SmallInt::kMaxValue - 256);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(result_obj.isSmallInt());
  SmallInt result(&scope, *result_obj);
  EXPECT_EQ(result.value(), static_cast<word>(input_value));
}

TEST_F(FloatBuiltinsTest,
       DunderIntWithValueGreaterThanSmallIntMaxValueReturnsLargeInt) {
  HandleScope scope(thread_);
  // Due to the truncation error, converting MaxValue to double strictly
  // increases the value.
  ASSERT_GT(static_cast<word>(static_cast<double>(SmallInt::kMaxValue)),
            SmallInt::kMaxValue);
  // Therefore, this is the smallest double greater than kMaxValue.
  double input_value = static_cast<double>(SmallInt::kMaxValue);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  EXPECT_TRUE(result_obj.isLargeInt());
}

TEST_F(FloatBuiltinsTest, DunderIntWithLargePositiveDoubleReturnsLargeInt) {
  HandleScope scope(thread_);
  double input_value = std::strtod("0x1.29ef685b3f6fbp+84", nullptr);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  ASSERT_TRUE(result_obj.isLargeInt());
  LargeInt result(&scope, *result_obj);
  EXPECT_TRUE(result.isPositive());
  const uword expected_digits[] = {0x85b3f6fb00000000, 0x129ef6};
  EXPECT_TRUE(isIntEqualsDigits(*result, expected_digits));
}

TEST_F(FloatBuiltinsTest, DunderIntWithLargeNegativeDoubleReturnsLargeInt) {
  HandleScope scope(thread_);
  double input_value = std::strtod("-0x1.29ef685b3f6fbp+84", nullptr);
  Object input_obj(&scope, runtime_->newFloat(input_value));
  Object result_obj(&scope, runBuiltin(METH(float, __int__), input_obj));
  ASSERT_TRUE(result_obj.isLargeInt());
  LargeInt result(&scope, *result_obj);
  EXPECT_TRUE(result.isNegative());
  // Represented as a two's complement, so 1 is added only to the lowest digit
  // as long as it doesn't creat a carry.
  const uword expected_digits[] = {~uword{0x85b3f6fb00000000} + 1,
                                   ~uword{0x129ef6}};
  EXPECT_TRUE(isIntEqualsDigits(*result, expected_digits));
}

TEST_F(FloatBuiltinsTest, DunderLeWithFloatReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(13.1));
  Object float1(&scope, runtime_->newFloat(9.4));
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, float1), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, float0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), float1, float0), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLeWithIntSelfNanReturnsFalse) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(kDoubleNaN));
  const uword digits[] = {0, 1};
  Object right(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __le__), left, right), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLeWithNonFloatReturnsNotImplemented) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(0.0));
  Object right(&scope, Str::empty());
  EXPECT_TRUE(
      runBuiltin(METH(float, __le__), left, right).isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderLeWithSmallIntReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(4.0));
  Object int0(&scope, runtime_->newInt(4));
  Object int1(&scope, runtime_->newInt(3));
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, int0), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, int1), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLeWithBoolReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(1.0));
  Object b_false(&scope, Bool::falseObj());
  Object b_true(&scope, Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, b_false), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), float0, b_true), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLeWithIntSubclassReturnsBool) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C(int): pass
zero = C()
one = C(1)
two = C(2)
)")
                   .isError());
  Object self(&scope, runtime_->newFloat(1.0));
  Object zero(&scope, mainModuleAt(runtime_, "zero"));
  Object one(&scope, mainModuleAt(runtime_, "one"));
  Object two(&scope, mainModuleAt(runtime_, "two"));
  EXPECT_EQ(runBuiltin(METH(float, __le__), self, zero), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), self, one), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __le__), self, two), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithFloatReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(-7.3));
  Object float1(&scope, runtime_->newFloat(1.25));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, float1), Bool::trueObj());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, float0), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float1, float0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithIntSelfNanReturnsFalse) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(kDoubleNaN));
  const uword digits[] = {0, 1};
  Object right(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), left, right), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithNonFloatReturnsNotImplemented) {
  HandleScope scope(thread_);
  Object left(&scope, runtime_->newFloat(0.0));
  Object right(&scope, Str::empty());
  EXPECT_TRUE(
      runBuiltin(METH(float, __lt__), left, right).isNotImplementedType());
}

TEST_F(FloatBuiltinsTest, DunderLtWithSmallIntReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(4.5));
  Object int0(&scope, runtime_->newInt(4));
  Object int1(&scope, runtime_->newInt(5));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int1), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithSmallIntExactReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(44.));
  Object int0(&scope, runtime_->newInt(44));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::falseObj());
  Object float1(&scope, runtime_->newFloat(-3.));
  Object int1(&scope, runtime_->newInt(1));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float1, int1), Bool::trueObj());

  Object float2(&scope, runtime_->newFloat(0x20000000000000));
  Object int2(&scope, runtime_->newInt(0x20000000000000));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float2, int2), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithSmallIntInexactReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(0x20000000000001));
  Object int0(&scope, runtime_->newInt(0x20000000000001));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::trueObj());
  Object float1(&scope, runtime_->newFloat(0x20000000000003));
  Object int1(&scope, runtime_->newInt(0x20000000000003));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float1, int1), Bool::falseObj());
  Object float2(&scope, runtime_->newFloat(0x100000000000011));
  Object int2(&scope, runtime_->newInt(0x100000000000011));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float2, int2), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithLargeIntDifferingSignReturnsBool) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(-1.0));
  const uword digits0[] = {0, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits0));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::trueObj());
  Object float1(&scope, runtime_->newFloat(1.0));
  const uword digits1[] = {0, kMaxUword};
  Object int1(&scope, runtime_->newLargeIntWithDigits(digits1));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float1, int1), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithLargeIntExactEqualsReturnsFalse) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {0, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithLargeIntRoundingDownReturnsTrue) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {1, 1};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  ASSERT_EQ(Float::cast(runBuiltin(METH(int, __float__), int0)).value(),
            Float::cast(*float0).value());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::trueObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithLargeIntRoundingUpReturnsFalse) {
  HandleScope scope(thread_);
  Object float0(&scope, runtime_->newFloat(std::strtod("0x1p64", nullptr)));
  const uword digits[] = {kMaxUword, 0};
  Object int0(&scope, runtime_->newLargeIntWithDigits(digits));
  ASSERT_EQ(Float::cast(runBuiltin(METH(int, __float__), int0)).value(),
            Float::cast(*float0).value());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), float0, int0), Bool::falseObj());
}

TEST_F(FloatBuiltinsTest, DunderLtWithIntSubclassReturnsBool) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C(int): pass
zero = C()
one = C(1)
two = C(2)
)")
                   .isError());
  Object self(&scope, runtime_->newFloat(1.0));
  Object zero(&scope, mainModuleAt(runtime_, "zero"));
  Object one(&scope, mainModuleAt(runtime_, "one"));
  Object two(&scope, mainModuleAt(runtime_, "two"));
  EXPECT_EQ(runBuiltin(METH(float, __lt__), self, zero), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), self, one), Bool::falseObj());
  EXPECT_EQ(runBuiltin(METH(float, __lt__), self, two), Bool::trueObj());
}

}  // namespace testing
}  // namespace py
