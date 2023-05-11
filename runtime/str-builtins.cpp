// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "str-builtins.h"

#include <cctype>
#include <cwchar>

#include "builtins.h"
#include "formatter-utils.h"
#include "formatter.h"
#include "frame.h"
#include "globals.h"
#include "int-builtins.h"
#include "objects.h"
#include "runtime.h"
#include "set-builtins.h"
#include "slice-builtins.h"
#include "thread.h"
#include "tuple-builtins.h"
#include "type-builtins.h"
#include "unicode.h"
#include "utils.h"

namespace py {

word adjustedStrIndex(const Str& str, word index) {
  word len = str.length();
  if (index >= 0) {
    return str.offsetByCodePoints(0, index);
  }
  if (-len < index) {
    return Utils::maximum(0l, str.offsetByCodePoints(len, index));
  }
  return 0;
}

RawObject dataArraySubstr(Thread* thread, const DataArray& data, word start,
                          word length) {
  word data_len = data.length();
  DCHECK_BOUND(start, data_len);
  DCHECK_BOUND(start, data_len - length);
  // SmallStr result
  if (length <= RawSmallStr::kMaxLength) {
    byte buffer[RawSmallStr::kMaxLength];
    data.copyToStartAt(buffer, length, start);
    return SmallStr::fromBytes(View<byte>(buffer, length));
  }
  // LargeStr result
  HandleScope scope(thread);
  MutableBytes result(&scope,
                      thread->runtime()->newMutableBytesUninitialized(length));
  result.replaceFromWithStartAt(0, *data, length, start);
  return result.becomeStr();
}

RawObject newStrFromWideChar(Thread* thread, const wchar_t* wc_str) {
  return newStrFromWideCharWithLength(thread, wc_str, std::wcslen(wc_str));
}

RawObject newStrFromWideCharWithLength(Thread* thread, const wchar_t* wc_str,
                                       word length) {
  static_assert(sizeof(*wc_str) * kBitsPerByte == 32,
                "only 32bit wchar_t supported.");

  for (word i = 0; i < length; ++i) {
    if (wc_str[i] < 0 || wc_str[i] > kMaxUnicode) {
      return thread->raiseWithFmt(LayoutId::kValueError,
                                  "character is not in range");
    }
  }

  return thread->runtime()->newStrFromUTF32(
      View<int32_t>(reinterpret_cast<const int32_t*>(wc_str), length));
}

void strCopyToWCStr(wchar_t* buf, size_t buf_length, const Str& str) {
  uword wchar_index = 0;
  for (word byte_index = 0, num_bytes = 0, byte_count = str.length();
       byte_index < byte_count && wchar_index < buf_length;
       byte_index += num_bytes, wchar_index += 1) {
    int32_t cp = Str::cast(*str).codePointAt(byte_index, &num_bytes);
    static_assert(sizeof(*buf) == sizeof(cp), "Requires 32bit wchar_t");
    buf[wchar_index] = static_cast<wchar_t>(cp);
  }
  buf[wchar_index] = '\0';
}

static word strCountCharFromTo(const Str& haystack, byte needle, word start,
                               word end) {
  word result = 0;
  if (haystack.isSmallStr()) {
    RawSmallStr small = SmallStr::cast(*haystack);
    for (word i = start; i < end; i++) {
      if (small.byteAt(i) == needle) result++;
    }
    return result;
  }
  byte* ptr = reinterpret_cast<byte*>(LargeStr::cast(*haystack).address());
  for (word i = start; i < end; i++) {
    if (ptr[i] == needle) result++;
  }
  return result;
}

RawObject strCount(const Str& haystack, const Str& needle, word start,
                   word end) {
  if (end < 0 || start < 0) {
    // N.B.: If end is negative we may be able to cheaply walk backward. We
    // should avoid calling adjustSearchIndices here since the underlying
    // container is not O(1) and replace it with something that preserves some
    // of the signals that would be useful to lower the cost of the O(n)
    // traversal.
    // TODO(T41400083): Use a different search algorithm
    Slice::adjustSearchIndices(&start, &end, haystack.codePointLength());
  }

  word start_index = start == 0 ? 0 : haystack.offsetByCodePoints(0, start);
  if (start_index == haystack.length() && needle.length() > 0) {
    // Haystack is too small; fast early return
    return SmallInt::fromWord(0);
  }

  word end_index = end == kMaxWord
                       ? haystack.length()
                       : haystack.offsetByCodePoints(start_index, end - start);
  if ((end_index - start_index) < needle.length() || start_index > end_index) {
    // Haystack is too small; fast early return
    return SmallInt::fromWord(0);
  }

  if (needle.length() == 1) {
    return SmallInt::fromWord(strCountCharFromTo(
        haystack, SmallStr::cast(*needle).byteAt(0), start_index, end_index));
  }

  // TODO(T41400083): Use a different search algorithm
  return SmallInt::fromWord(strCountSubStrFromTo(haystack, needle, start_index,
                                                 end_index, haystack.length()));
}

word strCountSubStrFromTo(const Str& haystack, const Str& needle, word start,
                          word end, word max_count) {
  DCHECK(max_count >= 0, "max_count must be non-negative");
  word needle_len = needle.length();
  word num_match = 0;
  // Loop is in byte space, not code point space
  for (word i = start; i <= end - needle_len && num_match < max_count;) {
    if (strHasPrefix(haystack, needle, i)) {
      i += needle_len;
      num_match++;
      continue;
    }
    i++;
  }
  return num_match;
}

word strCountSubStr(const Str& haystack, const Str& needle, word max_count) {
  return strCountSubStrFromTo(haystack, needle, 0, haystack.length(),
                              max_count);
}

RawObject strEncodeASCII(Thread* thread, const Str& str) {
  HandleScope scope(thread);
  if (!str.isASCII()) {
    return Unbound::object();
  }
  if (str.isSmallStr()) {
    return SmallStr::cast(*str).becomeBytes();
  }
  word str_len = str.length();
  MutableBytes bytes(&scope,
                     thread->runtime()->newMutableBytesUninitialized(str_len));
  bytes.replaceFromWithStr(0, *str, str_len);
  return bytes.becomeImmutable();
}

RawObject strEscapeNonASCII(Thread* thread, const Str& str) {
  if (str.isASCII()) return *str;
  // Slow implementation using `codecs`.
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Symbols* symbols = runtime->symbols();
  Object ascii(&scope, symbols->at(ID(ascii)));
  Object backslashreplace(&scope, symbols->at(ID(backslashreplace)));
  Bytes result_bytes(
      &scope, thread->invokeMethod3(str, ID(encode), ascii, backslashreplace));
  word length = result_bytes.length();
  MutableBytes result(&scope, runtime->newMutableBytesUninitialized(length));
  result.replaceFromWithBytes(0, *result_bytes, length);
  return result.becomeStr();
}

RawObject strJoinWithTupleOrList(Thread* thread, const Str& sep,
                                 const Object& iterable) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Tuple tuple(&scope, runtime->emptyTuple());
  word length = 0;
  if (iterable.isTuple()) {
    tuple = *iterable;
    length = tuple.length();
  } else if (iterable.isList()) {
    tuple = List::cast(*iterable).items();
    length = List::cast(*iterable).numItems();
  } else {
    // Slow path: collect items into list in Python and call again
    return Unbound::object();
  }
  Object elt(&scope, NoneType::object());
  for (word i = 0; i < length; i++) {
    elt = tuple.at(i);
    if (!runtime->isInstanceOfStr(*elt)) {
      return thread->raiseWithFmt(
          LayoutId::kTypeError,
          "sequence item %w: expected str instance, %T found", i, &elt);
    }
  }
  return runtime->strJoin(thread, sep, tuple, length);
}

