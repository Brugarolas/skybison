// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "float-builtins.h"

#include <cfloat>
#include <cmath>
#include <limits>

#include "builtins.h"
#include "float-conversion.h"
#include "formatter.h"
#include "frame.h"
#include "globals.h"
#include "int-builtins.h"
#include "objects.h"
#include "runtime.h"
#include "thread.h"
#include "type-builtins.h"
#include "unicode.h"
#include "utils.h"

namespace py {

// Convert `object` to double.
// Returns a NoneType and sets `value` if the conversion was successful.
// Returns an error or unimplemented otherwise. This does specifically not
// look for `__float__` to match the behavior of `CONVERT_TO_DOUBLE()` in
// cpython.
static RawObject convertToDouble(Thread* thread, const Object& object,
                                 double* result) {
  Runtime* runtime = thread->runtime();
  if (runtime->isInstanceOfFloat(*object)) {
    *result = floatUnderlying(*object).value();
    return NoneType::object();
  }
  if (runtime->isInstanceOfInt(*object)) {
    HandleScope scope(thread);
    Int value(&scope, intUnderlying(*object));
    return convertIntToDouble(thread, value, result);
  }
  return NotImplementedType::object();
}

static const BuiltinAttribute kUserFloatBaseAttributes[] = {
    {ID(_UserFloat__value), RawUserFloatBase::kValueOffset,
     AttributeFlags::kHidden},
};

void initializeFloatType(Thread* thread) {
  addBuiltinType(thread, ID(float), LayoutId::kFloat,
                 /*superclass_id=*/LayoutId::kObject, kUserFloatBaseAttributes,
                 UserFloatBase::kSize,
                 /*basetype=*/true);
}

static void digitsFromDigitsWithUnderscores(const char* s, char* dup,
                                            word* length) {
  char* end = dup;
  char prev = '\0';
  const char* p;
  const char* last = s + *length;
  for (p = s; p < last; p++) {
    if (*p == '_') {
      // Underscores are only allowed after digits.
      if (!ASCII::isDigit(prev)) {
        return;
      }
    } else {
      *end++ = *p;
      // Underscores are only allowed before digits.
      if (prev == '_' && !ASCII::isDigit(*p)) {
        return;
      }
    }
    prev = *p;
  }
  // Underscores are not allowed at the end.
  if (prev == '_') {
    return;
  }
  *end = '\0';
  *length = end - dup;
}

RawObject floatFromDigits(Thread* thread, const char* str, word length) {
  // TODO(T57022841): follow full CPython conversion for strings
  char* end;
  char* new_str;
  const char* dup = str;
  word dup_length = length;
  bool release_memory = false;
  if (std::strchr(str, '_') != nullptr) {
    word* new_length = &dup_length;
    release_memory = true;
    new_str = new char[length + 1];
    digitsFromDigitsWithUnderscores(str, new_str, new_length);
    if (new_str == nullptr) {
      delete[] new_str;
      return thread->raiseWithFmt(LayoutId::kValueError,
                                  "could not convert string to float: '%s'",
                                  str);
    }
    dup = new_str;
  }
  double result = std::strtod(dup, &end);
  // Overflow, return infinity or negative infinity.
  if (result == HUGE_VAL) {
    result = std::numeric_limits<double>::infinity();
  } else if (result == -HUGE_VAL) {
    result = -std::numeric_limits<double>::infinity();
  } else if (dup_length == 0 || end - dup != dup_length) {
    // Conversion was incomplete; the string was not a valid float.
    if (release_memory) {
      delete[] new_str;
    }
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "could not convert string to float: '%s'", str);
  }
  if (release_memory) {
    delete[] new_str;
  }
  return thread->runtime()->newFloat(result);
}

RawObject METH(float, __abs__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  return runtime->newFloat(std::fabs(self));
}

RawObject METH(float, __bool__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  return Bool::fromBool(self != 0.0);
}

RawObject METH(float, __eq__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  Object right(&scope, args.get(1));
  bool result;
  if (runtime->isInstanceOfFloat(*right)) {
    result = left == floatUnderlying(*right).value();
  } else if (runtime->isInstanceOfInt(*right)) {
    Int right_int(&scope, intUnderlying(*right));
    result = doubleEqualsInt(thread, left, right_int);
  } else {
    return NotImplementedType::object();
  }
  return Bool::fromBool(result);
}

