"""Test cases for the resource module."""
import os
import unittest
from unittest.mock import Mock

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
        self.subject._init_in_thread()
        self.request = Mock()
        self.request.prepath = [b"", b"foo", b"bar", b"", b"baz", b"qux"]
        self.request.args = {}

    def testInit(self):
        """Verify that the query parser resource instantiates as a resource."""
        self.assertIsInstance(self.subject, Resource)
        self.assertIsNotNone(self.subject.dbcentral)

    def testChild(self):
        """Verify that this resource breaks the Twisted resource parsing model.

        NOTE: This resource does too many things and should be broken up.
        """
        self.assertEqual(self.subject, self.subject.getChild("foo", self.request))

    def test_loading(self):
        """Test the loading page."""
        self.assertIsNotNone(self.subject._render_loading(self.request))

    def test_render_uri(self):
        """Test render_uri callback."""

        self.assertIsNone(self.subject.render_uri(self.request))


if __name__ == "__main__":
    unittest.main()