word strSpan(const Str& src, const Str& str) {
  word length = src.length();
  word str_length = str.length();
  word first = 0;
  for (; first < length; first++) {
    bool has_match = false;
    byte ch = src.byteAt(first);
    for (word j = 0; j < str_length; j++) {
      if (ch == str.byteAt(j)) {
        has_match = true;
        break;
      }
    }
    if (!has_match) {
      break;
    }
  }
  return first;
}

RawObject strSubstr(Thread* thread, const Str& str, word start, word length) {
  word str_len = str.length();
  DCHECK_BOUND(start, str_len);
  DCHECK_BOUND(length, str_len - start);
  if (length == str_len) {
    return *str;
  }
  // SmallStr result
  if (length <= RawSmallStr::kMaxLength) {
    byte buffer[RawSmallStr::kMaxLength];
    str.copyToStartAt(buffer, length, start);
    return SmallStr::fromBytes(View<byte>(buffer, length));
  }
  // LargeStr result
  HandleScope scope(thread);
  MutableBytes result(&scope,
                      thread->runtime()->newMutableBytesUninitialized(length));
  result.replaceFromWithStartAt(0, LargeStr::cast(*str), length, start);
  return result.becomeStr();
}

word strRSpan(const Str& src, const Str& str, word rend) {
  DCHECK(rend >= 0, "string index underflow");
  word length = src.length();
  word str_length = str.length();
  word result = 0;
  for (word i = length - 1; i >= rend; i--, result++) {
    byte ch = src.byteAt(i);
    bool has_match = false;
    for (word j = 0; j < str_length; j++) {
      if (ch == str.byteAt(j)) {
        has_match = true;
        break;
      }
    }
    if (!has_match) {
      break;
    }
  }
  return result;
}

static bool isLineBreak(int32_t c) {
  switch (c) {
    // Common cases
    case '\n':
    case '\r':
    // Less common cases
    case '\f':
    case '\v':
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x85:
    case 0x2028:
    case 0x2029:
      return true;
    default:
      return false;
  }
}

RawObject strSplit(Thread* thread, const Str& str, const Str& sep,
                   word maxsplit) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  word num_splits = strCountSubStr(str, sep, maxsplit);
  word result_len = num_splits + 1;
  MutableTuple result_items(&scope, runtime->newMutableTuple(result_len));
  word last_idx = 0;
  word sep_len = sep.length();
  for (word i = 0, result_idx = 0; result_idx < num_splits;) {
    if (strHasPrefix(str, sep, i)) {
      result_items.atPut(result_idx++,
                         strSubstr(thread, str, last_idx, i - last_idx));
      i += sep_len;
      last_idx = i;
    } else {
      i = str.offsetByCodePoints(i, 1);
    }
  }
  result_items.atPut(num_splits,
                     strSubstr(thread, str, last_idx, str.length() - last_idx));
  List result(&scope, runtime->newList());
  result.setItems(*result_items);
  result.setNumItems(result_len);
  return *result;
}

RawObject strSplitlines(Thread* thread, const Str& str, bool keepends) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  List result(&scope, runtime->newList());
  // Looping over code points, not bytes, but i is a byte offset
  for (word i = 0, j = 0; i < str.length(); j = i) {
    // Skip newline chars
    word num_bytes;
    while (i < str.length() && !isLineBreak(str.codePointAt(i, &num_bytes))) {
      i += num_bytes;
    }

    word eol_pos = i;
    if (i < str.length()) {
      int32_t cp = str.codePointAt(i, &num_bytes);
      word next = i + num_bytes;
      word next_num_bytes;
      // Check for \r\n specifically
      if (cp == '\r' && next < str.length() &&
          str.codePointAt(next, &next_num_bytes) == '\n') {
        i += next_num_bytes;
      }
      i += num_bytes;
      if (keepends) {
        eol_pos = i;
      }
    }

    // If there are no newlines, the str returned should be identity-equal
    if (j == 0 && eol_pos == str.length() && str.isStr()) {
      runtime->listAdd(thread, result, str);
      return *result;
    }

    Str substr(&scope, strSubstr(thread, str, j, eol_pos - j));
    runtime->listAdd(thread, result, substr);
  }

  return *result;
}

RawObject strStripSpace(Thread* thread, const Str& src) {
  word length = src.length();
  if (length == 0) {
    return *src;
  }
  if (length == 1 && ASCII::isSpace(src.byteAt(0))) {
    return Str::empty();
  }
  word first = 0;
  while (first < length) {
    word num_bytes;
    int32_t ch = src.codePointAt(first, &num_bytes);
    if (!Unicode::isSpace(ch)) {
      break;
    }
    first += num_bytes;
  }
  word last = length;
  while (last > first) {
    last = src.offsetByCodePoints(last, -1);
    word num_bytes;
    int32_t ch = src.codePointAt(last, &num_bytes);
    if (!Unicode::isSpace(ch)) {
      last += num_bytes;
      break;
    }
  }
  return strSubstr(thread, src, first, last - first);
}

