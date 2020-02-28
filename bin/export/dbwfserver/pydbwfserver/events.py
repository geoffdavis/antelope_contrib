"""The Events class for dbwfserver."""
from collections import defaultdict
import logging

from antelope import datascope, stock

from .config import DbwfserverConfig
from .dbcentral import Dbcentral, DbNulls


class Events:
    """Data structure and functions to query for events."""

    def __init__(self, dbcentral: Dbcentral, config: DbwfserverConfig):
        """Initialize Event object and retrieve data from the database."""

        self.logger = logging.getLogger(__name__)
        self.config = config
        self.first = True
        self.dbcentral = dbcentral
        self.event_cache = defaultdict(lambda: defaultdict(dict))
        self.offset = -1
        self.start = 0
        self.end = 0

        self.logger.debug("Events(): init() class")

        #
        # Load null class
        #
        self.logger.debug("Events(): self.nulls")
        self.nulls = DbNulls(
            self.config, dbcentral, ["events", "event", "origin", "assoc", "arrival"]
        )

        self._get_event_cache()

    def __getitem__(self, i):
        """Produce iteration context of Events."""

        return list(self.event_cache.keys())[i]

    def next(self):
        """Produce items util Stopiteration is raised."""

        if len(self.event_cache.keys()) == self.offset:

            self.offset = -1
            raise StopIteration

        else:

            self.offset += 1
            return list(self.event_cache.keys())[self.offset]

    def __str__(self):
        """Nicely format elements in class."""

        text = "Events: "
        for orid in self.event_cache:
            text += "\t%s(%s)" % (orid, self.event_cache[orid])

        return text

    def __call__(self, value):
        """Intercept data requests."""

        try:
            value = float(value)
        except ValueError:
            return "Not a valid number in function call: %s" % value

        if value in self.event_cache:
            return self.event_cache[value]
        else:
            self.logger.warning("Events(): %s not in database." % value)
            return self.list

    def list(self):
        """Produce station_patterns of event key names."""
        return list(self.event_cache.keys())

    def table(self):
        """Produce table representation of self."""
        return dict(self.event_cache)

    def time(self, orid_time, window=5):
        """Find an event near a given origin time.

        Look for event id close to a value of epoch time + or - window time in seconds.
        If no widow time is provided the default is 5 secods.
        """

        results = {}

        #
        # If running in simple mode we don't have access to the tables we need
        #
        if self.config.simple:
            return results

        try:
            orid_time = float(orid_time)
        except ValueError:
            self.logger.error("Not a valid number in function call: %s" % orid_time)
            return None

        start = float(orid_time) - float(window)
        end = float(orid_time) + float(window)

        dbname = self.dbcentral(orid_time)

        if not dbname:
            self.logger.error(
                "No match for orid_time in dbcentral object: (%s,%s)"
                % (orid_time, self.dbcentral(orid_time))
            )
            return None

        try:
            db = datascope.dbopen(dbname, "r")
            db = db.lookup(table="origin")
            db.query(datascope.dbTABLE_PRESENT)
        except Exception as e:
            self.logger.error(
                "Exception on Events() time(%s): " "Error on db pointer %s [%s]",
                orid_time,
                db,
                e,
            )
            return None

        db = db.subset("time >= %f" % start)
        db = db.subset("time <= %f" % end)

        try:
            db = datascope.dbopen(dbname, "r")
            db = db.lookup(table="wfdisc")
            records = db.query(datascope.dbRECORD_COUNT)

        except datascope.DatascopeException:
            records = 0

        if records:

            for i in range(records):

                db.record = i

                (orid, record_time) = db.getv("orid", "time")

                try:
                    orid = int(orid)
                except ValueError:
                    orid = None
                try:
                    record_time = float(record_time)
                except ValueError:
                    record_time = None
                results[orid] = record_time

        return results

    def _get_event_cache(self):
        # private function to load the data from the tables

        self.logger.info("Events(): update cache")

        for dbname in self.dbcentral.list():

            self.logger.debug("Events(): dbname: %s" % dbname)

            # Get min max for wfdisc table first
            start = end = None
            db = None
            try:
                db = datascope.dbopen(dbname, "r")
                db = db.lookup(table="wfdisc")
                start = db.ex_eval("min(time)")
                end = db.ex_eval("max(endtime)")
                if end > stock.now():
                    end = stock.now()
                records = db.query(datascope.dbRECORD_COUNT)

            except datascope.DatascopeException:
                records = 0

            if records:

                if not self.start:
                    self.start = start

                elif self.start > start:
                    self.start = start

                if not self.end:
                    self.end = end

                elif self.end < end:
                    self.end = end

            if db:
                db.close()

            try:
                db = datascope.dbopen(dbname, "r")
                db = db.lookup(table="event")
                records = db.query(datascope.dbRECORD_COUNT)

            except datascope.DatascopeException:
                records = 0

            if records:

                try:
                    db = db.join("origin")
                    db = db.subset("orid == prefor")
                except datascope.DatascopeException:
                    pass

            else:

                try:
                    db = db.lookup(table="origin")
                except datascope.DatascopeException:
                    pass

            try:
                records = db.query(datascope.dbRECORD_COUNT)
            except datascope.DatascopeException:
                records = 0

            if not records:
                self.logger.error("Events(): No records to work on any table")
                continue

            self.logger.debug(
                "Events(): origin db_pointer: [%s,%s,%s,%s]"
                % (db["database"], db["table"], db["field"], db["record"])
            )

            try:
                db = db.subset("time > %f" % self.start)
                db = db.subset("time < %f" % self.end)
            except datascope.DatascopeException:
                pass

            try:
                records = db.query(datascope.dbRECORD_COUNT)
            except datascope.DatascopeException:
                records = 0

            if not records:
                self.logger.error("Events(): No records after time subset")
                continue

            for i in range(records):

                db.record = i

                (orid, time, lat, lon, depth, auth, mb, ml, ms, nass) = db.getv(
                    "orid",
                    "time",
                    "lat",
                    "lon",
                    "depth",
                    "auth",
                    "mb",
                    "ml",
                    "ms",
                    "nass",
                )

                if auth == self.nulls("auth"):
                    auth = "-"

                if orid == self.nulls("orid"):
                    orid = "-"

                if time == self.nulls("time"):
                    time = "-"
                else:
                    time = "%0.2f" % time

                if lat == self.nulls("lat"):
                    lat = "-"
                else:
                    lat = "%0.2f" % lat

                if lon == self.nulls("lon"):
                    lon = "-"
                else:
                    lon = "%0.2f" % lon

                if depth == self.nulls("depth"):
                    depth = "-"
                else:
                    depth = "%0.2f" % depth

                if mb == self.nulls("mb"):
                    mb = "-"
                else:
                    mb = "%0.1f" % mb

                if ms == self.nulls("ms"):
                    ms = "-"
                else:
                    ms = "%0.1f" % ms

                if ml == self.nulls("ml"):
                    ml = "-"
                else:
                    ml = "%0.1f" % ml

                if nass == self.nulls("nass"):
                    nass = "-"
                else:
                    nass = "%d" % nass

                self.event_cache[orid] = defaultdict(
                    time=time,
                    lat=lat,
                    lon=lon,
                    depth=depth,
                    auth=auth,
                    mb=mb,
                    ms=ms,
                    ml=ml,
                    nass=nass,
                )

                if mb > 0:
                    self.event_cache[orid]["magnitude"] = mb
                    self.event_cache[orid]["mtype"] = "Mb"
                elif ms > 0:
                    self.event_cache[orid]["magnitude"] = ms
                    self.event_cache[orid]["mtype"] = "Ms"
                elif ml > 0:
                    self.event_cache[orid]["magnitude"] = ml
                    self.event_cache[orid]["mtype"] = "Ml"
                else:
                    self.event_cache[orid]["magnitude"] = "-"
                    self.event_cache[orid]["mtype"] = "-"

            try:
                db.close()
            except Exception:
                pass

        self.logger.info("Events(): Done updating cache. (%s)" % len(self.event_cache))

        self.logger.debug("Events(): %s" % self.event_cache.keys())

    def phases(self, phase_start_time, phase_end_time):
        """Retrieve all arrival phases for an event."""

        self.logger.debug(
            "Events():phases(%s,%s) " % (phase_start_time, phase_end_time)
        )

        phases = defaultdict(lambda: defaultdict(dict))

        assoc = False

        dbname = self.dbcentral(phase_start_time)

        self.logger.debug(
            "Events():phases(%s,%s) db:(%s)"
            % (phase_start_time, phase_end_time, dbname)
        )

        if not dbname:
            return phases

        open_dbviews = []
        with datascope.closing(datascope.dbcreate(dbname, "r")) as db:
            with datascope.freeing(db.lookup(table="arrival")) as db_arrivals:
                try:
                    db_arrival_assoc = db_arrivals.join("assoc")
                    open_dbviews.append(db_arrival_assoc)
                    dbv = db_arrival_assoc
                except datascope.DatascopeException:
                    dbv = db_arrivals

                # This "try/finally" block is to emulate a context manager for a successful join with the assoc table.
                try:
                    nrecs = dbv.query(datascope.dbRECORD_COUNT)

                    if not nrecs:
                        return dict(phases)

                    try:
                        db = db.subset(
                            "%s <= time && time <= %s"
                            % (float(phase_start_time), float(phase_end_time))
                        )
                        nrecs = db.query(datascope.dbRECORD_COUNT)
                    except datascope.DatascopeException:
                        nrecs = 0

                    if not nrecs:
                        return dict(phases)

                    for p in range(nrecs):
                        db.record = p

                        if assoc:
                            phase_field = "phase"
                        else:
                            phase_field = "iphase"

                        Sta, Chan, ArrTime, Phase = db.getv(
                            "sta", "chan", "time", phase_field
                        )
                        StaChan = Sta + "_" + Chan
                        phases[StaChan][ArrTime] = Phase

                        self.logger.debug("Phases(%s):%s" % (StaChan, Phase))
                finally:
                    for view in open_dbviews:
                        view.close()

        self.logger.debug(
            "Events: phases(): t1=%s t2=%s [%s]"
            % (phase_start_time, phase_end_time, phases)
        )

        return dict(phases)
