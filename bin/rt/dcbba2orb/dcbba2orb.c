/*
 * Constants
 */
#define VERSION "dcbba2orb $Revision$"

/*
 * Includes
 */
#include "dcbba2orb.h"

/*
 * Globals
 */
int                     orbfd=-1;
Pf                      *configpf=NULL;
struct stConfigData     oConfig;

/*
 * State Variables
 */

/*
 * Prototypes
 */
void showCommandLineUsage (void);
int parseCommandLineOptions (int iArgCount, char *aArgList []);
int paramFileRead();

static void
usage (void)
{
    cbanner ( "$Date: 2008/01/22 00:33:50 $", 
	      "[-m match] [-r reject] [-S statefile] [-v]"
	          "in out [start-time [period|end-time]]\n",
	      "Geoff Davis", 
	      "IGPP/SIO/UCSD", 
	      "gadavis@ucsd.edu" ) ;
    exit (1);
}

#define VERY_LARGE_NUMBER   1e36

int
main (int argc, char **argv)
{

    int             c,
                    errflg = 0;

    char           *in,
                   *out;
    int             orbin,
                    orbout;
    double          maxpkts = VERY_LARGE_NUMBER ;
    int             quit;
    char           *match = 0,
                   *reject = 0;
    int             nmatch;
    int             specified_after = 0;
    double          after = 0.0,
                    until = VERY_LARGE_NUMBER ;
    double          start_time,
                    end_time,
                    delta_t ;
    double          totpkts = 0,
                    totbytes = 0;
    static int      last_pktid = -1;
    static double   last_pkttime = 0.0;
    char           *statefile = 0;
    double          last_burial = 0.0;
    double          decent_interval = 300.0;
    int             mode = PKT_NOSAMPLES;
    int             rcode;
    char            srcname[ORBSRCNAME_SIZE];
    double          pkttime = 0.0 ;
    int             pktid;
    int             nbytes;
    char           *packet = 0;
    int             packetsz = 0;
    Packet         *unstuffed = 0;

    elog_init (argc, argv);
    elog_notify (0, "%s $Revision: 1.5 $ $Date: 2008/01/22 00:33:50 $\n",
		 Program_Name);

    while ((c = getopt (argc, argv, "m:n:r:S:v")) != -1) {
	switch (c) {
	  case 'm':
	    match = optarg;
	    break;

	  case 'n':
	    maxpkts = atoi (optarg);
	    break;

	  case 'r':
	    reject = optarg;
	    break;

	  case 'S':
	    statefile = optarg;
	    break;

	  case 'v':
	    oConfig.bVerboseModeFlag++;
	    break;

	  case 'V':
	    usage ();
	    break;

	  case '?':
	    errflg++;
	}
    }

    if (errflg || argc - optind < 2 || argc - optind > 4)
	usage ();

    in = argv[optind++];
    out = argv[optind++];

    if (argc > optind) {
	after = str2epoch (argv[optind++]);
	specified_after = 1;
	if (argc > optind) {
	    until = str2epoch (argv[optind++]);
	    if (until < after) {
		until += after ;
	    }
	}
    }
    if ((orbin = orbopen (in, "r&")) < 0)
	die (0, "Can't open input '%s'\n", in);

    if (statefile != 0) {
	char           *s;
	if (exhume (statefile, &quit, RT_MAX_DIE_SECS, 0) != 0) {
	    elog_notify (0, "read old state file\n");
	}
	if (orbresurrect (orbin, &last_pktid, &last_pkttime) == 0) {
	    elog_notify (0, "resurrection successful: repositioned to pktid #%d @ %s\n",
			 last_pktid, s = strtime (last_pkttime));
	    free (s);
	} else {
	    complain (0, "resurrection unsuccessful\n");
	}
    }
    if ((orbout = orbopen (out, "w&")) < 0) {
	die (0, "Can't open output '%s'\n", out);
    }
    if (match) {
	nmatch = orbselect (orbin, match);
    }
    if (nmatch < 0) {
	die (1, "select '%s' returned %d\n", match, nmatch);
    }
    if (reject) {
	nmatch = orbreject (orbin, reject);
    }
    if (nmatch < 0) {
	die (1, "reject '%s' returned %d\n", reject, nmatch);
    } else {
	printf ("%d sources selected\n", nmatch);
    }

    if (specified_after) {
	pktid = orbafter (orbin, after);
	if (pktid < 0) {
	    char           *s;
	    complain (1, "seek to %s failed\n", s = strtime (after));
	    free (s);
	    pktid = forbtell (orbin);
	    printf ("pktid is still #%d\n", pktid);
	} else {
	    printf ("new starting pktid is #%d\n", pktid);
	}
    }
    start_time = now ();
    while (!quit && pkttime < until && totpkts < maxpkts) {
	rcode = orbreap (orbin,
		    &pktid, srcname, &pkttime, &packet, &nbytes, &packetsz);

	switch (rcode) {
	  case 0:
	    totpkts++;
	    totbytes += nbytes;

	    if (oConfig.bVerboseModeFlag) {
		showPkt (pktid, srcname, pkttime, packet, nbytes, stdout, mode);
	    }
	    if (statefile != 0
		    && last_pkttime - last_burial > decent_interval) {
		bury ();
		last_burial = pkttime;
	    }
	    switch (unstuffPkt (srcname, pkttime, packet, nbytes, &unstuffed)) {
	      case Pkt_wf:
		break;

	      case Pkt_db:
		break;

	      case Pkt_pf:
		break;

	      case Pkt_ch:
		break;

	      default:
		break;
	    }

	    if (orbput (orbout, srcname, pkttime, packet, nbytes) < 0) {
		complain (0, "Couldn't send packet to %s\n", out);
		break;
	    }
	    last_pktid = pktid;
	    last_pkttime = pkttime;
	    break;

	  default:
	    break;
	}

    }

    if (statefile != 0)
	bury ();

    end_time = now ();
    delta_t = end_time - start_time;
    if (totpkts > 0) {
	printf ("\n%.0f %.2f byte packets (%.1f kbytes) in %.3f seconds\n\t%10.3f kbytes/s\n\t%10.3f kbaud\n\t%10.3f pkts/s\n",
		totpkts, totbytes / totpkts, totbytes / 1024,
		delta_t,
		totbytes / delta_t / 1024,
		totbytes / delta_t / 1024 * 8,
		totpkts / delta_t);
    } else {

	printf ("\nno packets copied\n");
    }

    if (orbclose (orbin)) {
	complain (1, "error closing read orb\n");
    }
    if (orbclose (orbout)) {
	complain (1, "error closing write orb\n");
    }
    return 0;
}