RawObject METH(float, __float__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  return floatUnderlying(*self);
}

RawObject METH(float, __format__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  Object spec_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*spec_obj)) {
    return thread->raiseRequiresType(spec_obj, ID(str));
  }
  Float self(&scope, floatUnderlying(*self_obj));
  Str spec(&scope, strUnderlying(*spec_obj));
  if (spec == Str::empty()) {
    if (self_obj.isFloat()) {
      unique_c_ptr<char> result(
          doubleToString(self.value(), 'r', 0, false, true, false, nullptr));
      return runtime->newStrFromCStr(result.get());
    }
    Object str(&scope, thread->invokeMethod1(self_obj, ID(__str__)));
    DCHECK(!str.isErrorNotFound(), "__str__ should always exist");
    if (str.isErrorException()) return *str;
    if (!runtime->isInstanceOfStr(*str)) {
      return thread->raiseWithFmt(
          LayoutId::kTypeError, "__str__ returned non-string (type %T)", &str);
    }
    return *str;
  }

  FormatSpec format;
  Object err(&scope, parseFormatSpec(thread, spec, '\0', '>', &format));
  if (err.isErrorException()) {
    return *err;
  }

  switch (format.type) {
    case '\0':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'n':
    case '%':
      return formatFloat(thread, self.value(), &format);
    default:
      return raiseUnknownFormatError(thread, format.type, self_obj);
  }
}

RawObject METH(float, __ge__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  Object right(&scope, args.get(1));
  bool result;
  if (runtime->isInstanceOfFloat(*right)) {
    result = left >= floatUnderlying(*right).value();
  } else if (runtime->isInstanceOfInt(*right)) {
    Int right_int(&scope, intUnderlying(*right));
    result = compareDoubleWithInt(thread, left, right_int, GE);
  } else {
    return NotImplementedType::object();
  }
  return Bool::fromBool(result);
}

RawObject METH(float, __gt__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  Object right(&scope, args.get(1));
  bool result;
  if (runtime->isInstanceOfFloat(*right)) {
    result = left > floatUnderlying(*right).value();
  } else if (runtime->isInstanceOfInt(*right)) {
    Int right_int(&scope, intUnderlying(*right));
    result = compareDoubleWithInt(thread, left, right_int, GT);
  } else {
    return NotImplementedType::object();
  }
  return Bool::fromBool(result);
}

void decodeDouble(double value, bool* is_neg, int* exp, int64_t* mantissa) {
  const uint64_t man_mask = (uint64_t{1} << kDoubleMantissaBits) - 1;
  const int num_exp_bits = kBitsPerDouble - kDoubleMantissaBits - 1;
  const uint64_t exp_mask = (uint64_t{1} << num_exp_bits) - 1;
  const int exp_bias = (1 << (num_exp_bits - 1)) - 1;
  uint64_t value_bits = bit_cast<uint64_t>(value);
  *is_neg = value_bits >> (kBitsPerDouble - 1);
  *exp = ((value_bits >> kDoubleMantissaBits) & exp_mask) - exp_bias;
  *mantissa = value_bits & man_mask;
}

RawObject intFromDouble(Thread* thread, double value) {
  bool is_neg;
  int exp;
  int64_t man;
  decodeDouble(value, &is_neg, &exp, &man);
  const int exp_bits = kBitsPerDouble - kDoubleMantissaBits - 1;
  const int max_exp = 1 << (exp_bits - 1);
  if (exp == max_exp) {
    if (man == 0) {
      return thread->raiseWithFmt(LayoutId::kOverflowError,
                                  "cannot convert float infinity to integer");
    }
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "cannot convert float NaN to integer");
  }

  // No integral part.
  if (exp < 0) {
    return SmallInt::fromWord(0);
  }
  // Number of bits needed to represent the result integer in 2's complement.
  // +1 for the implicit bit of value 1 and +1 for the sign bit.
  int result_bits = exp + 2;
  // If the number is the negative number of the greatest magnitude
  // (-10000...b), then no extra sign bit is needed.
  if (is_neg && man == 0) {
    result_bits = exp + 1;
  }
  // Fast path for integers that are a word or smaller in size.
  const word man_with_implicit_one = (word{1} << kDoubleMantissaBits) | man;
  // Path that fills a digit of Int, and left-shifts it to match
  // its magnitude with the given exponent.
  DCHECK(
      man_with_implicit_one >= 0,
      "man_with_implicit_one must be positive before the sign bit is applied.");
  Runtime* runtime = thread->runtime();
  if (result_bits <= kBitsPerWord) {
    const word result =
        (exp > kDoubleMantissaBits
             ? (man_with_implicit_one << (exp - kDoubleMantissaBits))
             : (man_with_implicit_one >> (kDoubleMantissaBits - exp)));
    return runtime->newInt(is_neg ? -result : result);
  }
  // TODO(djang): Make another interface for intBinaryLshift() to accept
  // words directly.
  HandleScope scope(thread);
  Int unshifted_result(&scope, runtime->newInt(is_neg ? -man_with_implicit_one
                                                      : man_with_implicit_one));
  Int shifting_bits(&scope, runtime->newInt(exp - kDoubleMantissaBits));
  return runtime->intBinaryLshift(thread, unshifted_result, shifting_bits);
}

