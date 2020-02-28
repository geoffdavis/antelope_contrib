"""Tests for the dbcentral module."""
import unittest

from pydbwfserver.dbcentral import Dbcentral


class DbcentralTests(unittest.TestCase):
    """Test the Dbcentral class."""

    def setUp(self) -> None:
        """Configure a test dbcentral instance."""
        self.dbcentral = Dbcentral(
            "/opt/antelope/data/db/demo/demo",
            debug=True,
            required_tables=["wfdisc", "stachan"],
        )

    def test_repr(self):
        """Test the __repr__ method on the demo db."""
        self.assertEqual(
            self.dbcentral.__repr__(),
            "Dbcentral(dbname=/opt/antelope/data/db/demo/demo, nickname=None, debug=True, required_tables=wfdisc,stachan, type=masquerade, dbs=/opt/antelope/data/db/demo/demo)",
        )

    def test_str(self) -> None:
        """Test the __str__ medhot on the demo db."""
        self.assertEqual(
            self.dbcentral.__str__(),
            "An instance of the Dbcentral class at '/opt/antelope/data/db/demo/demo' containing 1 databases: /opt/antelope/data/db/demo/demo",
        )

    def test_time(self):
        """Test retrieving data at a given time."""
        time = 1262404000.00000
        self.assertEqual(self.dbcentral(time=time), "/opt/antelope/data/db/demo/demo")

    def test_purge(self):
        """Test that the purge function fails with an unknown key."""
        with self.assertRaises(KeyError):
            self.dbcentral.purge("test")


if __name__ == "__main__":
    unittest.main()