RawObject strStripSpaceLeft(Thread* thread, const Str& src) {
  word length = src.length();
  if (length == 0) {
    return *src;
  }
  if (length == 1 && ASCII::isSpace(src.byteAt(0))) {
    return Str::empty();
  }
  word first = 0;
  while (first < length) {
    word num_bytes;
    int32_t ch = src.codePointAt(first, &num_bytes);
    if (!Unicode::isSpace(ch)) {
      break;
    }
    first += num_bytes;
  }
  return strSubstr(thread, src, first, length - first);
}

RawObject strStripSpaceRight(Thread* thread, const Str& src) {
  word length = src.length();
  if (length == 0) {
    return *src;
  }
  if (length == 1 && ASCII::isSpace(src.byteAt(0))) {
    return Str::empty();
  }
  word last = length;
  while (last > 0) {
    last = src.offsetByCodePoints(last, -1);
    word num_bytes;
    int32_t ch = src.codePointAt(last, &num_bytes);
    if (!Unicode::isSpace(ch)) {
      last += num_bytes;
      break;
    }
  }
  return strSubstr(thread, src, 0, last);
}

RawObject strStrip(Thread* thread, const Str& src, const Str& str) {
  word length = src.length();
  if (length == 0 || str.length() == 0) {
    return *src;
  }
  word first = strSpan(src, str);
  word last = strRSpan(src, str, first);
  return strSubstr(thread, src, first, length - first - last);
}

RawObject strStripLeft(Thread* thread, const Str& src, const Str& str) {
  word length = src.length();
  if (length == 0 || str.length() == 0) {
    return *src;
  }
  word first = strSpan(src, str);
  return strSubstr(thread, src, first, length - first);
}

RawObject strStripRight(Thread* thread, const Str& src, const Str& str) {
  word length = src.length();
  if (length == 0 || str.length() == 0) {
    return *src;
  }
  word first = 0;
  word last = strRSpan(src, str, first);
  return strSubstr(thread, src, 0, length - last);
}

RawObject strTranslateASCII(Thread* thread, const Str& src, const Str& table) {
  if (table.length() > kMaxASCII || !table.isASCII()) {
    return Unbound::object();
  }
  word src_len = src.length();
  word table_len = table.length();
  HandleScope scope(thread);
  MutableBytes result(&scope,
                      thread->runtime()->newMutableBytesUninitialized(src_len));
  // Since all non-ASCII bytes in UTF-8 have a 1 in front, we can iterate by
  // bytes instead of codepoints
  for (word i = 0; i < src.length(); i++) {
    byte to_translate = src.byteAt(i);
    if (to_translate > table_len) {
      result.byteAtPut(i, to_translate);
    } else {
      result.byteAtPut(i, table.byteAt(to_translate));
    }
  }
  return result.becomeStr();
}

RawObject strIteratorNext(Thread* thread, const StrIterator& iter) {
  HandleScope scope(thread);
  word byte_offset = iter.index();
  Str underlying(&scope, iter.iterable());
  if (byte_offset >= underlying.length()) {
    return Error::noMoreItems();
  }
  word num_bytes = 0;
  word code_point = underlying.codePointAt(byte_offset, &num_bytes);
  iter.setIndex(byte_offset + num_bytes);
  return RawSmallStr::fromCodePoint(code_point);
}

static const BuiltinAttribute kUserStrBaseAttributes[] = {
    {ID(_UserStr__value), RawUserStrBase::kValueOffset,
     AttributeFlags::kHidden},
};

static const BuiltinAttribute kStrIteratorAttributes[] = {
    {ID(_str_iterator__iterable), RawStrIterator::kIterableOffset,
     AttributeFlags::kHidden},
    {ID(_str_iterator__index), RawStrIterator::kIndexOffset,
     AttributeFlags::kHidden},
};

void initializeStrTypes(Thread* thread) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();

  Type str(&scope, addBuiltinType(thread, ID(str), LayoutId::kStr,
                                  /*superclass_id=*/LayoutId::kObject,
                                  kUserStrBaseAttributes, UserStrBase::kSize,
                                  /*basetype=*/true));

  {
    Type type(&scope,
              addImmediateBuiltinType(thread, ID(largestr), LayoutId::kLargeStr,
                                      /*builtin_base=*/LayoutId::kStr,
                                      /*superclass_id=*/LayoutId::kObject,
                                      /*basetype=*/false));
    Layout::cast(type.instanceLayout()).setDescribedType(*str);
    runtime->setLargeStrType(type);
  }

  {
    Type type(&scope,
              addImmediateBuiltinType(thread, ID(smallstr), LayoutId::kSmallStr,
                                      /*builtin_base=*/LayoutId::kStr,
                                      /*superclass_id=*/LayoutId::kObject,
                                      /*basetype=*/false));
    Layout::cast(type.instanceLayout()).setDescribedType(*str);
    runtime->setSmallStrType(type);
  }

  addBuiltinType(thread, ID(str_iterator), LayoutId::kStrIterator,
                 /*superclass_id=*/LayoutId::kObject, kStrIteratorAttributes,
                 StrIterator::kSize, /*basetype=*/false);
}

RawObject METH(str, __add__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!runtime->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return runtime->strConcat(thread, self, other);
}

RawObject METH(str, __bool__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  return Bool::fromBool(*self != Str::empty());
}

RawObject METH(str, __contains__)(Thread* thread, Arguments args) {
  RawObject self = args.get(0);
  RawObject other = args.get(1);
  Runtime* runtime = thread->runtime();
  bool is_self_invalid = true;
  if (runtime->isInstanceOfStr(self)) {
    is_self_invalid = false;
    if (runtime->isInstanceOfStr(other)) {
      return Bool::fromBool(strUnderlying(self).includes(strUnderlying(other)));
    }
  }
  HandleScope scope(thread);
  Object invalid(&scope, is_self_invalid ? self : other);
  return thread->raiseRequiresType(invalid, ID(str));
}

