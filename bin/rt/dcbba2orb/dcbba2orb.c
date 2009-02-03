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
int iDCDataConnectionHandle = INVALID_HANDLE;
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
void dcbbaCleanup(int iExitCode);
void closeAndFreeHandle(int *iHandle);
char *getBBAStaFromSID(int iSID);
char *getBBADataTypeFromSRate(int iSampleRate);
int dcDataConnect(int iConnType, char *sConnectionParams[]);
int dcDataConnectFile(char *sFileName);
int dcDataConnectSocket(char *sHost, in_port_t iPort);
int setFileBlocking(int *iHandle, int iBlocking);
int doReadBytes(int *iHandle, char *sBuffer, unsigned int iByteCount,
		int bBlocking);
int doWriteBytes(int *iHandle, char *sBuffer, unsigned int iByteCount);
void sig_hdlr(int iSignal);

/*
 * Main program loop
 */
int main(int iArgCount, char *aArgList[]) {
	int iTestSID;

	elog_init(iArgCount, aArgList);

	/* Parse out command line options */
	if (parseCommandLineOptions(iArgCount, aArgList) == RESULT_SUCCESS) {

		/* Read in the parameter file */
		if (paramFileRead() == RESULT_FAILURE) {
			elog_complain(1,
					"main(): Error encountered during paramFileRead() operation.");
			dcbbaCleanup(-1);
		}

		/* Set up a signal handler to re-read the parameter file on SIGUSR1*/
		signal(SIGUSR1, sig_hdlr);

		/*** TEST ROUTINES ***/
		/* Test getBBAStaFromSID */
		iTestSID = 697;
		printf("SID %i is station %s\n", iTestSID, getBBAStaFromSID(iTestSID));

		/* Connect to Data Concentrator's Data read port */
		if (dcDataConnect(oConfig.iConnectionType, oConfig.sDCConnectionParams)
				== RESULT_SUCCESS) {

			/*** BEGIN MAIN LOOP ***/
			do {

			} while (TRUE);
		}

		/* Else unable to connect, cleanup with failure (-1) exit code */
		else
			dcbbaCleanup(-1);

	} else {
		elog_complain(1,
				"main(): Error encountered during parseCommandLineOptions() operation.");
		dcbbaCleanup(-1);
	}

	/* If we got this far, cleanup with success (0) exit code */
	dcbbaCleanup(0);
	return (0);
}

/*
 * Displays the command line options
 */