word doubleHash(double value) {
  bool is_neg;
  int exp;
  int64_t mantissa;
  decodeDouble(value, &is_neg, &exp, &mantissa);
  const int exp_bits = kBitsPerDouble - kDoubleMantissaBits - 1;
  const int max_exp = 1 << (exp_bits - 1);
  const int min_exp = -(1 << (exp_bits - 1)) + 1;

  if (exp == max_exp) {
    word result;
    if (mantissa == 0) {
      result = is_neg ? -kHashInf : kHashInf;
    } else {
      result = kHashNan;
    }
    return result;
  }

  // The problem in the following is that for float numbers that compare equal
  // to an int number, the hash values have to equal the hash values produced
  // when hashing the integer. To achieve this we base the hashing on the same
  // ideas as `longIntHash()`. Here we want to compute
  // `(mantissa << (exp - mantissa_bits)) % kArithmeticHashModulus`.
  // `mantissa` is guaranteed to be smaller than `kArithmeticHashModulus` so as
  // explained in `longIntHash()` this just means we have to rotate it's bits by
  // `exp` for the result.

  // Add implicit one to mantissa if the number is not a subnormal.
  if (exp > min_exp) {
    mantissa |= int64_t{1} << kDoubleMantissaBits;
  } else if (mantissa == 0) {
    // Shortcut for 0.0 / -0.0.
    return 0;
  } else {
    // sub-normal number, adjust exponent.
    exp += 1;
  }

  // Compute `mantissa % kArithmeticHashModulus` which is just `mantissa`.
  static_assert(uword{1} << (kDoubleMantissaBits + 1) < kArithmeticHashModulus,
                "assumption `mantissa < modulus` does not hold");
  uword result = mantissa;

  // `mantissa` represented `kDoubleMantissaBits` bits shifted by `exp`. We want
  // to align the first integral bit to bit 0 in the result, so we have to
  // rotate by `exp - kDoubleMantissaBits`.
  exp -= kDoubleMantissaBits;
  exp = exp >= 0 ? exp % kArithmeticHashBits
                 : kArithmeticHashBits - 1 - ((-1 - exp) % kArithmeticHashBits);
  result = ((result << exp) & kArithmeticHashModulus) |
           result >> (kArithmeticHashBits - exp);

  if (is_neg) {
    result = -result;
  }

  // cpython replaces `-1` results with -2, because -1 is used as an
  // "uninitialized hash" marker in some situations. We do not use the same
  // marker, but do the same to match behavior.
  if (result == static_cast<uword>(word{-1})) {
    result--;
  }

  // Note: We cannot cache the hash value in the object header, because the
  // result must correspond to the hash values of SmallInt/LargeInt. The object
  // header however has fewer bits and can only store non-negative hash codes.
  return static_cast<word>(result);
}

RawObject METH(float, __hash__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  return SmallInt::fromWord(doubleHash(self));
}

RawObject METH(float, __int__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  return intFromDouble(thread, self);
}

RawObject METH(float, __le__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  Object right(&scope, args.get(1));
  bool result;
  if (runtime->isInstanceOfFloat(*right)) {
    result = left <= floatUnderlying(*right).value();
  } else if (runtime->isInstanceOfInt(*right)) {
    Int right_int(&scope, intUnderlying(*right));
    result = compareDoubleWithInt(thread, left, right_int, LE);
  } else {
    return NotImplementedType::object();
  }
  return Bool::fromBool(result);
}

