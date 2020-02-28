"""
Utility functions and classes for pydbwfserver.

Used by resource.QueryParserResource and other items
"""

import logging
from string import Template
import time

logger = logging.getLogger(__name__)


class ProgressLogger:
    """Output a log message every time_interval seconds or tick_interval."""

    def __init__(
        self,
        name,
        total_ticks,
        time_interval=1,
        tick_interval=False,
        logger_instance=logging.getLogger(__name__),
        level=logging.INFO,
    ):
        """Initialize the ProgressLogger instance."""
        self.logger = logger_instance
        self.name = name
        self.total_ticks = total_ticks
        if self.total_ticks <= 0:
            self.logger.warning("Total ticks is %d, should be > 0. Using 1")
            self.total_ticks = 1
        self.tick_interval = tick_interval
        self.time_interval = time_interval

        self.last_output = -1
        self.start_time = time.time()
        self.current_tick = 0
        self.level = level

    def progress(self):
        """Get the current progress as a percentage."""
        if self.total_ticks == 0:
            self.logger.critical(
                "progress: Total ticks set to 0, should be greater than 0"
            )
            return 100
        return (float(self.current_tick) / self.total_ticks) * 100

    def tick(self):
        """
        Iterate the progress logger_instance by one tick.

        If the tick_interval or time_interval have been reached, output a log
        message
        """
        self.current_tick += 1
        time_now = time.time()
        if (
            (self.tick_interval > 0) and (self.current_tick % self.tick_interval == 0)
        ) or (time_now - self.last_output > self.time_interval):
            self.logger.log(self.level, self.name + self._get_tick_text(time_now))
            self.last_output = time_now

    def _get_tick_text(self, time_now):
        return "%d of %d (%.1f %%) %d s" % (
            self.current_tick,
            self.total_ticks,
            self.progress(),
            time_now - self.start_time,
        )

    def finish(self, level=None):
        """Signal that the task has finished."""
        if level is None:
            level = self.level
        time_now = time.time()
        self.logger.log(
            level,
            self.name
            + "Finished at "
            + str(time_now)
            + " "
            + self._get_tick_text(time_now),
        )


def load_template(template_path):
    """Load a template from a specified file."""
    with open(template_path, "r") as opened_file:
        return Template(opened_file.read())


def str2bool(test: str) -> bool:
    """Get a boolean value from an input string.

    The following case in-sensitive values are treated as True:
    * "yes"
    * "y"
    * "true"
    * "t"
    * "1"

    The following case in-sensitive values are treated as False:
    * "no"
    * "n"
    * "false"
    * "f"
    * "0"

    Raises:
        ValueError if test is not one of the above values

    """
    try:
        test = test.lower()
    except AttributeError:
        pass
    else:
        if test.lower() in ("yes", "y", "true", "t", "1"):
            return True
        elif test.lower() in ("no", "n", "false", "f", "0"):
            return False

    raise ValueError("%s could not be interpreted as a boolean.", test)