void showCommandLineUsage(void) {
	cbanner(
			VERSION,
			" [-V] [-v] [-a dcipaddr | -t testfilename ] [-d dcdataport] [-c dccommandport] [-o orbname] [-g paramfile] [-s statefile]",
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
	int bDataFileSet = FALSE;

	/* Initialize the CONFIG structure */
	oConfig.bVerboseModeFlag = FALSE;
	oConfig.iConnectionType = CONNECT_TCP_IP;
	oConfig.sDCConnectionParams[0] = "";
	oConfig.sDCConnectionParams[1] = DEFAULT_DC_DATA_PORT;
	oConfig.sDCConnectionParams[2] = DEFAULT_DC_CONTROL_PORT;
	oConfig.sOrbName = ":";
	oConfig.sParamFileName = "dcbba2orb.pf";
	oConfig.sStateFileName = NULL;
	oConfig.oSites = NULL;

	/* Loop through all possible options */
	while ((iOption = getopt(iArgCount, aArgList, "vVa:d:c:o:g:s:t:")) != -1) {
		switch (iOption) {
		case 'V':
			showCommandLineUsage();
			return RESULT_FAILURE;
			break;
		case 'v':
			oConfig.bVerboseModeFlag = TRUE;
			break;
		case 'a': /* Address of the DC */
			oConfig.sDCConnectionParams[0] = optarg;
			oConfig.iConnectionType = CONNECT_TCP_IP;
			bAddressSet = TRUE;
			break;
		case 'd': /* Port number of the data port on the DC */
			oConfig.sDCConnectionParams[1] = optarg;
			break;
		case 'c': /* Port number of the control port on the DC */
			oConfig.sDCConnectionParams[2] = optarg;
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
		case 't': /* Filename of the Data port test file */
			oConfig.sDCConnectionParams[0] = optarg;
			bDataFileSet = TRUE;
			oConfig.iConnectionType = CONNECT_FILE;
			break;

			/* Handle invalid arguments */
		default:
			elog_complain(
					0,
					"parseCommandLineOptions(): Invalid command line argument: '-%c'\n\n",
					iOption);
			showCommandLineUsage();
			return RESULT_FAILURE;
		}
	}

	/* Output a log header for our program */
	elog_notify(0, "%s\n", VERSION);

	/* Verify valid command line options & combinations */
	if ((bAddressSet == FALSE) && bDataFileSet == FALSE) {
		elog_complain(0,
				"parseCommandLineOptions(): No address for Data Concentrator specified.\n");
		showCommandLineUsage();
		return RESULT_FAILURE;
	}
	/* If we got this far, everything was fine! */
	return RESULT_SUCCESS;
}

/*
 * Performs "cleanup" tasks -- closes the connections to the data concentrator
 */
void dcbbaCleanup(int iExitCode) {
	/* Log that we exited */
	elog_notify(0, "dcbbaCleanup(): Exiting with code %i...\n", iExitCode);

	/* Close the data connection handle */
	closeAndFreeHandle(&iDCDataConnectionHandle);

	/* Exit */
	exit(iExitCode);
}

/*
 * Closes the specified file handle and sets it to INVALID_HANDLE to prevent
 * further read/write operations on the handle
 */
void closeAndFreeHandle(int *iHandle) {

	/* Only close it if it's not already closed */
	if (*iHandle != INVALID_HANDLE) {
		close(*iHandle);
		*iHandle = INVALID_HANDLE;
	}
}

/*
 * Reads the parameter file.
 */
int paramFileRead() {
	int ret;
	static int first = 1;
	if ((ret = pfupdate(oConfig.sParamFileName, &configpf)) < 0) {
		complain(0,
				"pfupdate(\"%s\", configpf): failed to open config file.\n",
				oConfig.sParamFileName);
		exit(-1);
	} else if (ret == 1) {
		if (first)
			elog_notify(0, "config file loaded %s\n", oConfig.sParamFileName);
		else
			elog_notify(0, "updated config file loaded %s\n",
					oConfig.sParamFileName);

		first = 0;

		oConfig.sNetworkName = pfget_string(configpf, "Network_Name");
		if (oConfig.bVerboseModeFlag == TRUE)
			elog_notify(0, "Network Name set to %s", oConfig.sNetworkName);

		oConfig.oSites = pfget_arr(configpf, "Site");
	}
	/* Return results */
	return RESULT_SUCCESS;
}

/*
 * Helper function to get a BBA Station Name from the SID encoded in the packet.
 * Uses oConfig.oSites pulled from the Sites array defined in the parameter file
 */
char *getBBAStaFromSID(int iSID) {
	char sbuf[5];
	/* Convert iSID to a string so we can do the pf lookup */
	sprintf(sbuf, "%i", iSID);
	return getarr(oConfig.oSites, sbuf);
}

/*
 * Get the data packet type
 *
 * BBA packets can be HS, BS, or LS in type, defined from their sample rate.
 * LS: <10 Hz
 * BS: 10-99 Hz
 * HS: >=100 Hz
 *
 * This information was taken from the parse_newbba function in libdefuntpkt2 pkttype.c
 */
char *getBBADataTypeFromSRate(int iSampleRate) {
	if (iSampleRate < 10) {
		return "LS";
	} else if ((iSampleRate >= 10) && (iSampleRate < 100)) {
		return "BS";
	} else if (iSampleRate >= 100) {
		return "HS";
	}

	/* We shouldn't ever get here unless iSampleRate is way out of range or NULL*/
	elog_complain(0,
			"Unable to determine DataType from given sample rate of %i",
			iSampleRate);
	return "";
}

/*
 *  Connect to the Data Concentrator's Data port
 */
int dcDataConnect(int iConnType, char *sConnectionParams[]) {
	/* Initialize */
	int iAttemptsMade = 0;
	int iResult = RESULT_SUCCESS;

	/* Check for invalid params */
	if ((sConnectionParams[0] == NULL) || ((iConnType == CONNECT_TCP_IP)
			&& sConnectionParams[1] == NULL)) {
		elog_complain(0, "dcDataConnect(): Bad connection parameters.   "
			"Expected '<host>,<port>' or "
			"'<filename>'.\n");
		return RESULT_FAILURE;
	}

	/* Main loop */
	if (oConfig.bVerboseModeFlag == TRUE) {
		elog_notify(0,
				"dcDataConnect(): Connecting to the Data Concentrator's data port...\n");
	}

	while (iAttemptsMade < MAX_CONNECT_ATTEMPTS) {

		/* Handle TCP/IP connections */
		if (iConnType == CONNECT_TCP_IP) {
			iResult = dcDataConnectSocket(sConnectionParams[0], atoi(
					sConnectionParams[1]));
		}

		/* Handle File Simulation */
		else if (iConnType == CONNECT_FILE) {
			iResult = dcDataConnectFile(sConnectionParams[0]);
		}

		/* If successful, break out of loop */
		if (iResult == RESULT_SUCCESS) {
			break;
		}

		/* Increment attempt counter */
		iAttemptsMade++;
		sleep(3);
	}

	/* If we failed too many times, log error */
	if (iAttemptsMade >= MAX_CONNECT_ATTEMPTS) {
		elog_complain(
				0,
				"dcDataConnect(): Aborting after %i failed connection attempts.\n",
				iAttemptsMade);
		return RESULT_FAILURE;
	}

	/* Return results */
	if ((iResult == RESULT_SUCCESS) && (oConfig.bVerboseModeFlag == TRUE)) {
		elog_notify(0,
				"dcDataConnect(): Connected to the Data Concentrator's data port.\n");
	}
	return iResult;
}

/*
 * Connect to the Data Concentrator's Data Port via TCP/IP
 * NOT IMPLEMENTED YET
 */
int dcDataConnectSocket(char *sHost, in_port_t iPort) {
	return RESULT_FAILURE;

	/* Initialize */
	unsigned long iNetAddress;
	int iConnectionResult = 1;
	struct hostent *oHostEnt;
	struct sockaddr_in oAddress;
	/*int					iSocketOptVal = 1; UNUSED, would be used if we set keepalive*/

	/* Check for host already being a numeric IP address */
	if ((iNetAddress = inet_addr(sHost)) != INADDR_NONE)
		memcpy(&oAddress.sin_addr, &iNetAddress, min (sizeof (iNetAddress), sizeof(oAddress.sin_addr)));

	/* Else resolve name to IP address */
	else {
		oHostEnt = gethostbyname(sHost);
		if (oHostEnt == NULL) {
			elog_complain(1,
					"dcDataConnectSocket(): Could not resovle address '%s'.",
					sHost);
			return RESULT_FAILURE;
		}
		memcpy(&oAddress.sin_addr, oHostEnt -> h_addr, min (oHostEnt -> h_length, sizeof (oAddress.sin_addr)));
	}

	/* Create socket */
	if ((iDCDataConnectionHandle = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		elog_complain(1, "dcDataConnectSocket(): Could not create socket.");
		return RESULT_FAILURE;
	}

	/* Extract address from host entry */
	oAddress.sin_family = AF_INET;
	oAddress.sin_port = htons (iPort);
	if (oConfig.bVerboseModeFlag == TRUE)
		elog_notify(0, "connectSocket(): Connecting to '%s' on port %i.\n",
				sHost, iPort);

	/* Try connecting */
	iConnectionResult = connect(iDCDataConnectionHandle,
			(struct sockaddr *) &oAddress, sizeof(oAddress));
	if (iConnectionResult) {
		elog_complain(1, "dcDataConnectSocket(): Could not connect to socket");
		close(iDCDataConnectionHandle);
		return RESULT_FAILURE;
	}

	/* Do we need to do Keepalive like in davis2orb? Not implementing yet*/

	/* If we got this far, everything was successful */
	return RESULT_SUCCESS;

}

/*
 * Read a packet dump file as a simulation of the DC Data Port
 *
 * Packet dump is simply a file containing only the data fields from the TCP/IP packet stream as extracted by Wireshark
 */
int dcDataConnectFile(char *sFileName) {
	/*  Attempt to open the file in Read-Only mode */
	iDCDataConnectionHandle = open(sFileName, O_RDONLY);
	if (iDCDataConnectionHandle < 0) {
		elog_complain(1,
				"dcDataConnectFile(): Unable to open dummy data file '%s'.",
				sFileName);
		return RESULT_FAILURE;
	}

	/* If we got here, return RESULT_SUCCESS */
	return RESULT_SUCCESS;
}

void sig_hdlr(int signo) {

	elog_notify(0, "sig_hdlr(): Got signal %d.\n", signo);
	elog_notify(0, "Re-reading parameter file to get new settings...");

	if (paramFileRead() == RESULT_FAILURE) {
		elog_complain(1,
				"main(): Error encountered during paramFileRead() operation.");
		dcbbaCleanup(-1);
	}

	elog_notify(0, "Done.\n");
	signal(SIGUSR1, sig_hdlr);
	return;
}

/*
 **  Sets the BLOCKING state of the specified file descriptor.
 **
 **  Parameters:
 **
 **    [*iHandle]   The file descriptor to operate on
 **    [iBlocking]  Integer value representing the state of BLOCKING
 **                 (0 = Disable; 1 = Enable)
 **
 **  Returns:
 **
 **    RESULT_SUCCESS on success, RESULT_FAILURE otherwise.
 */
int setFileBlocking(int *iHandle, int iBlocking) {

	int iTempVal = 0;

	/* Initialize */
	iTempVal = fcntl(*iHandle, F_GETFL, 0);
	if (iTempVal == -1) {
		elog_complain(1, "setFileBlocking: fcntl(*fd,F_GETFL,0)");
		return RESULT_FAILURE;
	}

	/* Set the BLOCKING mode */
	if (iBlocking == 1)
		iTempVal &= ~O_NONBLOCK;
	else
		iTempVal |= O_NONBLOCK;

	/* Set the state */
	iTempVal = fcntl(*iHandle, F_SETFL, iTempVal);
	if (iTempVal == -1) {
		elog_complain(1, "setFileBlocking: fcntl(*fd,F_SETFL,iTempVal)");
		return RESULT_FAILURE;
	}

	/* Return results */
	return RESULT_SUCCESS;
}

int doReadBytes(int *iHandle, char *sBuffer, unsigned int iByteCount,
		int bBlocking) {

	int iReturnVal = 0;
	int iBytesRead = 0;
	fd_set readfd;
	fd_set except;
	struct timeval timeout;
	int selret;

	/* Check for valid handle */
	if (*iHandle == INVALID_HANDLE)
		return RESULT_FAILURE;

	if (setFileBlocking(iHandle, FALSE) == RESULT_FAILURE) {
		elog_complain(0, "doReadBytes: failed to set device to non-blocking\n");
		close(*iHandle);
		*iHandle = INVALID_HANDLE;
		return RESULT_FAILURE;
	}

	while (1) {
		timeout.tv_sec = MAX_DC_READ_DELAY;
		timeout.tv_usec = 0;

		FD_ZERO(&readfd);
		FD_SET(*iHandle,&readfd);

		FD_ZERO(&except);
		FD_SET(*iHandle,&except);

		selret = select(*iHandle + 1, &readfd, NULL,&except, &timeout);

		if (selret < 0) {
			elog_complain(1, "doReadBytes: select() on read failed");
			close(*iHandle);
			*iHandle = INVALID_HANDLE;
			return RESULT_FAILURE;
		} else if (!selret) {
			if (bBlocking == TRUE) {
				elog_complain(0,
						"doReadBytes: timed out (%d seconds) in select()\n",
						MAX_DC_READ_DELAY);
				close(*iHandle);
				*iHandle = INVALID_HANDLE;
				return RESULT_FAILURE;
			} else
				return iBytesRead;
		} else {

			iReturnVal = read(*iHandle, &(sBuffer[iBytesRead]), iByteCount);

			/* See if we need to error out */
			if ((iReturnVal < 0) && (errno!= EAGAIN)) {
				elog_complain(1,
						"doReadBytes: Error encountered during read from Davis:");
				close(*iHandle);
				*iHandle = INVALID_HANDLE;
				return RESULT_FAILURE;
			} else if (errno== EAGAIN && bBlocking == FALSE) {
				return 0;
			} else if (iReturnVal > 0) {
				iBytesRead += iReturnVal;
				if (iBytesRead == iByteCount) {
					/* Restore BLOCKING on the connection */
					if (setFileBlocking(iHandle, TRUE) == RESULT_FAILURE) {
						close(*iHandle);
						*iHandle = INVALID_HANDLE;
						return RESULT_FAILURE;
					}

					return iBytesRead;
				}
			}
		}
	}
}

/*
 **  Wrapper for write() command; handles errors and checks the file descriptor
 **  for validity before attempting to write.
 **
 **  Returns the number of bytes written (>=0), or RESULT_FAILURE if there was
 **  a problem.
 */
int doWriteBytes(int *iHandle, char *sBuffer, unsigned int iByteCount) {

	/* Initialize */
	int iResult = RESULT_FAILURE;

	/* Check for valid handle */
	if (*iHandle == INVALID_HANDLE)
		elog_complain(0, "doWriteBytes: called with invalid file descriptor.\n");

	/* Else it's valid, try writing */
	else {
		iResult = write(*iHandle, sBuffer, iByteCount);
		if (iResult < iByteCount) {
			elog_complain(
					1,
					"doWriteBytes: Error calling 'write' (error code=%u, %d bytes written):",
					errno,iResult);
			close(*iHandle);
			*iHandle = INVALID_HANDLE;
			return RESULT_FAILURE;
		}
	}

	/* Return results */
	return iResult;
}
