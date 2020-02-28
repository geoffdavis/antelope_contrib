"""The main application for pydbwfserver."""

import logging
import os
import platform
import sys

from pydbwfserver.config import DbwfserverConfig
from twisted.scripts.twistd import run

logger = logging.basicConfig()

# Necessary for overriding the twisted run function.
config = DbwfserverConfig()


def system_print():
    """Print system info in case of errors."""

    print()
    print("uname:", platform.uname())
    print()
    print("Sys Version  :", sys.version_info)
    print("Py Version   :", platform.python_version())
    print("Compiler     :", platform.python_compiler())
    print("Build        :", platform.python_build())
    print()
    print("system   :", platform.system())
    print("node     :", platform.node())
    print("release  :", platform.release())
    print("version  :", platform.version())
    print("machine  :", platform.machine())
    print("processor:", platform.processor())


def main(argv):
    """
    Run the twisted reactor, overriding the twistd class with new args.

    Uses the configuration generated by
    pydbwfserver.configuration.DbwfserverConfig() to override the arguments that
    are passed to twisted.scripts.twistd. The DbwfserverConfig is hard-coded to
    pass the filename of server.py to twistd as an argument.

    See server.py for the code that is executed when
    twisted.scripts.twistd.run() is called.
    """

    # Refer to global config var, necessary for twistd override
    global config

    # Always release the Python GIL when using the Antelope bindings
    os.environ["ANTELOPE_PYTHON_GILRELEASE"] = "1"

    # Set up the logging infrastructure
    logger = logging.getLogger("pydbwfserver")

    # Configure system with command-line flags and pf file values.
    config.opts = argv[1:]
    sys.argv = config.configure()

    if config.verbose:
        logger.setLevel(logging.INFO)

    if config.debug:
        logger.setLevel(logging.DEBUG)

    logger.info("Start Server!")

    run()


if __name__ == "__main__":
    exit(main(sys.argv))