RawObject METH(float, __lt__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  Object right(&scope, args.get(1));
  bool result;
  if (runtime->isInstanceOfFloat(*right)) {
    result = left < floatUnderlying(*right).value();
  } else if (runtime->isInstanceOfInt(*right)) {
    Int right_int(&scope, intUnderlying(*right));
    result = compareDoubleWithInt(thread, left, right_int, LT);
  } else {
    return NotImplementedType::object();
  }
  return Bool::fromBool(result);
}

RawObject METH(float, __mul__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double left = floatUnderlying(*self_obj).value();

  double right;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &right));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  return runtime->newFloat(left * right);
}

RawObject METH(float, __neg__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  return runtime->newFloat(-self);
}

RawObject METH(float, __add__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  double right;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &right));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  return runtime->newFloat(left + right);
}

RawObject METH(float, __truediv__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);

  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double left = floatUnderlying(*self_obj).value();

  double right;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &right));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  if (right == 0.0) {
    return thread->raiseWithFmt(LayoutId::kZeroDivisionError,
                                "float division by zero");
  }
  return runtime->newFloat(left / right);
}

RawObject METH(float, __round__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  Float value_float(&scope, floatUnderlying(*self_obj));
  double value = value_float.value();

  // If ndigits is None round to nearest integer.
  Object ndigits_obj(&scope, args.get(1));
  if (ndigits_obj.isNoneType()) {
    double result = std::round(value);
    // round to even.
    if (std::fabs(value - result) == 0.5) {
      result = 2.0 * std::round(value / 2.0);
    }
    return intFromDouble(thread, result);
  }

  // Round to ndigits decimals.
  if (!runtime->isInstanceOfInt(*ndigits_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "'%T' cannot be interpreted as an integer",
                                &ndigits_obj);
  }
  Int ndigits_int(&scope, intUnderlying(*ndigits_obj));
  if (ndigits_int.isLargeInt()) {
    return ndigits_int.isNegative() ? runtime->newFloat(0.0) : *value_float;
  }
  word ndigits = ndigits_int.asWord();

  // Keep NaNs and infinities unchanged.
  if (!std::isfinite(value)) {
    return *value_float;
  }

  // Set some reasonable bounds on ndigits and clip otherwise.
  // For `ndigits > ndigits_max`, `value` always rounds to itself.
  // For `ndigits < ndigits_min`, `value` always rounds to +-0.0.
  // Here 0.30103 is an upper bound for `log10(2)`.
  static const word ndigits_max =
      static_cast<word>((kDoubleDigits - kDoubleMinExp) * 0.30103);
  static const word ndigits_min =
      -static_cast<word>((kDoubleMaxExp + 1) * 0.30103);
  if (ndigits > ndigits_max) {
    return *value_float;
  }
  double result;
  if (ndigits < ndigits_min) {
    result = std::copysign(0.0, value);
  } else {
    result = doubleRoundDecimals(value, static_cast<int>(ndigits));
    if (result == HUGE_VAL || result == -HUGE_VAL) {
      return thread->raiseWithFmt(LayoutId::kOverflowError,
                                  "rounded value too large to represent");
    }
  }
  return runtime->newFloat(result);
}

RawObject METH(float, __rtruediv__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);

  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double right = floatUnderlying(*self_obj).value();

  double left;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &left));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  if (right == 0.0) {
    return thread->raiseWithFmt(LayoutId::kZeroDivisionError,
                                "float division by zero");
  }
  return runtime->newFloat(left / right);
}

RawObject METH(float, __sub__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  double left = floatUnderlying(*self).value();

  double right;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &right));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  return runtime->newFloat(left - right);
}

RawObject METH(float, __trunc__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  double self = floatUnderlying(*self_obj).value();
  double integral_part;
  static_cast<void>(std::modf(self, &integral_part));
  return intFromDouble(thread, integral_part);
}

RawObject METH(float, __pow__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }
  if (!args.get(2).isNoneType()) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "pow() 3rd argument not allowed unless all arguments are integers");
  }
  double left = floatUnderlying(*self).value();

  double right;
  Object other(&scope, args.get(1));
  Object maybe_error(&scope, convertToDouble(thread, other, &right));
  // May have returned NotImplemented or raised an exception.
  if (!maybe_error.isNoneType()) return *maybe_error;

  return runtime->newFloat(std::pow(left, right));
}

