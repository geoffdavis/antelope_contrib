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
 *      [us] unsigned short variable
 *      [st] Struct definition
 *      [d]	 double variable
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
Bns *oDCDataBNS = NULL;
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
int getBBAStaFromSID(int iStaID, char *sStaName);
int getBBADataTypeFromSRate(float fSampleRate, char *sDataType);
int dcDataConnect(int iConnType, char *sConnectionParams[]);
int dcDataConnectFile(char *sFileName);
int dcDataConnectSocket(char *sHost, in_port_t iPort);
void sig_hdlr(int iSignal);
int validateBBAChecksum(unsigned char *aBBAPkt, int iPktLength);
int parseBBAPacket(unsigned char *aBBAPkt, struct stBBAPacketInfo* oPktInfo);
int readFromDC(struct stBBAPacketInfo *oPktInfo, unsigned char *aBBAPkt);

/*
 * Main program loop
 */
int main(int iArgCount, char *aArgList[]) {

	unsigned char *aBBAPkt; /* Buffer to hold a bba packet read from the wire */
	int iPktCounter = 0;
	unsigned char cIn;
	int iReadState = 0;
	unsigned short usVal;
	unsigned short iPktLength;
	struct stBBAPacketInfo oPktInfo;

	elog_init(iArgCount, aArgList);

	/* Parse out command line options */
	if (parseCommandLineOptions(iArgCount, aArgList) == RESULT_SUCCESS) {

		/* Read in the parameter file */
		if (paramFileRead() == RESULT_FAILURE) {
			elog_complain(1,
					"main(): Error encountered during paramFileRead() operation.");
			dcbbaCleanup(-1);
		}

		/* Allocate memory for our packet */
		allot (unsigned char *, aBBAPkt, oConfig.iBBAPktBufSz);

		/* Set up a signal handler to re-read the parameter file on SIGUSR1*/
		signal(SIGUSR1, sig_hdlr);

		/* Connect to Data Concentrator's Data read port */
		if (dcDataConnect(oConfig.iConnectionType, oConfig.sDCConnectionParams)
				== RESULT_SUCCESS) {

			/*** BEGIN MAIN LOOP ***/
			while (bnsget(oDCDataBNS, &cIn, BYTES, 1) >= 0) {
				/*hexdump(stderr, &cIn, 1);*/
				switch (iReadState) { /* Waiting for sync character */
				case 0:
					iPktCounter = 0;
					if (cIn == BBA_SYNC) {
						aBBAPkt[iPktCounter++] = cIn;
						iReadState = 1;
					} else if (cIn == 0xAB || cIn == 0xBB) {
						elog_complain(
								0,
								"main(): state=0, Unsupported packet prefix encountered: %x\n",
								cIn, cIn);
					} else {
						elog_complain(
								0,
								"main(): state=0, discarding character '%c' = %x\n",
								cIn, cIn);
					}
					break;

				case 1:
					/* Copy packet type into packet */
					aBBAPkt[iPktCounter++] = cIn;

					switch (cIn) {
					case BBA_DAS_DATA:
					case BBA_DAS_STATUS:
					case BBA_DC_STATUS:
					case BBA_RTX_STATUS:
						iReadState = 4;
						break;

					default:
						iReadState = 0;
						elog_complain(
								0,
								"main: state=1, Unknown BBA Packet subtype - discarding character '%c' = %x\n",
								cIn, cIn);
						break;
					}
					break;
				case 4: /* Wait for PID and packet length in the header */
					aBBAPkt[iPktCounter++] = cIn;
					if (iPktCounter == BBA_CTRL_COUNT) {
						memcpy((char *) &usVal, &aBBAPkt[BBA_PSIZE_OFF], 2); /* packet size  */
						if (usVal == 0) {
							elog_complain(0,
									"Wrong header. Zero packet size detected.\n");
							hexdump(stderr, aBBAPkt, iPktCounter);
							iReadState = 0;
						} else {
							iPktLength = ntohs(usVal);
							iReadState = 5;
						}
					}
					break;

				case 5: /* We have a packet length */
					aBBAPkt[iPktCounter++] = cIn; /* Keep spooling packet from buffer */

					if (iPktCounter >= iPktLength) { /* packet complete */

						if (validateBBAChecksum(aBBAPkt, iPktLength)
								== RESULT_FAILURE) { /* Checksum's don't match */
							elog_complain(0,
									"discarding packet with bad checksum\n");
							hexdump(stderr, aBBAPkt, iPktLength);
						} else { /* Checksum's match, grab additional data from the packet */
							if (oConfig.bVerboseModeFlag)
								hexdump(stderr, aBBAPkt, iPktLength);

							if (parseBBAPacket(aBBAPkt, &oPktInfo)
									== RESULT_SUCCESS) {
								/* Put the packet into the orb */
							} else
								elog_complain(0,
										"parseBBAPacket was unable to parse the packet. Skipping.\n");
							/*
							 err = 0;
							 if ((err = valid_pkt(&newbuffer, &srcname[0],
							 &epoch, &psize, plength, hdrtype)) > 0) {
							 complain(0,
							 "read_socket(): Not valid packet. Wrong HEADER? \n");
							 } else {
							 cansend = 1;
							 if (fabs(epoch - prev_time) > 86400.0) {
							 prev_time = now();
							 if (fabs(epoch - prev_time) > 86400.0) {
							 sp = (ushort_t *) &newbuffer[0];
							 hdrsiz = ntohs(*sp);
							 memcpy((char *) &ysec, newbuffer
							 + hdrsiz + 10, 4);
							 complain(
							 0,
							 "%s packet has bad time - %s (epoch:%lf - ysec:%ld). Will discard packet.\n",
							 srcname, s = strtime(epoch),
							 epoch, ysec);
							 free(s);
							 if (Log)
							 hexdump(stderr, newbuffer + hdrsiz,
							 48);
							 cansend = 0;
							 } else
							 prev_time = epoch;
							 } else
							 prev_time = epoch;

							 if (ports->orb > 0 && cansend) {
							 if (orbput(ports->orb, &srcname[0], epoch,
							 (char *) newbuffer, psize) < 0) {
							 orbclose(ports->orb);
							 die(1,
							 "Can't send a packet to orbserver.\n");
							 }
							 }
							 }*/
						}
						iReadState = 0;
					}
					if (iPktCounter >= oConfig.iBBAPktBufSz) {
						complain(
								0,
								"attempted to accumulate %d byte packet: too large for internal buffer\n",
								iPktCounter);
						iReadState = 0;
					}
					break;

				}

			}
			elog_complain(0, "main(): bnsget failed to read");
			dcbbaCleanup(-1);
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
 * Reads packets from the Data Concentrator
 */
int readFromDC(struct stBBAPacketInfo *oPktInfo, unsigned char *aBBAPkt){
	return RESULT_FAILURE;
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
	oConfig.iBBAPktBufSz = DEFAULT_BBA_PKT_BUF_SZ;

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
	if (oDCDataBNS)
		bnsclose(oDCDataBNS);

	/* Exit */
	exit(iExitCode);
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
int getBBAStaFromSID(int iStaID, char *StaName) {
	char sbuf[5];
	void *result;
	/* Convert iStaID to a string so we can do the pf lookup */
	sprintf(sbuf, "%i", iStaID);
	result = getarr(oConfig.oSites, sbuf);
	if (result == NULL) {
		elog_complain(0,
				"getBBAStaFromSID: unable to find StaName for StaID %i", iStaID);
		return RESULT_FAILURE;
	}
	strcpy(StaName, result);
	return RESULT_SUCCESS;
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
int getBBADataTypeFromSRate(float fSampleRate, char *sDataType) {

	if (fSampleRate < 10) {
		strcpy(sDataType, "LS");
		return RESULT_SUCCESS;
	} else if ((fSampleRate >= 10) && (fSampleRate < 100)) {
		strcpy(sDataType, "BS");
		return RESULT_SUCCESS;
	} else if (fSampleRate >= 100) {
		strcpy(sDataType, "HS");
		return RESULT_SUCCESS;
	}

	/* We shouldn't ever get here unless iSampleRate is way out of range or NULL*/
	strcpy(sDataType, "");
	elog_complain(
			0,
			"getBBADataTypeFromSRate: Unable to determine DataType from given sample rate of %f",
			fSampleRate);
	return RESULT_FAILURE;
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
			iResult = dcDataConnectSocket(sConnectionParams[0],
					(in_port_t) atoi(sConnectionParams[1]));
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
 */
int dcDataConnectSocket(char *sHost, in_port_t iPort) {

	/* Initialize */
	unsigned long iNetAddress;
	int iConnectionResult = 1;
	int iFH; /*Temporary filehandle that gets used to create the Bns structure */
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
	if ((iFH = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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
	iConnectionResult = connect(iFH, (struct sockaddr *) &oAddress,
			sizeof(oAddress));
	if (iConnectionResult) {
		elog_complain(1, "dcDataConnectSocket(): Could not connect to socket");
		close(iFH);
		return RESULT_FAILURE;
	}

	/* Now that we've got our socket handle, make a Bns out of it */
	oDCDataBNS = bnsnew(iFH, DATA_RD_BUF_SZ);
	/* Since iFH is a socket, set up the BNS to use Socket read routines */
	bnsuse_sockio(oDCDataBNS);

	/* If we got this far, everything was successful */
	return RESULT_SUCCESS;

}

/*
 * Read a packet dump file as a simulation of the DC Data Port
 *
 * Packet dump is simply a file containing only the data fields from the TCP/IP packet stream as extracted by Wireshark
 */
int dcDataConnectFile(char *sFileName) {
	int iFH;
	/*  Attempt to open the file in Read-Only mode */
	iFH = open(sFileName, O_RDONLY);
	if (iFH < 0) {
		elog_complain(1,
				"dcDataConnectFile(): Unable to open dummy data file '%s'.",
				sFileName);
		return RESULT_FAILURE;
	}

	oDCDataBNS = bnsnew(iFH, DATA_RD_BUF_SZ);
	/* If we got here, return RESULT_SUCCESS */
	return RESULT_SUCCESS;
}

void sig_hdlr(int signo) {

	elog_notify(0, "sig_hdlr(): Got signal %d.\n", signo);
	elog_notify(0, "Re-reading parameter file to get new settings...");

	if (paramFileRead() == RESULT_FAILURE) {
		elog_complain(1,
				"sig_hdlr(): Error encountered during paramFileRead() operation:");
		dcbbaCleanup(-1);
	}

	elog_notify(0, "Done.\n");
	signal(SIGUSR1, sig_hdlr);
	return;
}

/*
 * Calculate a packet checksum and compare it to what is in the packet.
 * Return TRUE if the checksums match, FALSE if they don't
 */
int validateBBAChecksum(unsigned char *aBBAPkt, int iPktLength) {
	unsigned short usPktChksum, usCalcChksum; /* Holds the checksum values */
	unsigned short *usChksumPtr; /* Pointer for calculating the checksum */

	/* Go through the data portion of the packet one unsigned short at a time.
	 * The checksum is calculated on the entire packet assuming
	 * that the checksum byte fields are 0. Thus, we include the packet sync bytes from the beginning
	 * of the packet (0xDAAB or similar) but skip over the checksum bytes*/

	/* Initialize variables for checksum loop */
	usCalcChksum = 0;
	usChksumPtr = (unsigned short *) &aBBAPkt[0];

	/* Include the packet header bytes in usCalcChksum */
	usCalcChksum ^= ntohs(*usChksumPtr++);

	/* Extract the checksum from the packet, and skip it in the calculated checksum */
	usPktChksum = ntohs(*usChksumPtr++);

	/*
	 * We have now skipped forward enough that usChksumPtr should be pointing at the offset
	 * for the packet size, so we need to iterate through the remainder of the packet
	 */
	for (; (int) usChksumPtr < ((int) &aBBAPkt[0] + iPktLength); usChksumPtr++) {
		usCalcChksum ^= ntohs(*usChksumPtr);
	}

	if (usCalcChksum != usPktChksum) {
		elog_complain(0, "bad checksum  PCHK:%04X!=CHK:%04X\n", usPktChksum,
				usCalcChksum);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

/*
 * Parse a new format BBA packet and fill out a stBBAPacketInfo struct
 * returns RESULT_FAILURE if there's an error
 */
int parseBBAPacket(unsigned char *aBBAPkt, struct stBBAPacketInfo* oPktInfo) {
	/* Variables */
	unsigned short usVal;
	double dPTime, dYTime, dSec;
	int iYear, iDay, iHour, iMin;
	unsigned long ysec;
	char sTMPNameCmpt[PKT_TYPESIZE];
	char sGeneratedSrcName[500];

	/*
	 * Start extracting portions of the packet and putting the results into oPktInfo fields
	 */
	memcpy((char *) &usVal, &aBBAPkt[BBA_PSIZE_OFF], 2); /* packet size */
	if (usVal == 0) {
		elog_complain(0, "Wrong header. Zero packet size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iPktSize = ntohs(usVal);

	memcpy((char *) &usVal, &aBBAPkt[BBA_STAID_OFF], 2); /* sta ID */
	if (oPktInfo->iPktSize == 0) {
		elog_complain(0, "Wrong header. Zero packet size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iStaID = ntohs(usVal);

	memcpy((char *) &usVal, &aBBAPkt[BBA_NSAMP_OFF], 2); /* # of samples */
	if (usVal == 0) {
		elog_complain(0, "Wrong header. Zero number of samples detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iNSamp = ntohs(usVal);

	memcpy((char *) &usVal, &aBBAPkt[BBA_SRATE_OFF], 2); /* Sample rate */
	if (usVal == 0) {
		elog_complain(0, "Wrong header. Zero sample rate detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->fSrate = ntohs(usVal);

	memcpy((char *) &usVal, &aBBAPkt[BBA_HSIZE_OFF], 2); /* header size */
	if (usVal == 0) {
		elog_complain(0, "Wrong header. Zero header size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iHdrSize = ntohs(usVal);

	oPktInfo->iNChan = aBBAPkt[BBA_NCHAN_OFF]; /* Number of channels */
	if (oPktInfo->iNChan == 0) {
		elog_complain(0, "Wrong header. Zero number of channels detected.\n");
		return RESULT_FAILURE;
	}

	/* get data type */
	switch (aBBAPkt[BBA_DTYPE_OFF]) {
	case 0x0:
		strcpy(oPktInfo->sDataType, "s2");
		break;
	case 0x01:
		strcpy(oPktInfo->sDataType, "s4");
		break;
	case 0x02:
		strcpy(oPktInfo->sDataType, "t4");
	case 0x10:
	case 0x11:
	case 0x12:
		strcpy(oPktInfo->sDataType, "c0");
		break;
	default:
		elog_complain(0, "Can't recognize a data type %d(%02x)\n",
				aBBAPkt[BBA_DTYPE_OFF], aBBAPkt[BBA_DTYPE_OFF]);
		return RESULT_FAILURE;
	}

	/* Get packet time */
	dYTime = now();
	e2h(dYTime, &iYear, &iDay, &iHour, &iMin, &dSec);
	dPTime = epoch(iYear * 1000);

	memcpy((char *) &ysec, &aBBAPkt[BBA_TIME_OFF], 4);
	oPktInfo->dPktTime = dPTime + ntohl(ysec);

	/* Fill in the SrcName fields */
	strcpy(oPktInfo->oSrcname.src_net, oConfig.sNetworkName); /* Net */

	/* Sta */
	if (getBBAStaFromSID(oPktInfo->iStaID, sTMPNameCmpt) == RESULT_FAILURE) {
		elog_complain(0, "Lookup of Station Name Failed.\n");
		return RESULT_FAILURE;
	}
	strcpy(oPktInfo->oSrcname.src_sta, sTMPNameCmpt);
	strcpy(oPktInfo->oSrcname.src_chan, ""); /* Chan (always null for these packets) */
	strcpy(oPktInfo->oSrcname.src_loc, ""); /* Loc (always null for these packets) */
	strcpy(oPktInfo->oSrcname.src_suffix, "BBA"); /* Suffix */

	/* Subcode */
	switch (aBBAPkt[1]) {
	case BBA_DAS_DATA: /* Data Packets */
		if (getBBADataTypeFromSRate(oPktInfo->fSrate, sTMPNameCmpt)
				== RESULT_FAILURE) {
			elog_complain(0, "Lookup of Subcode from Sample Rate failed.\n");
			return RESULT_FAILURE;
		}
		strcpy(oPktInfo->oSrcname.src_subcode, sTMPNameCmpt);
		break;
	case BBA_DAS_STATUS: /* DAS Status Packets */
		strcpy(oPktInfo->oSrcname.src_subcode, "DAS");
		break;
	case BBA_DC_STATUS: /* DC Status Packets */
		strcpy(oPktInfo->oSrcname.src_subcode, "DC");
		break;
	case BBA_RTX_STATUS: /* RTX Status Packets */
		strcpy(oPktInfo->oSrcname.src_subcode, "RTX");
		break;
	default:
		elog_complain(0, "Can't recognize a data packet type - %d(%02x)\n",
				aBBAPkt[1], aBBAPkt[1]);
		return RESULT_FAILURE;
	}

	join_srcname(&oPktInfo->oSrcname, sGeneratedSrcName);
	if (oConfig.bVerboseModeFlag)
		printf("Sourcename evaluates to %s\n", sGeneratedSrcName);

	return RESULT_SUCCESS;
}
