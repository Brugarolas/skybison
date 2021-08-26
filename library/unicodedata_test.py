#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import unicodedata
import unittest


class UnicodedataTests(unittest.TestCase):
    def test_UCD_dunder_new_raises_type_error(self):
        with self.assertRaises(TypeError):
            unicodedata.UCD()

    def test_UCD_bidirectional_uses_old_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.bidirectional(" "), "WS")
        self.assertEqual(unicodedata.ucd_3_2_0.bidirectional("+"), "ET")
        self.assertEqual(unicodedata.ucd_3_2_0.bidirectional("A"), "L")
        self.assertEqual(unicodedata.ucd_3_2_0.bidirectional("\uFFFE"), "")
        self.assertEqual(unicodedata.ucd_3_2_0.bidirectional("\U00020000"), "L")

    def test_UCD_category_uses_old_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.category("A"), "Lu")
        self.assertEqual(unicodedata.ucd_3_2_0.category("a"), "Ll")
        self.assertEqual(unicodedata.ucd_3_2_0.category("\u00A7"), "So")
        self.assertEqual(unicodedata.ucd_3_2_0.category("\uFFFE"), "Cn")
        self.assertEqual(unicodedata.ucd_3_2_0.category("\U0001012A"), "Cn")
        self.assertEqual(unicodedata.ucd_3_2_0.category("\U00020000"), "Lo")

    def test_UCD_decimal_uses_old_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.decimal("5"), 5)
        self.assertEqual(unicodedata.ucd_3_2_0.decimal("\u0E50"), 0)

        # changed since 3.2.0
        self.assertEqual(unicodedata.ucd_3_2_0.decimal("\u00B2"), 2)

        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.decimal)
        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.decimal, "xx")
        self.assertRaises(ValueError, unicodedata.ucd_3_2_0.decimal, "a")
        self.assertRaises(ValueError, unicodedata.ucd_3_2_0.decimal, "\u00BD")

    def test_UCD_decomposition_uses_old_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.decomposition("\uFFFE"), "")
        self.assertEqual(
            unicodedata.ucd_3_2_0.decomposition("\u00BC"), "<fraction> 0031 2044 0034"
        )

        # unassigned in 3.2.0
        self.assertEqual(unicodedata.ucd_3_2_0.decomposition("\u0221"), "")

        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.decomposition)
        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.decomposition, "xx")

    def test_UCD_digit_uses_old_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.digit("2"), 2)
        self.assertEqual(unicodedata.ucd_3_2_0.digit("\u0E50"), 0)
        self.assertEqual(unicodedata.ucd_3_2_0.digit("\u00B2"), 2)

        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.digit)
        self.assertRaises(TypeError, unicodedata.ucd_3_2_0.digit, "xx")
        self.assertRaises(ValueError, unicodedata.ucd_3_2_0.digit, "a")
        self.assertRaises(ValueError, unicodedata.ucd_3_2_0.digit, "\u00BD")

    def test_UCD_normalize_with_non_UCD_raises_type_error(self):
        with self.assertRaises(TypeError):
            unicodedata.UCD.normalize(1, "NFC", "foo")

    def test_UCD_normalize_with_non_str_form_raises_type_error(self):
        with self.assertRaises(TypeError):
            unicodedata.ucd_3_2_0.normalize(2, "foo")

    def test_UCD_normalize_with_non_str_src_raises_type_error(self):
        with self.assertRaises(TypeError):
            unicodedata.ucd_3_2_0.normalize("foo", 2)

    def test_UCD_normalize_with_empty_str_ignores_form(self):
        self.assertEqual(unicodedata.ucd_3_2_0.normalize("invalid", ""), "")

    def test_UCD_normalize_with_invalid_form_raises_value_error(self):
        with self.assertRaises(ValueError):
            unicodedata.ucd_3_2_0.normalize("invalid", "foo")

    def test_UCD_normalize_uses_old_version(self):
        self.assertEqual(
            unicodedata.ucd_3_2_0.normalize(
                "NFD", u"\U0002F868 \U0002F874 \U0002F91F \U0002F95F \U0002F9bF"
            ),
            u"\U0002136A \u5F33 \u43AB \u7AAE \u4D57",
        )

    def test_bidirectional_uses_current_version(self):
        self.assertEqual(unicodedata.bidirectional(" "), "WS")
        self.assertEqual(unicodedata.bidirectional("+"), "ES")
        self.assertEqual(unicodedata.bidirectional("A"), "L")
        self.assertEqual(unicodedata.bidirectional("\uFFFE"), "")
        self.assertEqual(unicodedata.bidirectional("\U00020000"), "L")

    def test_category_uses_current_version(self):
        self.assertEqual(unicodedata.category("A"), "Lu")
        self.assertEqual(unicodedata.category("a"), "Ll")
        self.assertEqual(unicodedata.category("\u00A7"), "Po")
        self.assertEqual(unicodedata.category("\uFFFE"), "Cn")
        self.assertEqual(unicodedata.category("\U0001012A"), "No")
        self.assertEqual(unicodedata.category("\U00020000"), "Lo")

    def test_decomposition_uses_current_version(self):
        self.assertEqual(unicodedata.decomposition("\u0221"), "")
        self.assertEqual(unicodedata.decomposition("\uFFFE"), "")
        self.assertEqual(
            unicodedata.decomposition("\u00BC"), "<fraction> 0031 2044 0034"
        )

        self.assertRaises(TypeError, unicodedata.decomposition)
        self.assertRaises(TypeError, unicodedata.decomposition, "xx")

        # unassigned in 3.2.0

    def test_decimal_uses_current_version(self):
        self.assertEqual(unicodedata.decimal("2"), 2)
        self.assertEqual(unicodedata.decimal("\u0E50"), 0)

        self.assertRaises(TypeError, unicodedata.decimal)
        self.assertRaises(TypeError, unicodedata.decimal, "xx")
        self.assertRaises(ValueError, unicodedata.decimal, "a")
        self.assertRaises(ValueError, unicodedata.decimal, "\u00B2")
        self.assertRaises(ValueError, unicodedata.decimal, "\u00BD")

    def test_digit_uses_current_version(self):
        self.assertEqual(unicodedata.digit("2"), 2)
        self.assertEqual(unicodedata.digit("\u0E50"), 0)
        self.assertEqual(unicodedata.digit("\u00B2"), 2)

        self.assertRaises(TypeError, unicodedata.digit)
        self.assertRaises(TypeError, unicodedata.digit, "xx")
        self.assertRaises(ValueError, unicodedata.digit, "a")
        self.assertRaises(ValueError, unicodedata.digit, "\u00BD")

    def test_lookup_uses_current_version(self):
        self.assertEqual(unicodedata.lookup("latin CAPITAL Letter a"), "A")
        self.assertEqual(unicodedata.lookup("digit zero"), "0")
        self.assertEqual(unicodedata.lookup("TAI VIET LETTER LOW VO"), "\uAAAA")

        # Hangul
        self.assertEqual(unicodedata.lookup("Hangul jongseong RIEUL-PIEUP"), "\u11B2")
        self.assertEqual(unicodedata.lookup("HANGUL SYLLABLE JJWAENH"), "\uCAFA")

        # CJK
        self.assertEqual(unicodedata.lookup("CJK UNIFIED IDEOGRAPH-35AB"), "\u35AB")
        self.assertEqual(
            unicodedata.lookup("CJK UNIFIED IDEOGRAPH-20000"), "\U00020000"
        )

        # Named Sequences
        self.assertEqual(unicodedata.lookup("TAI VIET LETTER LOW VO"), "\uAAAA")

        self.assertRaises(KeyError, unicodedata.lookup, "letter b")
        self.assertRaises(KeyError, unicodedata.lookup, "hangul syllable son")
        self.assertRaises(KeyError, unicodedata.lookup, "cjk unified ideograph-20000")

    def test_numeric_uses_current_version(self):
        self.assertEqual(unicodedata.numeric("7"), 7.0)
        self.assertEqual(unicodedata.numeric("\u00BE"), 0.75)
        self.assertEqual(unicodedata.numeric("\u09F7"), 0.25)
        self.assertEqual(unicodedata.numeric("\U000109D3"), 200.0)
        self.assertEqual(unicodedata.numeric("\U00020AFD"), 3.0)

        self.assertEqual(unicodedata.numeric("A", "default"), "default")

        self.assertRaises(TypeError, unicodedata.numeric, 2)
        self.assertRaises(TypeError, unicodedata.numeric, "")
        self.assertRaises(TypeError, unicodedata.numeric, "foo")
        self.assertRaises(ValueError, unicodedata.numeric, "A")
        self.assertRaises(ValueError, unicodedata.numeric, "\u4EAC")

    def test_old_unidata_version(self):
        self.assertEqual(unicodedata.ucd_3_2_0.unidata_version, "3.2.0")

    def test_ucd_3_2_0_isinstance_of_UCD(self):
        self.assertIsInstance(unicodedata.ucd_3_2_0, unicodedata.UCD)


if __name__ == "__main__":
    unittest.main()