static word nextNonHexDigit(const Str& str, word pos) {
  for (word len = str.length(); pos < len; ++pos) {
    if (!Byte::isHexDigit(str.byteAt(pos))) {
      break;
    }
  }
  return pos;
}

static word nextNonWhitespace(const Str& str, word pos) {
  for (word length = str.length(); pos < length; ++pos) {
    if (!Byte::isSpace(str.byteAt(pos))) {
      break;
    }
  }
  return pos;
}

static bool strParseOptionalSign(const Str& str, word* pos) {
  if (*pos >= str.length()) {
    return false;
  }

  switch (str.byteAt(*pos)) {
    case '-':
      ++(*pos);
      return true;
    case '+':
      ++(*pos);
      // FALLTHROUGH
    default:
      return false;
  }
}

static bool strAdvancePrefixCaseInsensitiveASCII(const Str& str, word* pos,
                                                 const char* lowercase_prefix) {
  // Caution: if supporting unicode, don't re-write this naively,
  //  string case operations are tricky, and locale/language
  //  dependent
  DCHECK(pos != nullptr, "pos must be non-null");
  DCHECK(lowercase_prefix != nullptr, "lowercase_prefix must be non-null");

  word i = *pos;
  word length = str.length();
  if (i >= length) {
    return false;
  }

  for (; i < length && *lowercase_prefix != '\0'; ++i, ++lowercase_prefix) {
    if (Byte::toLower(str.byteAt(i)) != *lowercase_prefix) {
      return false;
    }
  }
  // Ensure that the entire prefix was present
  bool result = *lowercase_prefix == '\0';
  if (result) {
    *pos = i;
  }
  return result;
}

static bool parseInfOrNan(const Str& str, word* pos, double* result) {
  word pos_start = *pos;

  bool negate = strParseOptionalSign(str, pos);
  if (strAdvancePrefixCaseInsensitiveASCII(str, pos, "inf")) {
    strAdvancePrefixCaseInsensitiveASCII(str, pos, "inity");
    *result = negate ? -kDoubleInfinity : kDoubleInfinity;
  } else if (strAdvancePrefixCaseInsensitiveASCII(str, pos, "nan")) {
    *result = negate ? -kDoubleNaN : kDoubleNaN;
  } else {
    *pos = pos_start;
    *result = -1.0;
    return false;
  }

  return true;
}

static RawObject newFloatOrSubclass(Thread* thread, const Type& type,
                                    const Str& str, word pos, double result) {
  // Optional trailing whitespace leading to the end of the string
  pos = nextNonWhitespace(str, pos);

  if (pos != str.length()) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "invalid hexadecimal floating-point string");
  }

  if (type.instanceLayoutId() == LayoutId::kFloat) {
    return thread->runtime()->newFloat(result);
  }

  HandleScope scope(thread);
  Object obj(&scope, thread->runtime()->newFloat(result));
  return Interpreter::call1(thread, type, obj);
}

// For 0 <= i < ndigits (implicit), get_hex_digit(i) gives the jth most
// significant digit, skipping over the '.' between integral and fractional
// digits.
static word getHexDigit(const Str& str, word fdigits, word coeff_end, word i) {
  // Note: this assumes that:
  // A) all hex digit codepoints have been previously verified to be 1 byte long
  // B) the separating 'p' or 'P' character has been previously verified to be 1
  // byte long.
  word pos = i < fdigits ? coeff_end - (i) : coeff_end - 1 - i;
  word result = Byte::toHexDigit(str.byteAt(pos));
  DCHECK(result >= 0, "Only hex digits should be indexed here");
  return result;
}

// Computes a integer float value from the digits in str via getHexDigit(), from
// digit_ms..digit_ls, inclusive
static double sumHexDigitsDouble(const Str& str, word fdigits, word coeff_end,
                                 word digit_ms, word digit_ls) {
  double result = 0;
  for (word i = digit_ms; i >= digit_ls; --i) {
    result = 16.0 * result + getHexDigit(str, fdigits, coeff_end, i);
  }
  return result;
}

static RawObject raiseOverflowErrorHexFloatTooLarge(Thread* thread) {
  return thread->raiseWithFmt(
      LayoutId::kOverflowError,
      "hexadecimal value too large to represent as a float");
}