RawObject METH(str, __eq__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(self.equals(*other));
}

RawObject METH(str, __format__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));

  Object spec_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*spec_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "__format__() argument 1 must be str, not %T",
                                &spec_obj);
  }
  Str spec(&scope, strUnderlying(*spec_obj));

  if (spec.length() == 0) {
    return *self;
  }

  FormatSpec format;
  Object possible_error(&scope,
                        parseFormatSpec(thread, spec,
                                        /*default_type=*/'s',
                                        /*default_align=*/'<', &format));
  if (!possible_error.isNoneType()) {
    DCHECK(possible_error.isErrorException(), "expected exception");
    return *possible_error;
  }
  if (format.type != 's') {
    return raiseUnknownFormatError(thread, format.type, self_obj);
  }
  if (format.positive_sign != '\0') {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "Sign not allowed in string format specifier");
  }
  if (format.alternate) {
    return thread->raiseWithFmt(
        LayoutId::kValueError,
        "Alternate form (#) not allowed in string format specifier");
  }
  if (format.alignment == '=') {
    return thread->raiseWithFmt(
        LayoutId::kValueError,
        "'=' alignment not allowed in string format specifier");
  }

  return formatStr(thread, self, &format);
}

RawObject METH(str, __ge__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(self.compare(*other) >= 0);
}

bool METH(str, __getitem___intrinsic)(Thread* thread) {
  RawObject arg0 = thread->stackPeek(1);
  if (!arg0.isStr()) {
    return false;
  }
  RawObject arg1 = thread->stackPeek(0);
  word idx;
  if (arg1.isSmallInt()) {
    idx = RawSmallInt::cast(arg1).value();
  } else if (arg1.isBool()) {
    idx = RawBool::cast(arg1).value();
  } else {
    word start, stop;
    if (!tryUnpackSlice(arg1, &start, &stop)) {
      return false;
    }
    {
      // Manually adjust slice bounds to avoid an extra call to codePointLength
      HandleScope scope(thread);
      Str self(&scope, arg0);
      word start_index = adjustedStrIndex(self, start);
      word stop_index = adjustedStrIndex(self, stop);
      word length = stop_index - start_index;

      thread->stackDrop(2);
      thread->stackSetTop(length <= 0
                              ? Str::empty()
                              : strSubstr(thread, self, start_index, length));
    }
    return true;
  }
  RawStr self = Str::cast(arg0);
  word len = self.length();
  if (0 <= idx && idx < len) {
    word offset = self.offsetByCodePoints(0, idx);
    if (offset < len) {
      word ignored;
      thread->stackDrop(2);
      thread->stackSetTop(
          RawSmallStr::fromCodePoint(self.codePointAt(offset, &ignored)));
      return true;
    }
  }
  if (0 > idx) {
    word offset = self.offsetByCodePoints(len, idx);
    if (offset < len && offset != -1) {
      word ignored;
      thread->stackDrop(2);
      thread->stackSetTop(
          RawSmallStr::fromCodePoint(self.codePointAt(offset, &ignored)));
      return true;
    }
  }
  return false;
}

RawObject METH(str, __gt__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(self.compare(*other) > 0);
}

RawObject METH(str, __hash__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self)) {
    return thread->raiseRequiresType(self, ID(str));
  }
  Str self_str(&scope, strUnderlying(*self));
  return SmallInt::fromWord(strHash(thread, *self_str));
}

void strInternInTuple(Thread* thread, const Object& items) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfTuple(*items), "items must be a tuple instance");
  Tuple tuple(&scope, tupleUnderlying(*items));
  Str str(&scope, Str::empty());
  Object result(&scope, NoneType::object());
  for (word i = 0; i < tuple.length(); i++) {
    str = tuple.at(i);
    result = Runtime::internStr(thread, str);
    if (result.isError()) continue;
    if (result != str) {
      tuple.atPut(i, *result);
    }
  }
}

static bool allNameChars(const Str& str) {
  for (word i = 0; i < str.length(); i++) {
    byte c = str.byteAt(i);
    if (!ASCII::isAlnum(c) && c != '_') {
      return false;
    }
  }
  return true;
}

static bool strInternConstant(Thread* thread, Object* object) {
  if (object->isStr()) {
    HandleScope scope(thread);
    Str str(&scope, **object);
    if (allNameChars(str)) {
      str = Runtime::internStr(thread, str);
      if (str != *object) {
        *object = *str;
        return true;
      }
    }
    return false;
  }
  if (object->isTuple()) {
    HandleScope scope(thread);
    Tuple tuple(&scope, **object);
    Object item(&scope, NoneType::object());
    bool modified = false;
    for (word i = 0, length = tuple.length(); i < length; i++) {
      item = tuple.at(i);
      if (strInternConstant(thread, &item)) {
        modified = true;
        tuple.atPut(i, *item);
      }
    }
    if (!modified) return false;
    *object = *tuple;
    return true;
  }
  if (object->isFrozenSet()) {
    HandleScope scope(thread);
    FrozenSet set(&scope, **object);
    FrozenSet new_set(&scope, thread->runtime()->newFrozenSet());
    Object value(&scope, NoneType::object());
    Object hash(&scope, NoneType::object());
    bool modified = false;
    for (word idx = 0; setNextItem(set, &idx, &value);) {
      if (strInternConstant(thread, &value)) {
        modified = true;
      }
      hash = Interpreter::hash(thread, value);
      if (hash.isErrorException()) return false;
      setAdd(thread, new_set, value, SmallInt::cast(*hash).value());
    }
    if (!modified) return false;
    *object = *new_set;
    return true;
  }
  return false;
}

bool strInternConstants(Thread* thread, const Object& items) {
  HandleScope scope(thread);
  DCHECK(thread->runtime()->isInstanceOfTuple(*items),
         "items must be a tuple instance");
  Object tuple(&scope, tupleUnderlying(*items));
  return strInternConstant(thread, &tuple);
}

