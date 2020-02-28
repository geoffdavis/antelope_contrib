"""Test the config module."""
import unittest

from pydbwfserver.config import DbwfserverConfig

TEST_DB = "/opt/antelope/data/db/demo/demo"


class TestDbwfserverConfig(unittest.TestCase):
    """Test the dbwfserver configuration object."""

    def setUp(self) -> None:
        """Set up further tests."""
        self.test_db = TEST_DB
        self.subject = DbwfserverConfig([self.test_db])

    def testBadAttr(self):
        """Test an invalid attribute."""
        with self.assertRaises(AttributeError):
            self.subject.__getattr__("tacos")

    def testUnintializedAttr(self):
        """Test a valid attribute before it's available."""

        self.assertIsNone(self.subject.filters)

    def testAttr(self):
        """Test a valid attr after it's been loaded from the parameter file."""
        self.subject.configure()
        self.assertIsNotNone(self.subject.filters)


if __name__ == "__main__":
    unittest.main()
