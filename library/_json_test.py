#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import sys
import unittest

from test_support import cpython_only


if sys.implementation.name == "pyro":
    from _json import loads, JSONDecodeError
else:
    from json import loads, JSONDecodeError


class LoadsTests(unittest.TestCase):
    def test_with_bytes_utf8_no_bom_returns_str(self):
        self.assertEqual(loads(b'""'), "")
        self.assertEqual(loads(b'"hello"'), "hello")
        self.assertEqual(loads(b'"long string \\n blabla"'), "long string \n blabla")
        self.assertEqual(
            loads(b'"\xf0\x9f\x91\x8d\xf0\x9f\x90\x8d"'), "\U0001f44d\U0001f40d"
        )

    def test_with_bytes_utf8_bom_returns_str(self):
        self.assertEqual(loads(b'\xef\xbb\xbf""'), "")
        self.assertEqual(loads(b'\xef\xbb\xbf"hello"'), "hello")
        self.assertEqual(
            loads(b'\xef\xbb\xbf"long string \\n blabla"'), "long string \n blabla"
        )
        self.assertEqual(
            loads(b'\xef\xbb\xbf"\xf0\x9f\x91\x8d\xf0\x9f\x90\x8d"'),
            "\U0001f44d\U0001f40d",
        )

    def test_with_bytearray_utf8_returns_str(self):
        self.assertEqual(loads(bytearray(b'""')), "")
        self.assertEqual(loads(bytearray(b'"hello"')), "hello")
        self.assertEqual(
            loads(bytearray(b'"long string \\n blabla"')), "long string \n blabla"
        )
        self.assertEqual(
            loads(bytearray(b'"\xf0\x9f\x91\x8d\xf0\x9f\x90\x8d"')),
            "\U0001f44d\U0001f40d",
        )
        self.assertEqual(
            loads(bytearray(b'\xef\xbb\xbf"\xf0\x9f\x91\x8d\xf0\x9f\x90\x8d"')),
            "\U0001f44d\U0001f40d",
        )

    # TODO(T81477911) Implement UTF-16 decoding.
    @cpython_only
    def test_with_bytes_utf16_le_bom_returns_object(self):
        self.assertEqual(loads(b"\xff\xfe1\x002\x003\x00"), 123)
        self.assertEqual(loads(b'\xff\xfe"\x00\xaf!"\x00'), "\u21af")
        self.assertEqual(loads(b'\xff\xfe"\x00<\xd8\x84\xdf"\x00'), "\U0001f384")
        self.assertEqual(
            loads(bytearray(b'\xff\xfe"\x00<\xd8\x84\xdf"\x00')), "\U0001f384"
        )

    # TODO(T81477911) Implement UTF-16 decoding.
    @cpython_only
    def test_with_bytes_utf16_le_no_bom_returns_object(self):
        self.assertEqual(loads(b"1\x002\x003\x00"), 123)
        self.assertEqual(loads(b'"\x00\xaf!"\x00'), "\u21af")
        self.assertEqual(loads(b'"\x00<\xd8\x84\xdf"\x00'), "\U0001f384")
        self.assertEqual(loads(bytearray(b'"\x00<\xd8\x84\xdf"\x00')), "\U0001f384")

    # TODO(T81477911) Implement UTF-16 decoding.
    @cpython_only
    def test_with_bytes_utf16_be_bom_returns_object(self):
        self.assertEqual(loads(b"\xfe\xff\x005\x004\x007"), 547)
        self.assertEqual(loads(b'\xfe\xff\x00"!\xc4\x00"'), "\u21c4")
        self.assertEqual(loads(b'\xfe\xff\x00"\xd8>\xdd]\x00"'), "\U0001f95d")
        self.assertEqual(
            loads(bytearray(b'\xfe\xff\x00"\xd8>\xdd]\x00"')), "\U0001f95d"
        )

    # TODO(T81477911) Implement UTF-16 decoding.
    @cpython_only
    def test_with_bytes_utf16_be_no_bom_returns_object(self):
        self.assertEqual(loads(b"\x005\x004\x007"), 547)
        self.assertEqual(loads(b'\x00"!\xc4\x00"'), "\u21c4")
        self.assertEqual(loads(b'\x00"\xd8>\xdd]\x00"'), "\U0001f95d")
        self.assertEqual(loads(bytearray(b'\x00"\xd8>\xdd]\x00"')), "\U0001f95d")

    # TODO(T81478020) Implement UTF-32 decoding.
    @cpython_only
    def test_with_bytes_utf32_le_bom_returns_object(self):
        self.assertEqual(loads(b"\xff\xfe\x00\x007\x00\x00\x006\x00\x00\x00"), 76)
        self.assertEqual(
            loads(b'\xff\xfe\x00\x00"\x00\x00\x00Q\xf9\x01\x00"\x00\x00\x00'),
            "\U0001f951",
        )

    # TODO(T81478020) Implement UTF-32 decoding.
    @cpython_only
    def test_with_bytes_utf32_le_no_bom_returns_object(self):
        self.assertEqual(loads(b"7\x00\x00\x006\x00\x00\x00"), 76)
        self.assertEqual(
            loads(b'"\x00\x00\x00Q\xf9\x01\x00"\x00\x00\x00'), "\U0001f951"
        )

    # TODO(T81478020) Implement UTF-32 decoding.
    @cpython_only
    def test_with_bytes_utf32_be_bom_returns_object(self):
        self.assertEqual(loads(b"\x00\x00\xfe\xff\x00\x00\x00-\x00\x00\x005"), -5)
        self.assertEqual(
            loads(b'\x00\x00\xfe\xff\x00\x00\x00"\x00\x01\xf9W\x00\x00\x00"'),
            "\U0001f957",
        )

    # TODO(T81478020) Implement UTF-32 decoding.
    @cpython_only
    def test_with_bytes_utf32_be_no_bom_returns_object(self):
        self.assertEqual(loads(b"\x00\x00\x00-\x00\x00\x005"), -5)
        self.assertEqual(
            loads(b'\x00\x00\x00"\x00\x01\xf9W\x00\x00\x00"'), "\U0001f957"
        )

    def test_number_returns_int(self):
        self.assertEqual(loads("0"), 0)
        self.assertEqual(loads("1"), 1)
        self.assertEqual(loads("2"), 2)
        self.assertEqual(loads("3"), 3)
        self.assertEqual(loads("4"), 4)
        self.assertEqual(loads("5"), 5)
        self.assertEqual(loads("6"), 6)
        self.assertEqual(loads("7"), 7)
        self.assertEqual(loads("8"), 8)
        self.assertEqual(loads("9"), 9)
        self.assertEqual(loads("10"), 10)
        self.assertEqual(loads("91284647"), 91284647)
        self.assertEqual(loads("-1"), -1)
        self.assertEqual(loads("-42"), -42)
        self.assertEqual(loads("-0"), 0)
        self.assertEqual(loads("1000000000000000000"), 1000000000000000000)
        self.assertEqual(loads("-1000000000000000000"), -1000000000000000000)

    def test_large_number_returns_int(self):
        self.assertEqual(loads("10000000000000000000"), 10000000000000000000)
        self.assertEqual(loads("-10000000000000000000"), -10000000000000000000)
        self.assertEqual(
            loads("123456789123456789123456789123456789123456789"),
            123456789123456789123456789123456789123456789,
        )
        self.assertEqual(
            loads("-666666666666666666666666666666"), -666666666666666666666666666666
        )

    def test_number_with_whitespace_returns_int(self):
        self.assertEqual(loads("0 "), 0)
        self.assertEqual(loads("\r0"), 0)
        self.assertEqual(loads("-1\t"), -1)
        self.assertEqual(loads("\n-1"), -1)
        self.assertEqual(loads("99 "), 99)
        self.assertEqual(loads("\t99"), 99)
        self.assertEqual(loads(" \r\n\t-5700 "), -5700)
        self.assertEqual(loads(" -5700\t\r \t\t \n"), -5700)

    def test_number_calls_parse_int(self):
        arg = None
        marker = object()

        def func(string):
            nonlocal arg
            arg = string
            return marker

        self.assertIs(loads("12", parse_int=func), marker)
        self.assertEqual(arg, "12")
        self.assertIs(loads(" 0\t", parse_int=func), marker)
        self.assertEqual(arg, "0")
        self.assertIs(loads("-712\r\n", parse_int=func), marker)
        self.assertEqual(arg, "-712")
        self.assertIs(
            loads("   1234567890123456789012345687898239746924673564", parse_int=func),
            marker,
        )
        self.assertEqual(arg, "1234567890123456789012345687898239746924673564")
        self.assertIs(
            loads("-12233344445555566666677777778888888999999999", parse_int=func),
            marker,
        )
        self.assertEqual(arg, "-12233344445555566666677777778888888999999999")

    def test_minus_without_digit_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("-")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("-a")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("- 5")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("- Infinity")

    def test_leading_zeros_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("00")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("04")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("099")

    def test_number_returns_float(self):
        self.assertEqual(str(loads("0.0")), "0.0")
        self.assertEqual(str(loads("-0.0")), "-0.0")
        self.assertEqual(str(loads("1.0")), "1.0")
        self.assertEqual(str(loads("42.0")), "42.0")
        self.assertEqual(str(loads("-312.0")), "-312.0")
        self.assertEqual(
            str(loads("987654321987654321987654321987654321.0")),
            "9.876543219876544e+35",
        )

    def test_number_with_fraction_returns_float(self):
        self.assertEqual(str(loads("0.5")), "0.5")
        self.assertEqual(str(loads("-3.125")), "-3.125")
        self.assertEqual(str(loads("99.987654321")), "99.987654321")
        self.assertEqual(str(loads("-123456789.987654321")), "-123456789.98765433")
        self.assertEqual(
            str(loads("987654321.987654321987654321987654321987654321")),
            "987654321.9876543",
        )

    def test_number_with_exponent_returns_flaot(self):
        self.assertEqual(str(loads("0e0")), "0.0")
        self.assertEqual(str(loads("0e+0")), "0.0")
        self.assertEqual(str(loads("0e-0")), "0.0")
        self.assertEqual(str(loads("0e000")), "0.0")
        self.assertEqual(str(loads("0e-00")), "0.0")
        self.assertEqual(str(loads("0e-0000")), "0.0")
        self.assertEqual(str(loads("0e+1000")), "0.0")
        self.assertEqual(str(loads("0e-1000")), "0.0")

        self.assertEqual(str(loads("1e4")), "10000.0")
        self.assertEqual(str(loads("5e+5")), "500000.0")
        self.assertEqual(
            str(loads("5e+0000000000000000000000000000000000005")), "500000.0"
        )
        self.assertEqual(str(loads("10e-6")), "1e-05")
        self.assertEqual(str(loads("15e+77")), "1.5e+78")
        self.assertEqual(str(loads("1234e-98")), "1.234e-95")

    def test_number_returns_float_infinity(self):
        self.assertEqual(loads("1e10000"), float("inf"))
        self.assertEqual(loads("-3e10000"), float("-inf"))

    def test_number_returns_zero(self):
        self.assertEqual(str(loads("1e-10000")), "0.0")
        self.assertEqual(str(loads("-1e-10000")), "-0.0")

    def test_number_with_whitespace_returns_float(self):
        self.assertEqual(str(loads(" 0.0")), "0.0")
        self.assertEqual(str(loads("0.0 ")), "0.0")
        self.assertEqual(str(loads(" 0.0 ")), "0.0")

    def test_number_calls_parse_float(self):
        arg = None
        marker = object()

        def func(string):
            nonlocal arg
            arg = string
            return marker

        self.assertIs(loads("1.1", parse_float=func), marker)
        self.assertEqual(arg, "1.1")
        self.assertIs(loads(" -4.0\r\n", parse_float=func), marker)
        self.assertEqual(arg, "-4.0")
        self.assertIs(loads("41e81  ", parse_float=func), marker)
        self.assertEqual(arg, "41e81")
        self.assertIs(loads("\t -3.4E-7", parse_float=func), marker)
        self.assertEqual(arg, "-3.4E-7")

    def test_dot_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads(".")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads(".4")

    def test_number_dot_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("1.")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("2.a")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("3e")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("4e-")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("5e+")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("6e+x")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 4 \(char 3\)"
        ):
            loads("6.2e")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 2 \(char 1\)"
        ):
            loads("6ea")

    def test_string_returns_str(self):
        self.assertEqual(loads('""'), "")
        self.assertEqual(loads('" "'), " ")
        self.assertEqual(loads('"hello"'), "hello")
        self.assertEqual(loads('"hello y\'all"'), "hello y'all")

    def test_control_character_in_string_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid control character at: line 1 column 2 \(char 1\)"
        ):
            loads('"\x00"')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid control character at: line 1 column 7 \(char 6\)"
        ):
            loads('"hello\x01"')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid control character at: line 2 column 5 \(char 5\)"
        ):
            loads('\n"hel\x10lo"')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid control character at: line 1 column 4 \(char 3\)"
        ):
            loads('\t "\x1fhello"')

    def test_unterminated_string_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Unterminated string starting at: line 1 column 1 \(char 0\)",
        ):
            loads('"')
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Unterminated string starting at: line 2 column 1 \(char 2\)",
        ):
            loads('\t\n"he\nlo', strict=False)

    def test_with_strict_false_returns_str(self):
        self.assertEqual(loads('"he\x00llo"', strict=False), "he\x00llo")
        control_chars = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
        self.assertEqual(loads(f'"{control_chars}"', strict=False), control_chars)

    def test_string_with_escape_returns_str(self):
        self.assertEqual(loads('"\\""'), '"')
        self.assertEqual(loads('"\\\\"'), "\\")
        self.assertEqual(loads('"\\/"'), "/")
        self.assertEqual(loads('"\\b"'), "\b")
        self.assertEqual(loads('"\\f"'), "\f")
        self.assertEqual(loads('"\\n"'), "\n")
        self.assertEqual(loads('"\\r"'), "\r")
        self.assertEqual(loads('"\\t"'), "\t")

    def test_string_with_u_escape_returns_str(self):
        self.assertEqual(loads('"\\u0059"'), "Y")
        self.assertEqual(loads('"\\u1234"'), "\u1234")
        self.assertEqual(loads('"\\ucafe"'), "\ucafe")
        self.assertEqual(loads('"\\uCAFE"'), "\ucafe")
        self.assertEqual(loads('"\\uCa00"'), "\uca00")
        self.assertEqual(loads('"\\u99fE"'), "\u99fe")

    def test_string_with_u_escape_combines_surrogates(self):
        self.assertEqual(loads('"\\ud83e\\udd20"'), "\U0001f920")
        self.assertEqual(loads('"\\udbff\\udfff"'), "\U0010ffff")

    def test_string_with_lone_surrogates_returns_str(self):
        self.assertEqual(loads('"\\ud83e"'), "\ud83e")
        self.assertEqual(loads('"\\ud83e\\ud83e"'), "\ud83e\ud83e")
        self.assertEqual(loads('"\\udd20"'), "\udd20")
        self.assertEqual(loads('"\\udd20\\ud83e"'), "\udd20\ud83e")

    def test_string_with_whitespace_returns_str(self):
        self.assertEqual(loads(' ""'), "")
        self.assertEqual(loads('"" '), "")
        self.assertEqual(loads(' "" '), "")

    def test_chars_after_string_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 3 \(char 2\)"
        ):
            loads('""a')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 7 \(char 6\)"
        ):
            loads('""    ""')

    def test_string_with_unterminated_escape_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Unterminated string starting at: line 1 column 1 \(char 0\)",
        ):
            loads('"\\')

    def test_string_with_invalid_escape_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\escape: line 1 column 2 \(char 1\)"
        ):
            loads('"\\x"')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\escape: line 1 column 2 \(char 1\)"
        ):
            loads('"\\\U0001f974"')

    def test_string_with_invalid_u_escape_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\uXXXX escape: line 1 column 3 \(char 2\)"
        ):
            loads('"\\u')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\uXXXX escape: line 1 column 3 \(char 2\)"
        ):
            loads('"\\u123')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\uXXXX escape: line 1 column 3 \(char 2\)"
        ):
            loads('"\\u000g')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Invalid \\uXXXX escape: line 1 column 9 \(char 8\)"
        ):
            loads('"\\ud83e\\u5x"')

    def test_true_returns_bool(self):
        self.assertIs(loads("true"), True)

    def test_false_returns_bool(self):
        self.assertIs(loads("false"), False)

    def test_null_returns_none(self):
        self.assertIs(loads("null"), None)

    def test_infinity_returns_float(self):
        self.assertEqual(loads("Infinity"), float("inf"))

    def test_minus_infinity_returns_float(self):
        self.assertEqual(loads("-Infinity"), float("-inf"))

    def test_nan_returns_float(self):
        self.assertEqual(str(loads("NaN")), "nan")

    def test_whitespace_around_constant_is_ignored(self):
        self.assertIs(loads("true "), True)
        self.assertIs(loads("\tfalse"), False)
        self.assertIs(loads("\r null \n"), None)
        self.assertEqual(loads("  Infinity   "), float("inf"))
        self.assertEqual(loads("\n\r-Infinity\t"), float("-inf"))
        self.assertEqual(str(loads("\r\nNaN\t")), "nan")

    def test_calls_parse_constant(self):
        arg = None
        marker = object()

        def func(string):
            nonlocal arg
            arg = string
            return marker

        self.assertIs(loads("NaN", parse_constant=func), marker)
        self.assertEqual(arg, "NaN")
        self.assertIs(loads(" -Infinity\t\r", parse_constant=func), marker)
        self.assertEqual(arg, "-Infinity")
        self.assertIs(loads("  Infinity\n\r", parse_constant=func), marker)
        self.assertEqual(arg, "Infinity")

    def test_does_not_call_parse_constant(self):
        def func(string):
            raise Exception("should not be called")

        self.assertIs(loads("true", parse_constant=func), True)
        self.assertIs(loads("false", parse_constant=func), False)
        self.assertIs(loads("null", parse_constant=func), None)

    def test_parse_constant_propagates_exception(self):
        def func(string):
            raise UserWarning("test")

        with self.assertRaises(UserWarning):
            loads("NaN", parse_constant=func)

    def test_constant_with_extra_chars_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 5 \(char 4\)"
        ):
            loads("truee")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 1 column 7 \(char 6\)"
        ):
            loads("false null")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Extra data: line 2 column 2 \(char 5\)"
        ):
            loads("NaN\n\ta")

    def test_whitespace_string_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 2 \(char 1\)"
        ):
            loads(" ")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 2 column 1 \(char 4\)"
        ):
            loads("\t\r \n")

    def test_empty_string_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("")

    def test_list_returns_list(self):
        result = loads("[1,2,3]")
        self.assertIs(type(result), list)
        self.assertEqual(result, [1, 2, 3])

    def test_list_misc_returns_list(self):
        self.assertEqual(loads('[  "hello","world",\r\n42]'), ["hello", "world", 42])
        self.assertEqual(
            loads("[true, false, null, 13.13   \r]"), [True, False, None, 13.13]
        )
        self.assertEqual(
            loads('\n[5,\r-Infinity,\t"",Infinity]'),
            [5, float("-inf"), "", float("inf")],
        )
        result = loads("[13, NaN]")
        self.assertEqual(result[0], 13)
        self.assertEqual(str(result[1]), "nan")

    def test_nested_lists_returns_list(self):
        self.assertEqual(loads("[[]]"), [[]])
        self.assertEqual(loads("[1, []  ]"), [1, []])
        self.assertEqual(loads('[\t[], "test"]'), [[], "test"])
        self.assertEqual(
            loads('[[\r[[[]\t]\n], [-4.5, [[], ["a"] ]]]]'),
            [[[[[]]], [-4.5, [[], ["a"]]]]],
        )

    def test_empty_list_returns_list(self):
        self.assertEqual(loads("[]"), [])
        self.assertEqual(loads(" []"), [])
        self.assertEqual(loads("[ ]"), [])
        self.assertEqual(loads("[] "), [])

    def test_deep_list_nesting_raises_recursion_error(self):
        with self.assertRaises(RecursionError):
            loads("[" * 2000000 + "]" * 2000000)

    def test_unbalanced_lists_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 2 \(char 1\)"
        ):
            loads("[")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 4 \(char 3\)"
        ):
            loads("[[]")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 14 \(char 13\)"
        ):
            loads("[[], [], [[]]")

    def test_missing_list_delimiter_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 4 \(char 3\)"
        ):
            loads("[1 2]")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 4 \(char 3\)"
        ):
            loads('[""')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 4 \(char 3\)"
        ):
            loads("[[]4.4]")

    def test_dict_returns_dict(self):
        result = loads('{"foo": "bar", "baz": 42}')
        self.assertIs(type(result), dict)
        self.assertEqual(list(result.keys()), ["foo", "baz"])
        self.assertEqual(list(result.values()), ["bar", 42])

    def test_dict_misc_returns_dict(self):
        self.assertEqual(loads('{"": ""}'), {"": ""})
        self.assertEqual(
            loads(
                '{\t"foo": [], "1"\n:\r2, "3"  :4.4,"4":\r\ntrue, "5":   Infinity\t}'
            ),
            {"foo": [], "1": 2, "3": 4.4, "4": True, "5": float("inf")},
        )

    def test_dict_with_repeatet_keys_returns_dict(self):
        self.assertEqual(loads('{"a": [], "a": 7.7}'), {"a": 7.7})
        self.assertEqual(
            loads('{"a": 42, "b": 7, "c": 4, "a": -8, "b": 11}'),
            {"a": -8, "b": 11, "c": 4},
        )

    def test_empty_dict_returns_dict(self):
        self.assertEqual(loads("{}"), {})
        self.assertEqual(loads(" {}"), {})
        self.assertEqual(loads("{ }"), {})
        self.assertEqual(loads("{} "), {})

    def test_dict_with_non_string_key_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Expecting property name enclosed in double quotes: line 1 column 2 \(char 1\)",
        ):
            loads("{42:1}")
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Expecting property name enclosed in double quotes: line 1 column 2 \(char 1\)",
        ):
            loads("{[]:1}")
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Expecting property name enclosed in double quotes: line 1 column 2 \(char 1\)",
        ):
            loads("{{}:1}")

    def test_dict_with_missing_colon_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ':' delimiter: line 1 column 4 \(char 3\)"
        ):
            loads('{""}')
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ':' delimiter: line 1 column 19 \(char 18\)"
        ):
            loads('{"foo": 42, "bar" 5}')

    def test_dict_with_extra_comma_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError,
            r"Expecting property name enclosed in double quotes: line 1 column 8 \(char 7\)",
        ):
            loads('{"": 4,}')

    def test_dict_with_missing_comma_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting ',' delimiter: line 1 column 8 \(char 7\)"
        ):
            loads('{"": 4 "a": 5}')

    def test_dict_with_deep_nesting_raises_recursion_error(self):
        with self.assertRaises(RecursionError):
            loads('{"":' * 2000000 + "{" + "}" * 2000000)

    def test_dict_calls_object_hook(self):
        arg = None
        marker = object()

        def func(d):
            nonlocal arg
            arg = d
            return marker

        self.assertIs(loads("{}", object_hook=func), marker)
        self.assertIs(type(arg), dict)
        self.assertEqual(arg, {})

        self.assertEqual(
            loads('[4, {"foo": 4, "aa": -8}, false]', object_hook=func),
            [4, marker, False],
        )
        self.assertIs(type(arg), dict)
        self.assertEqual(list(arg.keys()), ["foo", "aa"])
        self.assertEqual(list(arg.values()), [4, -8])

    def test_dict_calls_object_pairs_hook(self):
        arg = None
        marker = object()

        def func(pairs):
            nonlocal arg
            arg = pairs
            return marker

        self.assertIs(loads("{}", object_pairs_hook=func), marker)
        self.assertIs(type(arg), list)
        self.assertEqual(arg, [])

        self.assertEqual(
            loads('[4, {"b": 4, "a": -8, "c":null}, false]', object_pairs_hook=func),
            [4, marker, False],
        )
        self.assertIs(type(arg), list)
        self.assertIs(type(arg[0]), tuple)
        self.assertEqual(arg, [("b", 4), ("a", -8), ("c", None)])

    def test_dict_with_both_hooks_calls_object_pairs_hook(self):
        arg0 = None
        arg1 = None
        marker = object()

        def func0(d):
            nonlocal arg0
            arg0 = d
            raise Exception("should not be called")

        def func1(pairs):
            nonlocal arg1
            arg1 = pairs
            return marker

        self.assertIs(
            loads('{"foo": "baz"}', object_hook=func0, object_pairs_hook=func1), marker
        )
        self.assertIs(arg0, None)
        self.assertEqual(arg1, [("foo", "baz")])

    def test_dict_keys_are_merged(self):
        result = loads(
            '[{"foo": 5, "bar": 8, "a very long key": 11}, [{"a very long key": 4, "foo": []}]]'
        )
        self.assertEqual(
            result,
            [
                {"foo": 5, "bar": 8, "a very long key": 11},
                [{"a very long key": 4, "foo": []}],
            ],
        )
        d0 = result[0]
        d1 = result[1][0]
        d0_keys = list(d0.keys())
        d1_keys = list(d1.keys())
        self.assertEqual(d0_keys, ["foo", "bar", "a very long key"])
        self.assertEqual(d1_keys, ["a very long key", "foo"])
        self.assertIs(d0_keys[0], d1_keys[1])
        self.assertIs(d0_keys[2], d1_keys[0])

    def test_with_cls_calls_cls_and_calls_decode(self):
        class C:
            def decode(self, s):
                return f"decode called on {s}"

        func_args = None
        func_kwargs = None

        def func(*args, **kwargs):
            nonlocal func_args
            nonlocal func_kwargs
            func_args = args
            func_kwargs = kwargs
            return C()

        result = loads("<test input>", cls=func, foo=42, bar="hello")
        self.assertEqual(func_args, ())
        self.assertEqual(
            func_kwargs,
            {
                "foo": 42,
                "bar": "hello",
            },
        )
        self.assertEqual(result, "decode called on <test input>")

        func_args = None
        func_kwargs = None
        result = loads(
            s="<test input>",
            encoding="ignored",
            cls=func,
            object_hook=1,
            parse_float=2,
            parse_int=3,
            parse_constant=4,
            object_pairs_hook=5,
            strict=6,
            foobar=7,
        )
        self.assertEqual(func_args, ())
        self.assertEqual(
            func_kwargs,
            {
                "object_hook": 1,
                "parse_float": 2,
                "parse_int": 3,
                "parse_constant": 4,
                "object_pairs_hook": 5,
                "strict": 6,
                "foobar": 7,
            },
        )
        self.assertEqual(result, "decode called on <test input>")

    def test_unexpected_char_raises_json_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("a")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("$")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("\x00")
        with self.assertRaisesRegex(
            JSONDecodeError, r"Expecting value: line 1 column 1 \(char 0\)"
        ):
            loads("\U0001f480")

    def test_utf8_bom_raises_decode_error(self):
        with self.assertRaisesRegex(
            JSONDecodeError, r"Unexpected UTF-8 BOM \(decode using utf-8-sig\)"
        ):
            loads("\ufeff0")

    def test_json_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, "the JSON object must be str, bytes or bytearray, not float"
        ):
            loads(42.42)


if __name__ == "__main__":
    unittest.main()
