BIN  = dbwfserver_extract
BIN += dbwfserver
PF =  dbwfserver.pf
MAN1 = dbwfserver.1

ldlibs = -lxtra $(TRLIBS) $(GPLLIBS)
SUBDIR = /contrib
CLEAN = *.pyc *.o $(BIN)
UNINSTALL = $(ANTELOPE)$(SUBDIR)/data/$(DATADIR)/*.pyc $(ANTELOPE)$(SUBDIR)/data/$(DATADIR)/*.pyc


include $(ANTELOPEMAKE)

test: install
	pfverify -v
	python -m unittest discover -s test_dbwfserver

DIRS  = Contents
DIRS += pydbwfserver