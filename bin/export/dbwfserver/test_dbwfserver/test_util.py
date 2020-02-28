"""Tests for pydbwfserver.util."""

import unittest

from pydbwfserver.util import str2bool


class TestUtil(unittest.TestCase):
    """Test cases for the util submodule."""

    def test_str2bool_true(self):
        """Test the str2bool function with true values."""
        true_vals = ("true", "t", "1", "yes", "y")

        for tv in true_vals:
            with self.subTest(tv=tv):
                self.assertEqual(
                    str2bool(tv), True, msg="Value %s should be true." % tv
                )
                self.assertEqual(
                    str2bool(tv.upper()),
                    True,
                    msg="Value %s should be true." % tv.upper(),
                )

    def test_str2bool_false(self):
        """Test str2bool with false values."""
        false_vals = ("false", "f", "0", "no", "n")
        for fv in false_vals:
            with self.subTest(fv=fv):
                self.assertEqual(
                    str2bool(fv), False, msg="Value %s should be false." % fv
                )
                self.assertEqual(
                    str2bool(fv.upper()),
                    False,
                    msg="Value %s should be false." % fv.upper(),
                )

    def test_str2bool_unknown(self):
        """Test str2bool with unknown values."""
        unknown_vals = (
            1,
            0,
            69,
            9999,
            "tacos",
            "",
            "This much is true.",
            ["blah", "blah"],
        )
        for uv in unknown_vals:
            with self.subTest(uv=uv):
                with self.assertRaises(ValueError):
                    str2bool(uv)


if __name__ == "__main__":
    unittest.main()