word strFind(const Str& haystack, const Str& needle) {
  word haystack_len = haystack.length();
  word needle_len = needle.length();
  if (needle_len > haystack_len) {
    return -1;
  }
  if (needle_len == 0) {
    return 0;
  }
  if (needle_len == 1 && haystack.isASCII()) {
    return strFindAsciiChar(haystack, needle.byteAt(0));
  }
  // Loop is in byte space, not code point space
  word result = 0;
  // TODO(T41400083): Use a different search algorithm
  for (word i = 0; i <= haystack_len - needle_len; result++) {
    if (strHasPrefix(haystack, needle, i)) {
      return result;
    }
    i = haystack.offsetByCodePoints(i, 1);
  }
  return -1;
}

word strFindWithRange(const Str& haystack, const Str& needle, word start,
                      word end) {
  if (end < 0 || start < 0) {
    Slice::adjustSearchIndices(&start, &end, haystack.codePointLength());
  }

  word start_index = haystack.offsetByCodePoints(0, start);
  if (start_index == haystack.length() && needle.length() > 0) {
    // Haystack is too small; fast early return
    return -1;
  }
  word end_index = haystack.offsetByCodePoints(start_index, end - start);

  if ((end_index - start_index) < needle.length() || start_index > end_index) {
    // Haystack is too small; fast early return
    return -1;
  }

  // Loop is in byte space, not code point space
  word result = start;
  // TODO(T41400083): Use a different search algorithm
  for (word i = start_index; i <= end_index - needle.length(); result++) {
    bool has_match = strHasPrefix(haystack, needle, i);
    word next = haystack.offsetByCodePoints(i, 1);
    if (i == next) {
      // We've reached a fixpoint; offsetByCodePoints will not advance past the
      // length of the string.
      if (start_index >= i) {
        // The start is greater than the length of the string.
        return -1;
      }
      // If the start is within bounds, just return the last found index.
      break;
    }
    if (has_match) {
      return result;
    }
    i = next;
  }
  return -1;
}

word strFindAsciiChar(const Str& haystack, byte needle) {
  DCHECK(needle <= kMaxASCII, "must only be called for ASCII `needle`");
  for (word i = 0, length = haystack.length(); i < length; i++) {
    if (haystack.byteAt(i) == needle) {
      return i;
    }
  }
  return -1;
}

word strFindFirstNonWhitespace(const Str& str) {
  word i = 0;
  for (word codepoint_len, length = str.length(); i < length;
       i += codepoint_len) {
    if (!Unicode::isSpace(str.codePointAt(i, &codepoint_len))) return i;
  }
  return i;
}

bool strHasPrefix(const Str& str, const Str& prefix, word start) {
  word str_len = str.length();
  word prefix_len = prefix.length();
  if (str_len - start < prefix_len) {
    return false;
  }
  for (word i = 0; i < prefix_len; i++) {
    if (str.byteAt(start + i) != prefix.byteAt(i)) {
      return false;
    }
  }
  return true;
}

bool strHasSurrogate(const Str& str) {
  return str.includesByte(UTF8::kSurrogateLeadByte);
}

word strRFind(const Str& haystack, const Str& needle, word start, word end) {
  // Haystack slice is empty; fast early return
  if (start > end) return -1;
  // Needle is empty
  if (needle == Str::empty()) return end;
  word start_index = haystack.offsetByCodePoints(0, start);
  if (start_index == haystack.length()) {
    // Haystack is too small; fast early return
    return -1;
  }
  word end_index = haystack.offsetByCodePoints(start_index, end - start);
  if ((end_index - start_index) < needle.length() || start_index > end_index) {
    // Haystack is too small; fast early return
    return -1;
  }
  // Loop is in byte space, not code point space
  // Invariant: cp_offset and byte_offset describe the same offset into the
  // string, but one is in code point space and the other is in byte space
  // TODO(T41400083): Use a different search algorithm
  for (word cp_offset = end - 1,
            byte_offset = haystack.offsetByCodePoints(end_index, -1);
       byte_offset >= 0; cp_offset--,
            byte_offset = haystack.offsetByCodePoints(byte_offset, -1)) {
    if (strHasPrefix(haystack, needle, byte_offset)) {
      return cp_offset;
    }
  }
  return -1;
}

word strRFindAsciiChar(const Str& haystack, byte needle) {
  DCHECK(needle <= kMaxASCII, "must only be called for ASCII `needle`");
  for (word i = haystack.length() - 1; i >= 0; i--) {
    if (haystack.byteAt(i) == needle) {
      return i;
    }
  }
  return -1;
}

RawObject METH(str, __le__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(self.compare(*other) <= 0);
}

RawObject METH(str, __len__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  return SmallInt::fromWord(self.codePointLength());
}

static RawObject strLowerASCII(Thread* thread, const Str& str, word length) {
  if (str.isSmallStr()) {
    byte buf[SmallStr::kMaxLength];
    for (word i = 0; i < length; i++) {
      buf[i] = ASCII::toLower(str.byteAt(i));
    }
    return SmallStr::fromBytes({buf, length});
  }
  // Search for the first uppercase character.
  word first_uppercase = 0;
  {
    RawLargeStr str_raw = LargeStr::cast(*str);
    for (; first_uppercase < length; first_uppercase++) {
      if (ASCII::isUpper(str_raw.byteAt(first_uppercase))) {
        break;
      }
    }
  }
  if (first_uppercase >= length) {
    return *str;
  }

  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  MutableBytes result(&scope, runtime->newMutableBytesUninitialized(length));
  result.replaceFromWithStr(0, *str, first_uppercase);
  for (word i = first_uppercase; i < length; i++) {
    byte lower = ASCII::toLower(str.byteAt(i));
    result.byteAtPut(i, lower);
  }
  return result.becomeStr();
}

