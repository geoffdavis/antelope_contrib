"""
Twisted.web resources for use by the pydbwfserver application.

FaviconResource is a thin wrapper around a static File resource.

QueryParserResource is the main resource that runs at the root of the
pydbwfserver application
"""

import json
import logging
import os
import socket
from string import Template
import sys
from textwrap import dedent

import six
import twisted.internet.defer
import twisted.internet.reactor
from twisted.internet.threads import deferToThread
import twisted.web.resource
import twisted.web.server
import twisted.web.static

from antelope import stock

from .dbcentral import Dbcentral
from .util import Events, Stations, load_template


class FaviconResource(twisted.web.static.File):
    """Serve up a favicon from the static content directory."""

    def __init__(self, config):
        """Initialize the FaviconResource."""
        twisted.web.static.File.__init__(
            self,
            os.path.join(config.static_dir, "images/favicon.ico"),
            defaultType="image/vnd.microsoft.icon",
        )


class QueryParserResource(twisted.web.resource.Resource):
    """
    Serve http queries.

    Functions as the root resource of the pydbwfserver Site.
    """

    isLeaf = False

    allowedMethods = "GET"

    def __init__(self, config, dbname):
        """Initialize the QueryParserResource class."""

        self.logger = logging.getLogger(__name__)

        self.logger.info("########################")
        self.logger.info("        Loading!        ")
        self.logger.info("########################")

        self.init_finished = False
        self.init_failure = False

        self.config = config
        self.dbname = dbname
        self.loading_stations = True
        self.loading_events = True

        self.root_template = load_template(self.config.template)
        self.plot_template = load_template(self.config.plot_template)

        #
        # Initialize Classes
        #
        self.logger.debug(
            "QueryParser(): Init DB: Load class twisted.web.resource.Resource.__init__(self)"
        )
        twisted.web.resource.Resource.__init__(self)

        #
        # Open db using Dbcentral CLASS
        #
        self.logger.debug(
            "QueryParser(): Init DB: Create Dbcentral object with database(%s)."
            % self.dbname
        )
        self.dbcentral = Dbcentral(
            self.dbname, self.config.nickname, self.config.debug, ["wfdisc", "sitechan"]
        )
        if self.config.debug:
            self.dbcentral.info()

        if not self.dbcentral.list():
            self.logger.critical("Init DB: No databases to use! (%s)" % self.dbname)
            sys.exit(twisted.internet.reactor.stop())

        self.tvals = {
            "filters": '<option value="None">None</option>',
            "display_arrivals": "",
            "display_points": "",
            "proxy_url": self.config.proxy_url,
            "dbname": self.dbname,
            "application_title": self.config.application_title,
        }

        for filter_instance in self.config.filters:
            self.tvals["filters"] += (
                "<option value=" + filter_instance.replace(" ", "_") + ">"
            )
            self.tvals["filters"] += filter_instance
            self.tvals["filters"] += "</option>"

        if self.config.event == "true" and self.config.display_arrivals:
            self.tvals["display_arrivals"] = 'checked="checked"'

        if self.config.display_points:
            self.tvals["display_points"] = 'checked="checked"'

        if not self.dbcentral.list():
            self.logger.critical(
                "No valid databases to work with! -v or -V for more info"
            )
            return

        d = deferToThread(self._init_in_thread)
        d.addCallback(self._init_finished)
        d.addErrback(self._init_failed)

    def _init_finished(self, d):
        """Mark initialization as finished."""
        self.init_finished = True

    def _init_failed(self, failure):
        """Handle initialization failures."""
        self.init_failure = True
        self.logger.critical("An error occurred during initialization: " + str(failure))
        sys.exit(twisted.internet.reactor.stop())

    def _init_in_thread(self):
        """Run Initialization within a new thread."""

        self.logger.info("Loading Stations()")
        self.stations = Stations(self.config, self.dbcentral)
        self.loading_stations = False
        self.logger.info("Done loading Stations()")

        if self.config.event == "true":
            self.logger.info("Loading Events()")
            self.events = Events(self.dbcentral, config=self.config)
            self.loading_events = False
            self.logger.info("Done loading Events()")
        else:
            self.loading_events = False

        self.logger.info("READY!")

    def getChild(self, name, request):
        """Get the child process."""

        self.logger.debug("getChild(): name:%s request:%s" % (name, request))
        return self

    def _render_loading(self, request):
        """Output an error page while loading Stations and Events."""

        response_code = 503

        html_template = Template(
            dedent(
                """\
            <html>
                <head><title>ERROR: $responseCode - $appname</title></head>
                <body>
                    <h1>ERROR: $responseCode - $appname</h1>
                    <p>The DBWFSERVER $appname is still starting up. Try again later.</p>
                    <p>Waiting for Stations: $loading_stations</p>
                    <p>Waiting for Events: $loading_events</p>
                </body>
            </html>
            """
            )
        )

        html = html_template.substitute(
            appname=self.config.application_title,
            responseCode=response_code,
            loading_stations=self.loading_stations,
            loading_events=self.loading_events,
        )

        request.setHeader("content-type", "text/html")
        request.setResponseCode(response_code)
        self.logger.debug("_render_loading returning: \n" + html)
        return html.encode()

    def render_GET(self, request):
        """Render a GET request."""

        self.logger.debug("QueryParser(): render_GET(%s)", request)
        self.logger.debug("QueryParser(): render_GET() request.uri:%s", request.uri)
        self.logger.debug("QueryParser(): render_GET() request.args:%s", request.args)
        self.logger.debug(
            "QueryParser(): render_GET() request.prepath:%s", request.prepath
        )
        self.logger.debug(
            "QueryParser(): render_GET() request.postpath:%s", request.postpath
        )
        self.logger.debug("QueryParser(): render_GET() request.path:%s", request.path)

        if self.loading_stations or self.loading_events:
            return self._render_loading(request)

        self.logger.debug("QueryParser():\tQUERY: %s ", request)
        self.logger.debug("QueryParser():\tHost=> [%s]", request.host)
        self.logger.debug(
            "QueryParser():\tsocket.gethostname() => [%s]", socket.gethostname()
        )
        self.logger.debug("")

        d = twisted.internet.defer.Deferred()
        d.addCallback(self.render_uri)
        twisted.internet.reactor.callInThread(d.callback, request)

        self.logger.debug("QueryParser(): render_GET() - return server.NOT_DONE_YET")

        return twisted.web.server.NOT_DONE_YET

    def render_uri(self, request):
        """Handle a generic request."""

        #
        # Clean and prep vars
        #
        response_meta = {}

        response_meta.update(
            {
                "error": "false",
                "setupEvents": self.config.event,
                "setupUI": "false",
                "realtime": self.config.realtime,
                # "proxy_url":  self.config.proxy_url,
                "style": self.config.style,
                "meta_query": "false",
            }
        )

        # if self.config.proxy_url: response_meta['proxy'] = "'" + self.config.proxy_url + "'"

        #
        # remove all empty  elements
        # This (localhost:8008/stations/) is the same as # (localhost:8008/stations)
        #
        path = request.prepath
        # print("Hello, world! I am located at %r." % (request.prepath))
        while True:
            try:
                path.remove(b"")
            except Exception:
                break

        # Parse all elements on the station_patterns
        query = self._parse_request(path)

        if b"precision" in request.args:
            query.update({"precision": int(request.args[b"precision"][0].decode())})
        else:
            query.update({"precision": 1})

        if b"period" in request.args:
            query.update({"period": int(request.args[b"period"][0].decode())})
        else:
            query.update({"period": 0})

        if b"median" in request.args:
            test = request.args[b"median"][0].decode()
            if test.lower() in ("yes", "true", "t", "1"):
                query.update({"median": 1})
            else:
                query.update({"median": 0})
        else:
            query.update({"median": 0})

        if b"realtime" in request.args:
            test = request.args[b"realtime"][0].decode()
            if test.lower() in ("yes", "true", "t", "1"):
                query.update({"realtime": 1})
            else:
                query.update({"realtime": 0})
        else:
            query.update({"realtime": 0})

        if b"filter" in request.args:
            filter = request.args[b"filter"][0].decode()
            query.update({"filter": filter.replace("_", " ")})
        else:
            query.update({"filter": "None"})

        if b"calibrate" in request.args:
            test = request.args[b"calibrate"][0].decode()
            if test.lower() in ("yes", "true", "t", "1"):
                query.update({"calibrate": 1})
            else:
                query.update({"calibrate": 0})
        else:
            request.args.update({"calibrate": [self.config.apply_calib]})
            query.update({"calibrate": self.config.apply_calib})

        self.logger.debug(
            "QueryParser(): render_uri() request.prepath => path(%s)[%s]"
            % (len(path), path)
        )
        self.logger.debug("QueryParser(): render_uri() query => [%s]" % query)

        if query["data"] is True:

            self.logger.debug('QueryParser(): render_uri() "data" query')

            if len(path) == 0:
                # ERROR: we need one option
                self.logger.error(
                    'QueryParser(): render_uri() ERROR: Empty "data" query!'
                )
                return self.uri_results(request, "Invalid data query.")

            elif path[0] == b"events":

                """
                    Return events dictionary as JSON objects. For client ajax calls.
                    Called with or without argument.
                    """

                self.logger.debug("QueryParser(): render_uri() query => data => events")
                if self.config.event != "true":
                    return self.uri_results(request, {})

                elif len(path) == 2:
                    return self.uri_results(request, self.events(path[1].decode()))

                elif len(path) == 3:
                    return self.uri_results(
                        request, self.events.phases(path[1].decode(), path[2].decode())
                    )

                else:
                    return self.uri_results(request, self.events.table())

            elif path[0] == b"dates":

                """
                    Return station_patterns of yearday values for time in db
                    for all wfs in the cluster of dbs.
                    """

                self.logger.debug("QueryParser(): render_uri() query => data => dates")

                return self.uri_results(request, self.stations.dates())

            elif path[0] == b"stadates":

                """
                    Return station_patterns of yearday values for time in db
                    for all stations in the cluster of dbs.
                    """

                self.logger.debug("QueryParser(): render_uri() query => data => dates")

                if len(path) == 2:
                    return self.uri_results(
                        request, self.stations.stadates(path[1].decode())
                    )

                if len(path) == 3:
                    return self.uri_results(
                        request,
                        self.stations.stadates(path[1].decode(), path[2].decode()),
                    )

                return self.uri_results(request, self.stations.stadates())

            elif path[0] == b"stations":

                """
                    Return station station_patterns as JSON objects. For client ajax calls.
                    Called with argument return dictionary
                    """

                self.logger.debug(
                    "QueryParser(): render_uri() query => data => stations"
                )

                if len(path) == 2:
                    return self.uri_results(request, self.stations(path[1].decode()))

                return self.uri_results(request, self.stations.list())

            elif path[0] == b"channels":

                """
                    Return channels station_patterns as JSON objects. For client ajax calls.
                    """

                self.logger.debug(
                    "QueryParser(): render_uri() query => data => channels"
                )

                if len(path) == 2:
                    stas = self.stations.convert_sta(path[1].decode().split("|"))
                    return self.uri_results(request, self.stations.channels(stas))

                return self.uri_results(request, self.stations.channels())

            elif path[0] == b"now":

                """
                    Return JSON object for epoch(now).
                    """

                self.logger.debug("QueryParser(): render_uri() query => data => now")

                return self.uri_results(request, [stock.now()])

            elif path[0] == b"filters":

                """
                    Return station_patterns of filters as JSON objects. For client ajax calls.
                    """

                self.logger.debug(
                    "QueryParser(): render_uri() query => data => filters %s"
                    % self.config.filters
                )

                return self.uri_results(request, self.config.filters)

            elif path[0] == b"wf":

                """
                    Return JSON object of data. For client ajax calls.
                    """

                self.logger.debug("QueryParser(): render_uri(): get_data(%s))" % query)

                return self.uri_results(request, self.get_data(query))

            elif path[0] == b"coverage":

                """
                    Return coverage tuples as JSON objects. For client ajax calls.
                    """
                self.logger.debug("QueryParser(): render_uri(): Get coverage")

                query.update({"coverage": 1})

                return self.uri_results(request, self.get_data(query))

            else:
                # ERROR: Unknown query type.
                return self.uri_results(request, "Unknown query type:(%s)" % path)

        response_meta.update(self.tvals)

        if (not path) or len(path) == 0:
            return self.uri_results(
                request, self.root_template.safe_substitute(response_meta)
            )

        response_meta["meta_query"] = {}
        response_meta["meta_query"]["sta"] = query["sta"]
        response_meta["meta_query"]["chan"] = query["chan"]
        response_meta["meta_query"]["time_start"] = query["start"]
        response_meta["meta_query"]["time_end"] = query["end"]
        response_meta["meta_query"]["page"] = query["page"]

        self.logger.debug("Request Args: %s", request.args)

        if request.args is not None:
            decoded_args = {
                k.decode(): [vv.decode() for vv in v] for k, v in request.args.items()
            }
            response_meta["setupUI"] = json.dumps(decoded_args)

        response_meta["meta_query"] = json.dumps(str(response_meta["meta_query"]))

        if path[0] == b"wf":
            return self.uri_results(
                request, self.root_template.safe_substitute(response_meta)
            )

        elif path[0] == b"plot":
            return self.uri_results(
                request, self.plot_template.safe_substitute(response_meta)
            )

        return self.uri_results(request, "Invalid query.")

    def _parse_request(self, args):
        """
        Parse a request, acting as our own resource mapper.

        NOTE: this seems to be fighting the Twisted application model. Consider
        fixing it so that Twisted does the URL dispatch instead of this
        catch-all function.

        Strict format for uri:
            localhost/wf/sta/chan/time/time/page

            Data-only calls:
            localhost/data/wf
            localhost/data/times
            localhost/data/events
            localhost/data/filters
            localhost/data/stations
            localhost/data/coverage
            localhost/data/channels
            localhost/data/wf/sta/chan/time/time/page
        """

        self.logger.debug("QueryParser(): _parse_request(): URI: %s" % str(args))

        uri = {}
        time_window = float(self.config.default_time_window)

        uri.update(
            {
                "sta": [],
                "chan": [],
                "end": 0,
                "data": False,
                "start": 0,
                "page": 1,
                "coverage": 0,
                "time_window": False,
            }
        )

        if b"data" in args:
            self.logger.info("QueryParser() _parse_request(): data query!")
            uri["data"] = True
            args.remove(b"data")

        # localhost/sta
        if len(args) > 1:
            uri["sta"] = args[1].decode()

        # localhost/sta/chan
        if len(args) > 2:
            uri["chan"] = args[2].decode()

        # localhost/sta/chan/time
        if len(args) > 3:
            uri["start"] = args[3].decode()

        # localhost/sta/chan/time/time
        if len(args) > 4:
            uri["end"] = args[4].decode()

        # localhost/sta/chan/time/time/page
        if len(args) > 5:
            uri["page"] = args[5].decode()

        #
        # Fix start, and sanitize bad values while we're at it.
        #
        if "start" in uri and uri["start"] is not None:
            if uri["start"] == "hour":
                uri["start"] = 0
                time_window = 3600
            elif uri["start"] == "day":
                uri["start"] = 0
                time_window = 86400
            elif uri["start"] == "week":
                uri["start"] = 0
                time_window = 604800
            elif uri["start"] == "month":
                uri["start"] = 0
                time_window = 2629743
            else:
                try:
                    uri["start"] = float(uri["start"])
                except ValueError:
                    self.logger.warning(
                        "Invalid value '%s' for start time received.", uri["start"]
                    )
                    uri["start"] = 0

        #
        # Fix end, and sanitize bad values while we're at it.
        #
        if "end" in uri and uri["end"] is not None:
            if uri["end"] == "hour":
                uri["end"] = 0
                time_window = 3600
            elif uri["end"] == "day":
                uri["end"] = 0
                time_window = 86400
            elif uri["end"] == "week":
                uri["end"] = 0
                time_window = 604800
            elif uri["end"] == "month":
                uri["end"] = 0
                time_window = 2629743
            else:
                try:
                    uri["end"] = float(uri["end"])
                except ValueError:
                    self.logger.warning(
                        "Invalid value '%s' for end time received.", uri["end"]
                    )
                    uri["end"] = None

        #
        # Build missing times if needed
        #
        if uri["sta"] and uri["chan"]:
            if not uri["start"]:
                uri["end"] = self.stations.max_time()
                uri["start"] = uri["end"] - time_window

            if not uri["end"]:  # should handle both None and 0
                uri["end"] = uri["start"] + time_window

        self.logger.debug(
            "QueryParser(): _parse_request(): [sta:%s chan:%s start:%s end:%s]"
            % (uri["sta"], uri["chan"], uri["start"], uri["end"])
        )

        return uri

    def uri_results(self, uri=None, results=None):
        """Return the results of a query to a particular URI."""
        self.logger.info("QueryParser(): uri_results(%s,%s)" % (uri, type(results)))

        if uri:

            if results is not None:
                if isinstance(results, six.integer_types) or isinstance(
                    results, six.string_types
                ):
                    uri.setHeader("content-type", "text/html")
                    uri.write(results.encode())
                else:
                    uri.setHeader("content-type", "application/json")
                    uri.write(json.dumps(results).encode())
            else:
                uri.setHeader("content-type", "text/html")
                uri.setResponseCode(500)
                uri.write(b"Problem with server!")

            try:
                uri.finish()
            except Exception:
                pass

        self.logger.info("QueryParser(): uri_results() DONE!")

    def get_data(self, query):
        """Return points or bins of data for query."""

        self.logger.debug("QueryParser(): get_data(): Build COMMAND")

        self.logger.debug(
            "QueryParser(): get_data(): Get data for uri:%s.%s"
            % (query["sta"], query["chan"])
        )

        if not query["sta"]:
            response_data = "Not valid station value"
            self.logger.error(response_data)
            return {"ERROR": response_data}

        if not query["chan"]:
            response_data = "Not valid channel value "
            self.logger.error(response_data)
            return {"ERROR": response_data}
        try:
            start = float(query["start"])
        except ValueError:
            start = None

        try:
            end = float(query["end"])
        except ValueError:
            end = None

        if not start:
            temp_dic = self.stations(query["sta"][0])
            if temp_dic:
                start = (
                    temp_dic[query["chan"][0]]["end"] - self.config.default_time_window
                )

        # if not start: start = stock.now()

        if not end:
            end = start + self.config.default_time_window

        tempdb = self.dbcentral(start)
        if not tempdb:
            response_data = "Not valid database for this time [%s]" % start
            self.logger.error(response_data)
            return {"ERROR": response_data}

        regex = "-s 'sta=~/%s/ && chan=~/%s/' " % (query["sta"], query["chan"])

        if query["filter"] != "None":
            filter_string = "-f '%s'" % query["filter"]
        else:
            filter_string = ""

        if query["coverage"]:
            coverage = "-b "
        else:
            coverage = ""

        if query["realtime"]:
            realtime = "-r "
        else:
            realtime = ""

        if query["precision"]:
            precision = "-q %s" % query["precision"]
        else:
            precision = ""

        if query["median"]:
            median = "-a "
        else:
            median = ""

        if query["calibrate"]:
            calibrate = "-c "
        else:
            calibrate = ""

        if query["period"]:
            period = "-t %s" % query["period"]
        else:
            period = ""

        if query["page"]:
            page = "-p %s" % query["page"]
        else:
            page = ""

        run = (
            "dbwfserver_extract %s %s %s %s %s %s %s %s %s -n %s -m %s %s %s %s 2>&1"
            % (
                regex,
                coverage,
                filter_string,
                page,
                calibrate,
                precision,
                realtime,
                median,
                period,
                self.config.max_traces,
                self.config.max_points,
                tempdb,
                start,
                end,
            )
        )

        self.logger.info("QueryParser(): get_data(): Extraction command: [%s]" % run)

        # Method 1
        # master, slave = pty.openpty()
        # pipe = Popen(run, stdin=PIPE, stdout=slave, stderr=slave, close_fds=True, shell=True)
        # stdout = os.fdopen(master)
        # return stdout.readline()

        # Method 2
        return os.popen(run).read().replace("\n", "")