static void floatHexParseCoefficients(const Str& str, word* pos, word* ndigits,
                                      word* fdigits, word* coeff_end) {
  DCHECK(pos != nullptr && ndigits != nullptr && fdigits != nullptr &&
             coeff_end != nullptr,
         "Invalid argument to floatHexParseCoefficients");

  word coeff_start = *pos;
  *pos = nextNonHexDigit(str, *pos);

  const word length = str.length();
  word pos_store = *pos;
  if (*pos < length && '.' == str.byteAt(*pos)) {
    // Note skipping over the '.' character
    *pos = nextNonHexDigit(str, *pos + 1);
    *coeff_end = *pos - 1;
  } else {
    *coeff_end = *pos;
  }

  // ndigits = total # of hex digits; fdigits = # after point
  *ndigits = *coeff_end - coeff_start;
  *fdigits = *coeff_end - pos_store;
}

static long floatHexParseExponent(const Str& str, word* pos) {
  long exponent = 0;
  if (*pos < str.length() && 'p' == Byte::toLower(str.byteAt(*pos))) {
    ++(*pos);
    bool negate = strParseOptionalSign(str, pos);

    for (word length = str.length();
         *pos < length && Byte::isDigit(str.byteAt(*pos)); ++(*pos)) {
      exponent = exponent * 10 + Byte::toDigit(str.byteAt(*pos));
    }
    if (negate) {
      exponent = -exponent;
    }
  }
  return exponent;
}

RawObject METH(float, as_integer_ratio)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(float));
  }
  Float self(&scope, floatUnderlying(*self_obj));
  double value = self.value();
  if (std::isinf(value)) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "cannot convert Infinity to integer ratio");
  }
  if (std::isnan(value)) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "cannot convert NaN to integer ratio");
  }
  int exponent;
  double float_part = std::frexp(value, &exponent);
  // If FLT_RADIX != 2, the 300 steps may leave a tiny fractional part to be
  // truncated by intFromDouble.
  for (word i = 0; i < 300 && float_part != std::floor(float_part); i++) {
    float_part *= 2.0;
    exponent--;
  }
  Object numerator_obj(&scope, intFromDouble(thread, float_part));
  if (numerator_obj.isErrorException()) {
    return *numerator_obj;
  }
  Int numerator(&scope, *numerator_obj);
  Int denominator(&scope, SmallInt::fromWord(1));
  if (exponent > 0) {
    Int exponent_obj(&scope, SmallInt::fromWord(exponent));
    numerator = runtime->intBinaryLshift(thread, numerator, exponent_obj);
  } else {
    Int exponent_obj(&scope, SmallInt::fromWord(-exponent));
    denominator = runtime->intBinaryLshift(thread, denominator, exponent_obj);
  }
  return runtime->newTupleWith2(numerator, denominator);
}