RawObject METH(str, casefold)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word length = self.length();
  if (self.isASCII()) {
    return strLowerASCII(thread, self, length);
  }

  // Search for the first uppercase character.
  word first_uppercase = 0;
  for (word char_length; first_uppercase < length;
       first_uppercase += char_length) {
    int32_t code_point = self.codePointAt(first_uppercase, &char_length);
    if (Unicode::isUpper(code_point) || Unicode::isTitle(code_point) ||
        Unicode::isUnfolded(code_point)) {
      break;
    }
  }
  if (first_uppercase >= length && self_obj.isStr()) {
    return *self_obj;
  }

  StrArray result(&scope, runtime->newStrArray());
  runtime->strArrayEnsureCapacity(thread, result, length);
  // Since the prefix is valid UTF-8 and guaranteed to fit, it's safe to write
  // directly to the underlying MutableBytes.
  MutableBytes result_bytes(&scope, result.items());
  result_bytes.replaceFromWithStr(0, *self, first_uppercase);
  result.setNumItems(first_uppercase);
  for (word char_length, i = first_uppercase; i < length; i += char_length) {
    struct FullCasing casefold =
        Unicode::toFolded(self.codePointAt(i, &char_length));
    for (word j = 0; j < 3; j++) {
      int32_t decoded = casefold.code_points[j];
      if (decoded == -1) break;
      runtime->strArrayAddCodePoint(thread, result, decoded);
    }
  }
  return runtime->strFromStrArray(result);
}

RawObject METH(str, lower)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word length = self.length();
  if (self.isASCII()) {
    return strLowerASCII(thread, self, length);
  }

  // Search for the first uppercase character.
  word first_uppercase = 0;
  for (word char_length; first_uppercase < length;
       first_uppercase += char_length) {
    int32_t code_point = self.codePointAt(first_uppercase, &char_length);
    if (Unicode::isUpper(code_point) || Unicode::isTitle(code_point)) {
      break;
    }
  }
  if (first_uppercase >= length && self_obj.isStr()) {
    return *self_obj;
  }

  StrArray result(&scope, runtime->newStrArray());
  runtime->strArrayEnsureCapacity(thread, result, length);
  // Since the prefix is valid UTF-8 and guaranteed to fit, it's safe to write
  // directly to the underlying MutableBytes.
  MutableBytes result_bytes(&scope, result.items());
  result_bytes.replaceFromWithStr(0, *self, first_uppercase);
  result.setNumItems(first_uppercase);
  for (word char_length, i = first_uppercase; i < length; i += char_length) {
    struct FullCasing lower =
        Unicode::toLower(self.codePointAt(i, &char_length));
    for (word j = 0; j < 3; j++) {
      int32_t decoded = lower.code_points[j];
      if (decoded == -1) break;
      runtime->strArrayAddCodePoint(thread, result, decoded);
    }
  }
  return runtime->strFromStrArray(result);
}

RawObject METH(str, title)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }

  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();

  bool previous_is_cased = false;
  StrArray result(&scope, runtime->newStrArray());
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);

    FullCasing mapped = previous_is_cased ? Unicode::toLower(code_point)
                                          : Unicode::toTitle(code_point);
    for (word j = 0; mapped.code_points[j] != -1; j++) {
      runtime->strArrayAddCodePoint(thread, result, mapped.code_points[j]);
    }

    previous_is_cased = Unicode::isCased(code_point);
  }

  return runtime->strFromStrArray(result);
}

RawObject METH(str, upper)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word length = self.length();
  if (self.isASCII()) {
    if (self.isSmallStr()) {
      byte buf[SmallStr::kMaxLength];
      for (word i = 0; i < length; i++) {
        buf[i] = ASCII::toUpper(self.byteAt(i));
      }
      return SmallStr::fromBytes({buf, length});
    }
    // Search for the first lowercase character.
    word first_lowercase = 0;
    for (; first_lowercase < length; first_lowercase++) {
      if (ASCII::isLower(self.byteAt(first_lowercase))) {
        break;
      }
    }
    if (first_lowercase >= length && self_obj.isStr()) {
      return *self_obj;
    }

    MutableBytes result(&scope, runtime->newMutableBytesUninitialized(length));
    result.replaceFromWithStr(0, *self, first_lowercase);
    for (word i = first_lowercase; i < length; i++) {
      byte lower = ASCII::toUpper(self.byteAt(i));
      result.byteAtPut(i, lower);
    }
    return result.becomeStr();
  }

  // Search for the first lowercase character.
  word first_lowercase = 0;
  for (word char_length; first_lowercase < length;
       first_lowercase += char_length) {
    int32_t code_point = self.codePointAt(first_lowercase, &char_length);
    if (Unicode::isLower(code_point) || Unicode::isTitle(code_point)) {
      break;
    }
  }
  if (first_lowercase >= length && self_obj.isStr()) {
    return *self_obj;
  }

  StrArray result(&scope, runtime->newStrArray());
  runtime->strArrayEnsureCapacity(thread, result, length);
  // Since the prefix is valid UTF-8 and guaranteed to fit, it's safe to write
  // directly to the underlying MutableBytes.
  MutableBytes result_bytes(&scope, result.items());
  result_bytes.replaceFromWithStr(0, *self, first_lowercase);
  result.setNumItems(first_lowercase);
  for (word char_length, i = first_lowercase; i < length; i += char_length) {
    struct FullCasing upper =
        Unicode::toUpper(self.codePointAt(i, &char_length));
    for (word j = 0; j < 3; j++) {
      int32_t decoded = upper.code_points[j];
      if (decoded == -1) break;
      runtime->strArrayAddCodePoint(thread, result, decoded);
    }
  }
  return runtime->strFromStrArray(result);
}

RawObject METH(str, __lt__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(self.compare(*other) < 0);
}

RawObject METH(str, __mul__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Object count_index(&scope, args.get(1));
  Object count_obj(&scope, intFromIndex(thread, count_index));
  if (count_obj.isError()) return *count_obj;
  word count = intUnderlying(*count_obj).asWordSaturated();
  if (!SmallInt::isValid(count)) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "cannot fit '%T' into an index-sized integer",
                                &count_obj);
  }
  Str self(&scope, *self_obj);
  word length = self.length();
  if (count <= 0 || length == 0) {
    return Str::empty();
  }
  word new_length;
  if (__builtin_mul_overflow(length, count, &new_length) ||
      !SmallInt::isValid(new_length)) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "repeated string is too long");
  }
  return runtime->strRepeat(thread, self, count);
}

RawObject METH(str, __ne__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object other_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  if (!thread->runtime()->isInstanceOfStr(*other_obj)) {
    return NotImplementedType::object();
  }
  Str self(&scope, strUnderlying(*self_obj));
  Str other(&scope, strUnderlying(*other_obj));
  return Bool::fromBool(!self.equals(*other));
}

