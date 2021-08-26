#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)

import faulthandler
import tempfile
import unittest
from unittest.mock import Mock


class FaulthandlerTest(unittest.TestCase):
    def test_dump_traceback_with_non_int_fd_calls_fileno_and_flush(self):
        with tempfile.TemporaryFile() as file:

            class C:
                fileno = Mock(return_value=file.fileno())
                flush = Mock()

            C.fileno.assert_not_called()
            C.flush.assert_not_called()
            faulthandler.dump_traceback(C(), False)
            C.fileno.assert_called_once()
            C.flush.assert_called_once()

    def test_dump_traceback_with_non_int_all_threads_raises_type_error(self):
        self.assertRaises(TypeError, faulthandler.dump_traceback, 2, None)

    def test_is_enabled(self):
        self.assertIs(faulthandler.is_enabled(), False)
        faulthandler.enable()
        self.assertIs(faulthandler.is_enabled(), True)
        faulthandler.disable()
        self.assertIs(faulthandler.is_enabled(), False)


if __name__ == "__main__":
    unittest.main()
