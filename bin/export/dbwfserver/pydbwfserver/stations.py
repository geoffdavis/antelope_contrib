"""The Stations class for dbwfserver."""
from collections import defaultdict
import logging
import re
import sys
import time

from dbcentral import Dbcentral
from twisted.internet import reactor
from util import ProgressLogger

from antelope import datascope, stock


class Stations:
    """Data structure and functions to query for stations."""

    def __init__(self, config, db: Dbcentral):
        """Load class and get the data."""

        self.logger = logging.getLogger(__name__)
        self.config = config
        self.first = True
        self.dbcentral = db
        self.stachan_cache = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
        self.wfdisc_stachan = defaultdict(set)
        self.offset = -1
        self.wfdates = defaultdict(lambda: defaultdict(dict))
        self.maxtime = -1
        self.mintime = 0

        self.logger.debug("init() class")

        self._get_stachan_cache()

    def __getitem__(self, i):
        """Act as an Iteration context."""
        k = list(self.stachan_cache.keys())
        return k[i]

    def next(self):
        """Produce stations from the stachan cache."""

        if len(self.stachan_cache.keys()) == self.offset:

            self.offset = -1
            raise StopIteration

        else:

            return list(self.stachan_cache.keys())[self.offset]

    def __str__(self):
        """Nicely format elements in class."""

        text = "Stations(): "
        for st in self.stachan_cache.keys():
            chans = self.stachan_cache[st].keys()
            text += "\t%s: %s" % (st, chans)

        return text

    def __call__(self, station):
        """Intercept data requests."""

        if station in self.stachan_cache:
            self.logger.debug(
                "Stations: %s => %s" % (station, self.stachan_cache[station])
            )
            return self.stachan_cache[station]

        else:
            self.logger.warning("Stations(): No value for station:%s" % station)
            for sta in self.stachan_cache:
                for chan in self.stachan_cache[sta]:
                    self.logger.debug(
                        "\t%s.%s => %s" % (sta, chan, self.stachan_cache[sta][chan])
                    )

        return False

    def _get_stachan_cache(self):
        """Load data into cache."""

        self.logger.info("Stations(): update cache")

        for dbname in self.dbcentral.list():

            self.logger.debug("Station(): dbname: %s" % dbname)

            dates = {}

            query_start_time = time.time()
            try:
                self.logger.debug("Dbopen " + dbname)
                db = datascope.dbopen(dbname, "r")
                table = "wfdisc"
                field = "time"
                self.logger.debug("Dblookup table=%s field=%s" % (table, field))
                dbwfdisc = db.lookup(table=table, field=field)
                self.logger.debug("Getting record count of " + table)
                records = dbwfdisc.query(datascope.dbRECORD_COUNT)
                self.mintime = dbwfdisc.ex_eval("min(time)")
                self.maxtime = dbwfdisc.ex_eval("max(endtime)")
            except Exception as e:
                self.logger.exception(
                    "Problem with wfdisc table. %s: %s" % (Exception, e)
                )
                sys.exit(reactor.stop())

            elapsed_time = time.time() - query_start_time
            self.logger.debug(
                "Intial dbquery and wfdisc record count took %d seconds" % elapsed_time
            )
            if self.maxtime > stock.now() or self.maxtime > (stock.now() - 3600):
                self.maxtime = -1

            self.logger.debug("Starting wfdisc processing of %d records" % records)
            prog = ProgressLogger(
                "Stations: processing wfdisc record ",
                records,
                logger_instance=self.logger,
            )
            for j in range(records):
                prog.tick()
                dbwfdisc.record = j

                try:
                    sta, chan, dbtime = dbwfdisc.getv("sta", "chan", "time")
                    self.wfdisc_stachan[sta].add(chan)
                    self.wfdates[stock.yearday(dbtime)] = 1
                except datascope.DatascopeException as e:
                    self.logger.exception("(%s=>%s)" % (Exception, e))

            prog.finish()
            self.logger.debug("Stations(): maxtime: %s" % self.maxtime)
            self.logger.debug("Stations(): mintime: %s" % self.mintime)
            self.logger.debug("Stations(): dates: %s" % dates.keys())

            try:
                dbsitechan = db.lookup(table="sitechan")
                ssc = dbsitechan.sort(["sta", "chan"])
                records = ssc.query(datascope.dbRECORD_COUNT)

            except Exception as e:
                self.logger.exception(
                    "Stations(): Problems with sitechan table %s: %s" % (Exception, e)
                )
                sys.exit(reactor.stop())

            if not records:
                self.logger.critical("Stations(): No records after sitechan sort.")
                sys.exit(reactor.stop())

            prog = ProgressLogger(
                "Stations: processing stachan record ",
                records,
                logger_instance=self.logger,
            )
            for j in range(records):
                prog.tick()

                ssc.record = j
                sta = chan = ondate = offdate = None
                try:
                    sta, chan, ondate, offdate = ssc.getv(
                        "sta", "chan", "ondate", "offdate"
                    )
                except Exception as e:
                    self.logger.exception("Station(): (%s=>%s)" % (Exception, e))

                ondate = stock.str2epoch(str(ondate))
                if chan in self.wfdisc_stachan[sta]:
                    if offdate != -1:
                        offdate = stock.str2epoch(str(offdate))

                    self.stachan_cache[sta][chan]["dates"].extend([[ondate, offdate]])

                    self.logger.debug(
                        "Station(): %s.%s dates: %s"
                        % (sta, chan, self.stachan_cache[sta][chan]["dates"])
                    )
                else:
                    self.logger.debug(
                        "Station(): %s.%s was not in the wfdisc. Skipping" % (sta, chan)
                    )

            try:
                ssc.free()
                db.close()
            except Exception:
                pass

            prog.finish(level=logging.INFO)

        self.logger.info(
            "Stations(): Done updating cache (%s) sta-chan pairs."
            % len(self.stachan_cache)
        )

    def min_time(self):
        """Get time of first wfdisc sample."""

        return self.mintime

    def max_time(self):
        """Get time of last wfdisc sample."""

        if self.maxtime == -1:
            return stock.now()

        return self.maxtime

    def stadates(self, start=False, end=False):
        """
        Determine start and end times for a station.

        Get station_patterns of valid dates
        """

        if not start:
            return self.stachan_cache.keys()

        cache = {}

        if not end:
            end = stock.now()
        if start > end:
            end = stock.now()
        start = float(start)
        end = float(end)

        for sta in self.stachan_cache:
            for chan in self.stachan_cache[sta]:
                for date in self.stachan_cache[sta][chan]["dates"]:

                    if date[1] == -1:

                        if date[0] <= start:
                            cache[sta] = 1
                        if date[0] <= end:
                            cache[sta] = 1

                    else:

                        if date[0] <= start <= date[1]:
                            cache[sta] = 1
                        if date[0] <= end <= date[1]:
                            cache[sta] = 1
                        if start <= date[0] and date[1] <= end:
                            cache[sta] = 1

        self.logger.info("cache.keys: %s", str(cache.keys()))
        return cache.keys()

    def dates(self):
        """Return start and end times for a station."""
        return list(self.wfdates.keys())

    def get_channels(self, station=None):
        """Get unique station_patterns of channels."""
        if station is None:
            station = []
        channels = {}

        if station:

            for sta in station:
                if sta in self.stachan_cache:

                    for ch in self.stachan_cache[sta]:
                        channels[ch] = 1
                else:

                    return False
        else:

            for st in self.stachan_cache.keys():

                for ch in self.stachan_cache[st]:
                    channels[ch] = 1

        return list(channels.keys())

    def convert_sta(self, station_patterns=None):
        """Get station_patterns of stations for the query."""

        if station_patterns is None:
            station_patterns = [r".*"]
        stations = []
        keys = {}

        self.logger.debug("Stations(): convert_sta(%s)" % station_patterns)

        for test in station_patterns:

            if re.search(r"^\w*$", test):
                stations.extend([x for x in self.stachan_cache if x == test])

            else:

                if not re.search(r"^\^", test):
                    test = r"^" + test
                if not re.search(r"\$$", test):
                    test = test + r"$"

                stations.extend([x for x in self.stachan_cache if re.search(test, x)])

        for s in stations:
            keys[s] = 1

        stations = keys.keys()

        self.logger.debug(
            "Stations(): convert_sta(%s) => %s" % (station_patterns, stations)
        )

        return stations

    def list(self):
        """List of station names in this Stations object."""
        return list(self.stachan_cache.keys())