RawObject METH(str, __iter__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  return runtime->newStrIterator(self);
}

RawObject METH(str, __repr__)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  const word self_len = self.length();
  word result_len = 0;
  word squote = 0;
  word dquote = 0;
  // Precompute the size so that only one allocation is necessary.
  for (word i = 0, char_len; i < self_len; i += char_len) {
    int32_t code_point = self.codePointAt(i, &char_len);
    if (code_point == '\'') {
      squote++;
      result_len += 1;
    } else if (code_point == '"') {
      dquote++;
      result_len += 1;
    } else if (code_point == '\\' || code_point == '\t' || code_point == '\r' ||
               code_point == '\n') {
      result_len += 2;
    } else if (Unicode::isPrintable(code_point)) {
      result_len += char_len;
    } else if (code_point < 0x100) {
      result_len += 4;
    } else if (code_point < 0x10000) {
      result_len += 6;
    } else {
      result_len += 10;
    }
  }

  byte quote = '\'';
  bool unchanged = (result_len == self_len);
  if (squote > 0) {
    unchanged = false;
    // If there are both single quotes and double quotes, the outer quote will
    // be singles, and all internal quotes will need to be escaped.
    if (dquote > 0) {
      // Add the size of the escape backslashes on the single quotes.
      result_len += squote;
    } else {
      quote = '"';
    }
  }
  result_len += 2;  // quotes

  MutableBytes buf(&scope, runtime->newMutableBytesUninitialized(result_len));
  buf.byteAtPut(0, quote);
  buf.byteAtPut(result_len - 1, quote);
  if (unchanged) {
    // Remaining characters were unmodified, so copy them directly.
    buf.replaceFromWithStr(1, *self, self_len);
    return buf.becomeStr();
  }
  word out = 1;
  for (word in = 0, char_len; in < self_len; in += char_len) {
    int32_t code_point = self.codePointAt(in, &char_len);
    if (code_point == quote) {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, quote);
    } else if (code_point == '\\') {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, '\\');
    } else if (code_point == '\t') {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 't');
    } else if (code_point == '\r') {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'r');
    } else if (code_point == '\n') {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'n');
    } else if (' ' <= code_point && code_point < kMaxASCII) {
      buf.byteAtPut(out++, code_point);
    } else if (code_point <= kMaxASCII) {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'x');
      uwordToHexadecimalWithMutableBytes(*buf, /*index=*/out, /*num_digits=*/2,
                                         code_point);
      out += 2;
    } else if (Unicode::isPrintable(code_point)) {
      for (word i = 0; i < char_len; i++) {
        buf.byteAtPut(out + i, self.byteAt(in + i));
      }
      out += char_len;
    } else if (code_point <= 0xff) {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'x');
      uwordToHexadecimalWithMutableBytes(*buf, /*index=*/out, /*num_digits=*/2,
                                         code_point);
      out += 2;
    } else if (code_point <= 0xffff) {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'u');
      uwordToHexadecimalWithMutableBytes(*buf, /*index=*/out, /*num_digits=*/4,
                                         code_point);
      out += 4;
    } else {
      buf.byteAtPut(out++, '\\');
      buf.byteAtPut(out++, 'U');
      uwordToHexadecimalWithMutableBytes(*buf, /*index=*/out,
                                         /*num_digits=*/8, code_point);
      out += 8;
    }
  }
  DCHECK(out == result_len - 1, "wrote %ld characters, expected %ld", out - 1,
         result_len - 2);
  return buf.becomeStr();
}

bool METH(str, _mod_convert_number_int_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackTop();
  if (arg.isBool()) {
    thread->stackDrop(2);
    thread->stackSetTop(convertBoolToInt(arg));
    return true;
  }
  if (arg.isInt()) {
    thread->stackDrop(2);
    thread->stackSetTop(arg);
    return true;
  }
  return false;
}

bool METH(str, _mod_convert_number_index_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackTop();
  if (arg.isBool()) {
    thread->stackDrop(2);
    thread->stackSetTop(convertBoolToInt(arg));
    return true;
  }
  if (arg.isInt()) {
    thread->stackDrop(2);
    thread->stackSetTop(arg);
    return true;
  }
  return false;
}

bool METH(str, _mod_check_single_arg_intrinsic)(Thread* thread) {
  Runtime* runtime = thread->runtime();
  RawObject arg = thread->stackTop();
  if (runtime->isInstanceOfTuple(arg)) {
    RawTuple arg_tuple = tupleUnderlying(arg);
    if (arg_tuple.length() != 1) return false;
    thread->stackDrop(2);
    thread->stackSetTop(arg_tuple);
    return true;
  }
  RawMutableTuple result = MutableTuple::cast(runtime->newMutableTuple(1));
  // Note that we need to re-fetch stackTop() since newMutableTuple() may
  // have triggered GC.
  result.atPut(0, thread->stackTop());
  thread->stackDrop(2);
  thread->stackSetTop(result.becomeImmutable());
  return true;
}

RawObject METH(str, isalnum)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isAlnum(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject METH(str, isalpha)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isAlpha(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject METH(str, isascii)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  return Bool::fromBool(self.isASCII());
}

RawObject METH(str, isdecimal)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isDecimal(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject METH(str, isdigit)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isDigit(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

bool strIsIdentifier(const Str& str) {
  word char_length = str.length();
  if (char_length == 0) {
    return false;
  }
  word len;
  int32_t first = str.codePointAt(0, &len);
  if (!Unicode::isXidStart(first) && first != '_') {
    return false;
  }
  for (word i = len; i < char_length; i += len) {
    int32_t code_point = str.codePointAt(i, &len);
    if (!Unicode::isXidContinue(code_point)) {
      return false;
    }
  }
  return true;
}

RawObject METH(str, isidentifier)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  return Bool::fromBool(strIsIdentifier(self));
}

RawObject METH(str, islower)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  bool cased = false;
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (Unicode::isUpper(code_point) || Unicode::isTitle(code_point)) {
      return Bool::falseObj();
    }
    if (!cased && Unicode::isLower(code_point)) {
      cased = true;
    }
  }
  return Bool::fromBool(cased);
}

