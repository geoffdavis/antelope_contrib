"""Test cases for the resource module."""
import os
import unittest

from pydbwfserver.config import DbwfserverConfig
from pydbwfserver.resource import FaviconResource, QueryParserResource
from twisted.web.resource import Resource

TEST_DB = "/opt/antelope/data/db/demo/demo"


class TestFaviconresource(unittest.TestCase):
    """Test the favicon resource."""

    def setUp(self):
        """Create common test items."""
        self.config = DbwfserverConfig()
        self.config.static_dir = os.path.abspath("Contents/static")
        self.subject = FaviconResource(self.config)

    def test_faviconresource(self):
        """Ensure that faviconresource is a subclass of Resource."""
        self.assertIsInstance(self.subject, Resource)


class TestQueryParserResource(unittest.TestCase):
    """Tests for the Query Parser resource."""

    def setUp(self):
        """Set up common items for tests."""
        self.test_db = TEST_DB
        self.config = DbwfserverConfig([self.test_db])
        self.config.configure()
        self.subject = QueryParserResource(self.config, self.test_db)

    def testInit(self):
        """Verify that the query parser resource instantiates as a resource."""
        self.assertIsInstance(self.subject, Resource)


if __name__ == "__main__":
    unittest.main()
