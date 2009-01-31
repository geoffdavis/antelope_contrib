/*
 *  NAMING CONVENTIONS
 *
 *    The following variable naming conventions are used throughout this code:
 *    xVarName, where "x" is one of the following:
 *
 *      [i]  Integer variable
 *      [c]  Character variable
 *      [s]  String variable
 *      [a]  Array variable
 *      [o]  Object/struct variable
 *      [st] Struct definition
 *      [b]  Boolean (psuedo) variable (use FALSE and TRUE constants defined
 *           in "dcbba2orb.h")
 */
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
int iDataConnectionHandle = INVALID_HANDLE;
int orbfd = -1;
Pf *configpf = NULL;
struct stConfigData oConfig;

/*
 * State Variables
 */

/*
 * Prototypes
 */
void showCommandLineUsage(void);
int parseCommandLineOptions(int iArgCount, char *aArgList[]);
int paramFileRead(void);
void dcbbaCleanup (int iExitCode);
void closeAndFreeHandle (int *iHandle);
char *getBBAStaFromSID (int iSID);
int iTestSID;

/*
 * Main program loop
 */
int main(int iArgCount, char *aArgList[]) {

	elog_init(iArgCount, aArgList);

	/* Parse out command line options */
	if (parseCommandLineOptions(iArgCount, aArgList) == RESULT_SUCCESS) {
		if (paramFileRead () == RESULT_FAILURE) {
			elog_complain (1, "main(): Error encountered during paramFileRead() operation.");
			dcbbaCleanup (-1);
		}
	}
	else{
		elog_complain (1, "main(): Error encountered during parseCommandLineOptions() operation.");
		dcbbaCleanup (-1);
	}

	/* Test getBBAStaFromSID */
	iTestSID=697;
	printf ("SID %i is station %s\n",iTestSID,getBBAStaFromSID(iTestSID));

	/* If we got this far, cleanup with success (0) exit code */
	dcbbaCleanup(0);
	return (0);
}

/*
 * Displays the command line options
 */
void showCommandLineUsage(void) {
	cbanner (VERSION,
			" [-V] [-v] -a dcipaddr [-d dcdataport] [-c dccommandport] [-o orbname] [-g paramfile] [-s statefile]",
			"Geoff Davis", "IGPP, UCSD", "gadavis@ucsd.edu");
}

/*
 * Parses out the command line options. Returns RESULT_SUCCESS if all options
 * are good and within limits, RESULT_FAILURE if there was a problem or that
 * the program needn't continue running
 */
int parseCommandLineOptions(int iArgCount, char *aArgList[]) {
	int iOption = '\0';
	int bAddressSet = FALSE;

	/* Initialize the CONFIG structure */
	oConfig.bVerboseModeFlag = FALSE;
	oConfig.sDCAddress = "";
	oConfig.sDCDataPort = DEFAULT_DC_DATA_PORT;
	oConfig.sDCControlPort = DEFAULT_DC_CONTROL_PORT;
	oConfig.sOrbName = ":";
	oConfig.sParamFileName = "dcbba2orb.pf";
	oConfig.sStateFileName = NULL;
	oConfig.aSites = NULL;

	/* Loop through all possible options */
	while ((iOption = getopt (iArgCount, aArgList, "vVa:d:c:o:g:s:")) != -1) {
		switch (iOption) {
		case 'V':
			showCommandLineUsage();
			return RESULT_FAILURE;
			break;
		case 'v':
			oConfig.bVerboseModeFlag = TRUE;
			break;
		case 'a':
			oConfig.sDCAddress = optarg;
			bAddressSet = TRUE;
			break;
		case 'd':
			oConfig.sDCDataPort = optarg;
			break;
		case 'c':
			oConfig.sDCControlPort = optarg;
			break;
		case 'o':
			oConfig.sOrbName = optarg;
			break;
		case 'g':
			oConfig.sParamFileName = optarg;
			break;
		case 's':
			oConfig.sStateFileName = optarg;
			break;

		/* Handle invalid arguments */
		default:
			elog_complain (0, "parseCommandLineOptions(): Invalid command line argument: '-%c'\n\n", iOption);
			showCommandLineUsage();
			return RESULT_FAILURE;
		}
	}

	/* Output a log header for our program */
	elog_notify (0, "%s\n", VERSION);

	/* Verify valid command line options & combinations */
	if (bAddressSet == FALSE) {
		elog_complain (0, "parseCommandLineOptions(): No address for Data Concentrator specified.\n");
		showCommandLineUsage();
		return RESULT_FAILURE;
	}
	/* If we got this far, everything was fine! */
	return RESULT_SUCCESS;
}

/*
 * Performs "cleanup" tasks -- closes the connections to the data concentrator
 */
void dcbbaCleanup (int iExitCode) {
	/* Log that we exited */
	elog_notify (0, "dcbbaCleanup(): Exiting with code %i...\n", iExitCode);

	/* Close the data connection handle */
	closeAndFreeHandle (&iDataConnectionHandle);

	/* Exit */
	exit (iExitCode);
}

/*
 * Closes the specified file handle and sets it to INVALID_HANDLE to prevent
 * further read/write operations on the handle
 */
void closeAndFreeHandle (int *iHandle) {

	/* Only close it if it's not already closed */
	if (*iHandle != INVALID_HANDLE) {
		close (*iHandle);
		*iHandle = INVALID_HANDLE;
	}
}

/*
 * Reads the parameter file.
 */
int paramFileRead () {
	int ret;
	static int first=1;
	if ((ret=pfupdate(oConfig.sParamFileName,&configpf))<0)
	{
		complain(0,"pfupdate(\"%s\", configpf): failed to open config file.\n",oConfig.sParamFileName);
		exit (-1);
	}
	else if (ret==1){
		if (first)
			elog_notify(0,"config file loaded %s\n",oConfig.sParamFileName);
		else
			elog_notify(0,"updated config file loaded %s\n",oConfig.sParamFileName);

		first=0;

		oConfig.sNetworkName = pfget_string(configpf, "Network_Name");
		if (oConfig.bVerboseModeFlag==TRUE)
			elog_notify(0,"Network Name set to %s",oConfig.sNetworkName);

		oConfig.aSites = pfget_arr(configpf, "Site");
	}
	/* Return results */
	return RESULT_SUCCESS;
}

char *getBBAStaFromSID (int iSID) {
	char sbuf[5];
	/* Convert iSID to a string so we can do the pf lookup */
	sprintf(sbuf,"%i",iSID);
	return getarr(oConfig.aSites, sbuf);
}