RawObject METH(str, isnumeric)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isNumeric(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject METH(str, isprintable)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (!Unicode::isPrintable(code_point)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject METH(str, isspace)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  if (char_length == 0) {
    return Bool::falseObj();
  }
  if (char_length == 1) {
    return ASCII::isSpace(self.byteAt(0)) ? Bool::trueObj() : Bool::falseObj();
  }
  word byte_index = 0;
  do {
    word num_bytes;
    int32_t codepoint = self.codePointAt(byte_index, &num_bytes);
    if (!Unicode::isSpace(codepoint)) {
      return Bool::falseObj();
    }
    byte_index += num_bytes;
  } while (byte_index < char_length);
  return Bool::trueObj();
}

RawObject METH(str, istitle)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  bool cased = false;
  bool previous_is_cased = false;
  word char_length = self.length();
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (Unicode::isUpper(code_point) || Unicode::isTitle(code_point)) {
      if (previous_is_cased) return Bool::falseObj();
      cased = true;
      previous_is_cased = true;
    } else if (Unicode::isLower(code_point)) {
      if (!previous_is_cased) return Bool::falseObj();
      previous_is_cased = true;
      cased = true;
    } else {
      previous_is_cased = false;
    }
  }
  return Bool::fromBool(cased);
}

RawObject METH(str, isupper)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();
  bool cased = false;
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (Unicode::isLower(code_point) || Unicode::isTitle(code_point)) {
      return Bool::falseObj();
    }
    if (!cased && Unicode::isUpper(code_point)) {
      cased = true;
    }
  }
  return Bool::fromBool(cased);
}

RawObject METH(str, lstrip)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str str(&scope, strUnderlying(*self_obj));
  Object other_obj(&scope, args.get(1));
  if (other_obj.isNoneType()) {
    return strStripSpaceLeft(thread, str);
  }
  if (!runtime->isInstanceOfStr(*other_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "str.lstrip() arg must be None or str");
  }
  Str chars(&scope, strUnderlying(*other_obj));
  return strStripLeft(thread, str, chars);
}

RawObject METH(str, rstrip)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str str(&scope, strUnderlying(*self_obj));
  Object other_obj(&scope, args.get(1));
  if (other_obj.isNoneType()) {
    return strStripSpaceRight(thread, str);
  }
  if (!runtime->isInstanceOfStr(*other_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "str.rstrip() arg must be None or str");
  }
  Str chars(&scope, strUnderlying(*other_obj));
  return strStripRight(thread, str, chars);
}

RawObject METH(str, strip)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str str(&scope, strUnderlying(*self_obj));
  Object other_obj(&scope, args.get(1));
  if (other_obj.isNoneType()) {
    return strStripSpace(thread, str);
  }
  if (!runtime->isInstanceOfStr(*other_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "str.strip() arg must be None or str");
  }
  Str chars(&scope, strUnderlying(*other_obj));
  return strStrip(thread, str, chars);
}

static int32_t handleCapitalSigma(const Str& str, word i) {
  bool final_sigma = false;
  for (word j = str.offsetByCodePoints(i, -1), len; j >= 0;
       j = str.offsetByCodePoints(j, -1)) {
    int32_t code_point = str.codePointAt(j, &len);
    if (!Unicode::isCaseIgnorable(code_point)) {
      final_sigma = Unicode::isCased(code_point);
      break;
    }
  }
  if (!final_sigma) {
    return 0x03C3;
  }

  word char_length = str.length();
  for (word j = str.offsetByCodePoints(i, 1), len; j < char_length; j += len) {
    int32_t code_point = str.codePointAt(j, &len);
    if (!Unicode::isCaseIgnorable(code_point)) {
      return Unicode::isCased(code_point) ? 0x03C3 : 0x03C2;
    }
  }
  return 0x03C2;
}

RawObject METH(str, swapcase)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }
  Str self(&scope, strUnderlying(*self_obj));
  word char_length = self.length();

  // Most of the time, this will be sufficient. However, due to Unicode casing,
  // it's possible that we could need up to 3 times as much space as the input.
  StrArray result(&scope, runtime->newStrArray());
  runtime->strArrayEnsureCapacity(thread, result, char_length);
  for (word i = 0, len; i < char_length; i += len) {
    int32_t code_point = self.codePointAt(i, &len);
    if (Unicode::isUpper(code_point)) {
      if (code_point == 0x03a3) {
        runtime->strArrayAddCodePoint(thread, result,
                                      handleCapitalSigma(self, i));
      } else {
        FullCasing lower = Unicode::toLower(code_point);
        for (word j = 0; j < 3; j++) {
          int32_t decoded = lower.code_points[j];
          if (decoded == -1) break;
          runtime->strArrayAddCodePoint(thread, result, decoded);
        }
      }
    } else {
      FullCasing upper = Unicode::toUpper(code_point);
      for (word j = 0; j < 3; j++) {
        int32_t decoded = upper.code_points[j];
        if (decoded == -1) break;
        runtime->strArrayAddCodePoint(thread, result, decoded);
      }
    }
  }
  return runtime->strFromStrArray(result);
}

RawObject METH(str_iterator, __iter__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isStrIterator()) {
    return thread->raiseRequiresType(self, ID(str_iterator));
  }
  return *self;
}

// TODO(T35578204) Implement this for UTF-8. This probably means keeping extra
// state and logic so that __next__() will advance to the next codepoint.

RawObject METH(str_iterator, __next__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isStrIterator()) {
    return thread->raiseRequiresType(self, ID(str_iterator));
  }
  StrIterator iter(&scope, *self);
  Object value(&scope, strIteratorNext(thread, iter));
  if (value.isError()) {
    return thread->raise(LayoutId::kStopIteration, NoneType::object());
  }
  return *value;
}

RawObject METH(str_iterator, __length_hint__)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isStrIterator()) {
    return thread->raiseRequiresType(self, ID(str_iterator));
  }
  StrIterator str_iterator(&scope, *self);
  Str str(&scope, str_iterator.iterable());
  return SmallInt::fromWord(str.length() - str_iterator.index());
}

}  // namespace py