RawObject METH(float, fromhex)(Thread* thread, Arguments args) {
  // Convert a hexadecimal string to a float.
  // Check the function arguments
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();

  Object type_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfType(*type_obj)) {
    return thread->raiseRequiresType(type_obj, ID(type));
  }
  Type type(&scope, *type_obj);

  Object str_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*str_obj)) {
    return thread->raiseRequiresType(str_obj, ID(str));
  }

  const Str str(&scope, strUnderlying(*str_obj));

  //
  // Parse the string
  //

  // leading whitespace
  word pos = nextNonWhitespace(str, 0);

  // infinities and nans
  {
    double result;
    if (parseInfOrNan(str, &pos, &result)) {
      return newFloatOrSubclass(thread, type, str, pos, result);
    }
  }

  // optional sign
  bool negate = strParseOptionalSign(str, &pos);

  // [0x]
  strAdvancePrefixCaseInsensitiveASCII(str, &pos, "0x");

  // coefficient: <integer> [. <fraction>]
  word ndigits, fdigits, coeff_end;
  floatHexParseCoefficients(str, &pos, &ndigits, &fdigits, &coeff_end);
  if (ndigits == 0) {
    return thread->raiseWithFmt(
        LayoutId::kValueError,
        "invalid hexadecimal floating-point string, no digits");
  }

  if (ndigits > Utils::minimum(kDoubleMinExp - kDoubleDigits - kMinLong / 2,
                               kMaxLong / 2 + 1 - kDoubleMaxExp) /
                    4) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "hexadecimal string too long to convert");
  }

  // [p <exponent>]
  long exponent = floatHexParseExponent(str, &pos);

  //
  // Compute rounded value of the hex string
  //

  // Discard leading zeros, and catch extreme overflow and underflow
  while (ndigits > 0 &&
         getHexDigit(str, fdigits, coeff_end, ndigits - 1) == 0) {
    --ndigits;
  }
  if (ndigits == 0 || exponent < kMinLong / 2) {
    return newFloatOrSubclass(thread, type, str, pos, negate ? -0.0 : 0.0);
  }
  if (exponent > kMaxLong / 2) {
    return raiseOverflowErrorHexFloatTooLarge(thread);
  }

  // Adjust exponent for fractional part, 4 bits per nibble
  exponent -= 4 * static_cast<long>(fdigits);

  // top_exponent = 1 more than exponent of most sig. bit of coefficient
  long top_exponent = exponent + 4 * (static_cast<long>(ndigits) - 1);
  for (int digit = getHexDigit(str, fdigits, coeff_end, ndigits - 1);
       digit != 0; digit /= 2) {
    ++top_exponent;
  }

  // catch almost all nonextreme cases of overflow and underflow here
  if (top_exponent < kDoubleMinExp - kDoubleDigits) {
    return newFloatOrSubclass(thread, type, str, pos, negate ? -0.0 : 0.0);
  }
  if (top_exponent > kDoubleMaxExp) {
    return raiseOverflowErrorHexFloatTooLarge(thread);
  }

  // lsb = exponent of least significant bit of the *rounded* value.
  // This is top_exponent - kDoubleDigits unless result is subnormal.
  long lsb = Utils::maximum(top_exponent, static_cast<long>(kDoubleMinExp)) -
             kDoubleDigits;
  // Check if rounding required
  double result = 0.0;
  if (exponent >= lsb) {
    // no rounding required
    result = sumHexDigitsDouble(str, fdigits, coeff_end, ndigits - 1, 0);
  } else {
    // rounding required.  key_digit is the index of the hex digit
    // containing the first bit to be rounded away.
    int half_eps = 1 << static_cast<int>((lsb - exponent - 1) % 4);
    long key_digit = (lsb - exponent - 1) / 4;
    result =
        sumHexDigitsDouble(str, fdigits, coeff_end, ndigits - 1, key_digit + 1);

    // sum in the final key_digit, but subtract off 2*half_eps from it first to
    // allow for the rounding below.
    int digit = getHexDigit(str, fdigits, coeff_end, key_digit);
    result = 16.0 * result + static_cast<double>(digit & (16 - 2 * half_eps));

    // round-half-even: round up if bit lsb-1 is 1 and at least one of
    // bits lsb, lsb-2, lsb-3, lsb-4, ... is 1.
    if ((digit & half_eps) != 0) {
      bool round_up = false;
      if ((digit & (3 * half_eps - 1)) != 0 ||
          (half_eps == 8 &&
           (getHexDigit(str, fdigits, coeff_end, key_digit + 1) & 1) != 0)) {
        round_up = true;
      } else {
        for (ssize_t i = key_digit - 1; i >= 0; --i) {
          if (getHexDigit(str, fdigits, coeff_end, i) != 0) {
            round_up = true;
            break;
          }
        }
      }
      if (round_up) {
        result += 2 * half_eps;
        if (top_exponent == kDoubleMaxExp &&
            result == ldexp(static_cast<double>(2 * half_eps), kDoubleDigits)) {
          // overflow corner case: pre-rounded value < 2**kDoubleMaxExp;
          // rounded=2**kDoubleMaxExp.
          return raiseOverflowErrorHexFloatTooLarge(thread);
        }
      }
    }
    // Adjust the exponent over 4 bits for every nibble we skipped processing
    exponent += 4 * key_digit;
  }
  result = ldexp(result, static_cast<int>(exponent));
  return newFloatOrSubclass(thread, type, str, pos, negate ? -result : result);
}

RawObject METH(float, hex)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }

  double double_value = floatUnderlying(*self).value();
  return formatDoubleHexadecimalSimple(runtime, double_value);
}

RawObject METH(float, is_integer)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfFloat(*self)) {
    return thread->raiseRequiresType(self, ID(float));
  }

  double double_value = floatUnderlying(*self).value();
  return Bool::fromBool(!std::isinf(double_value) &&
                        std::floor(double_value) == double_value);
}

}  // namespace py
