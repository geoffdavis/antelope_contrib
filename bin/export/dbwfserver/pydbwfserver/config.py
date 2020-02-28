"""Model pydbwfserver configuration."""
from getopt import GetoptError, getopt
import os
import sys
from typing import Union

from antelope import stock


def usage():
    """Display a usage banner."""

    print(
        "\n\tUsage: pydbwfserver [-drevV] [-n nickname] "
        + "[-p pfname] [-P port] dbname\n"
    )


class DbwfserverConfig:
    """Object to hold the pydbwfserver configuration.

    Usage:
        config = DbwfserverConfig()
        config.configure()

    """

    _my_attrs = (
        "event",
        "pfname",
        "nickname",
        "style",
        "application_title",
        "static_dir",
        "jquery_dir",
        "template",
        "plot_template",
        "local_data",
        "antelope",
        "proxy_url",
        "port",
        "max_points",
        "max_traces",
        "apply_calib",
        "display_tracebacks",
        "display_arrivals",
        "display_points",
        "verbose",
        "debug",
        "daemonize",
        "default_time_window",
        "filters",
        "run_server",
        "realtime",
    )

    def __init__(self, opts: Union[list, None] = None):
        """Initialize the DbwfserverConfig object."""

        if opts is None:
            opts = []

        # Initialize object instance variables with None.
        for a in self.__class__._my_attrs:
            self.__dict__[a] = None

        # Set our instance defaults.
        self.opts = opts
        self.event = "false"
        self.pfname = "dbwfserver"
        self.nickname = ""
        self.application_title = ""
        self.dbname = ""
        self.port = None
        self.realtime = "false"
        self.verbose = False
        self.debug = False
        self.daemonize = False
        self.run_server = {}

    def configure(self):
        """Parse argments, read parameter file, and fix up options before handing configuration off to Twistd."""
        try:
            opts, pargs = getopt(self.opts, "dp:P:vVern:")
        except GetoptError:
            usage()
            sys.exit(-1)

        if len(pargs) == 1:
            self.dbname = pargs[0]

        for option, value in opts:

            if "-e" in option:
                self.event = "true"

            if "-p" in option:
                self.pfname = str(value)

            if "-r" in option:
                self.realtime = "true"

            if "-d" in option:
                self.daemonize = True

            if "-V" in option:
                self.debug = True
                self.verbose = True

            if "-v" in option:
                self.verbose = True

            if "-P" in option:
                self.port = int(value)

            if "-n" in option:
                self.nickname = str(value)

        #
        # Get values from pf file
        #
        pf = stock.pfread(self.pfname)

        if self.port is None:
            self.port = pf.get("port", 80)

        self.max_points = pf.get("max_points", -1)
        self.max_traces = pf.get("max_traces", -1)
        self.jquery_dir = pf.get("jquery_dir", "")
        self.static_dir = pf.get("static_dir", "")
        self.template = pf.get("template", "")
        self.plot_template = pf.get("plot_template", "")
        self.local_data = pf.get("local_data", "")
        self.style = pf.get("jquery_ui_style", "cupertino")
        self.antelope = pf.get("antelope", "")
        self.application_title = pf.get("application_title")
        self.proxy_url = pf.get("proxy_url", "")
        self.apply_calib = pf.get("apply_calib", False)
        self.display_tracebacks = pf.get("display_tracebacks", False)
        self.display_arrivals = pf.get("display_arrivals", True)
        self.display_points = pf.get("display_points", False)
        self.default_time_window = pf.get("default_time_window", -1)
        self.filters = list(pf.get("filters", []))

        """Fix paths and sanity check arguments before handoff to Twisted."""
        #
        # Fix paths
        #
        self.template = "%s/%s/%s" % (self.antelope, self.local_data, self.template)
        self.plot_template = "%s/%s/%s" % (
            self.antelope,
            self.local_data,
            self.plot_template,
        )
        self.jquery_dir = "%s/%s/%s" % (self.antelope, self.local_data, self.jquery_dir)
        self.static_dir = "%s/%s/%s" % (self.antelope, self.local_data, self.static_dir)

        #
        # Sanity check
        #
        if not os.path.isfile(self.template):
            sys.exit("\n\nERROR: No template found (%s)\n" % self.template)

        # Build dictionary of servers we want to run
        if self.dbname and self.port:

            # only one for now
            self.run_server = {int(self.port): str(self.dbname)}

        else:

            usage()
            sys.exit(
                "\n\nERROR: Not a valid db:port setup. (%s,%s)\n"
                % (self.dbname, self.port)
            )

        argv_remap = list()
        argv_remap.append(sys.argv[0])

        if not self.daemonize:
            argv_remap.append("-n")

        argv_remap.append("-y")
        argv_remap.append(
            os.path.join(
                os.environ["ANTELOPE"], "contrib/data/python/pydbwfserver/server.py"
            )
        )

        if os.path.isdir("./state"):
            pid_path = "./state"
        else:
            pid_path = "/tmp"
        argv_remap.append("--pidfile")
        argv_remap.append(pid_path + "/dbwfserver_" + str(os.getpid()) + ".pid")

        return argv_remap
