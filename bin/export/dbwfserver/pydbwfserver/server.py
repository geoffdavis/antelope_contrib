"""main server resource for pydbwfserver.

This resource sets up a service collection of multiple running instances of the dbwfserver.

Notably, it also includes support for placing the daemon behind a reverse proxy server,
with a resource titled "vhost" at the root level of each application instance.

For more information, see:
https://twistedmatrix.com/documents/15.0.0/web/howto/using-twistedweb.html#using-vhostmonster
"""

from pydbwfserver.main import config
from pydbwfserver.resource import FaviconResource, QueryParserResource
from twisted.application import internet, service
from twisted.python.log import ILogObserver, PythonLoggingObserver
from twisted.web import server, static
from twisted.web.vhost import VHostMonsterResource

for port, db in config.run_server.items():

    root = QueryParserResource(config, db)

    root.putChild(b"vhost", VHostMonsterResource())

    root.putChild(b"static", static.File(config.static_dir))

    root.putChild(b"favicon.ico", FaviconResource(config))

    site = server.Site(root)

    site.displayTracebacks = config.display_tracebacks

    application = service.Application("pydbwfserver")

    observer = PythonLoggingObserver("pydbwfserver.twisted.port" + str(port))

    application.setComponent(ILogObserver, observer.emit)

    sc = service.IServiceCollection(application)

    sc.addService(internet.TCPServer(int(port), site))
