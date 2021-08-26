#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import unittest


class GrpModuleTest(unittest.TestCase):
    def test_it_imports(self):
        import grp

        self.assertEqual(grp.__name__, "grp")


if __name__ == "__main__":
    unittest.main()
