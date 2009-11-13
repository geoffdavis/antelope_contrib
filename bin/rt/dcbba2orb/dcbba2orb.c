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
int addOrbHeaderToBBA(struct stBBAPacketInfo *PktInfo,
		unsigned char *BBAPktIn, char **OrbPktOut);
char *getBBAChNameFromId(unsigned short usBBAPktType, char *sSubCode,
		int iStaId, int iChId);
int initSiteLookupArrays(void);

/*
 * Main program loop
 */
int main(int iArgCount, char *aArgList[]) {

	unsigned char *aBBAPkt; /* Buffer to hold a bba packet read from the wire */
	struct stBBAPacketInfo oPktInfo; /* Struct to hold information from the packet header */
	double dPrevTime;
	char *sTimeStamp;
	int bOKToSend;
	char *sOutPkt; /* Packet to put onto the orb */
	int iOutPktLen; /* Length of sOutPkt */

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

		/* Connect to the ORB */
		if ((orbfd = orbopen(oConfig.sOrbName, "w&")) < 0) {
			elog_complain(1, "orbopen: unable to connect to ORB \"%s\".",
					oConfig.sOrbName);
			dcbbaCleanup(-1);
		}

		/* Connect to Data Concentrator's Data read port */
		if (dcDataConnect(oConfig.iConnectionType, oConfig.sDCConnectionParams)
				== RESULT_SUCCESS) {
			dPrevTime = now();

			/*** BEGIN MAIN LOOP ***/
			while (readFromDC(&oPktInfo, aBBAPkt) == RESULT_SUCCESS) {
				bOKToSend = TRUE;
				/* Check the packet age */
				/*if (fabs(oPktInfo.dPktTime - dPrevTime) > 86400.0) {
				 dPrevTime = now();
				 if (fabs(oPktInfo.dPktTime - dPrevTime) > 86400.0) {
				 elog_complain(
				 0,
				 "%s packet has bad time - %s (epoch:%lf). Will discard packet.\n",
				 oPktInfo.sSrcname, sTimeStamp = strtime(
				 oPktInfo.dPktTime), oPktInfo.dPktTime);
				 free(sTimeStamp);
				 bOKToSend = FALSE;
				 } else
				 dPrevTime = oPktInfo.dPktTime;
				 } else
				 dPrevTime = oPktInfo.dPktTime;*/

				if (bOKToSend == TRUE) {
					/* Add orb header to Packet */
					iOutPktLen = addOrbHeaderToBBA(&oPktInfo, aBBAPkt,
							&sOutPkt);
					if (iOutPktLen == 0) {
						/*whine*/
						elog_complain(
								1,
								"An error occurred while adding the ORB header to the raw packet. Not submitting to the orb.");
					} else if (sOutPkt == 0) {
						die(1,
								"Output packet length was non-zero but pointer to Output packet is null");
					} else {

						/*put it into the  orb, chief */
						if (oConfig.bVerboseModeFlag == TRUE) {
							showPkt(0, oPktInfo.sSrcname, oPktInfo.dPktTime,
									sOutPkt, iOutPktLen, stderr, PKT_UNSTUFF);
							showPkt(0, oPktInfo.sSrcname, oPktInfo.dPktTime,
									sOutPkt, iOutPktLen, stderr, PKT_DUMP);
						}

						if (orbput(orbfd, oPktInfo.sSrcname, oPktInfo.dPktTime,
								sOutPkt, iOutPktLen)) {
							elog_complain(0, "orbput() failed in main()\n");
							dcbbaCleanup(-1);
						}

						if (oConfig.bVerboseModeFlag == TRUE)
							elog_notify(0, "packet submitted under %s\n",
									oPktInfo.sSrcname);

						/* Free the packet */
						free(sOutPkt);
					}
				}
			}

			/*
			 * If we get here, it means readFromDC failed to get a packet from oDCDataBNS.
			 * This could be either that an EOF was reached if we were reading from a file,
			 * or that the socket died unexpectedly.
			 */
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
int readFromDC(struct stBBAPacketInfo *oPktInfo, unsigned char *aBBAPkt) {
#define ST_WAIT_FOR_SYNC 0
#define ST_READ_PKTTYPE 1
#define ST_READ_HEADER 4
#define ST_READ_BODY 5
	/* Declarations */
	int iPktCounter = 0;
	unsigned char cIn;
	int iReadState = ST_WAIT_FOR_SYNC;
	unsigned short usVal;
	unsigned short iPktLength;

	/* Keep reading from the BNS until we get an entire packet or EOF */
	while (bnsget(oDCDataBNS, &cIn, BYTES, 1) >= 0) {

		switch (iReadState) {
		case ST_WAIT_FOR_SYNC:
			/* Waiting for sync character */
			iPktCounter = 0;
			if (cIn == BBA_SYNC) {
				aBBAPkt[iPktCounter++] = cIn;
				iReadState = ST_READ_PKTTYPE;
			} else if (cIn == 0xAB || cIn == 0xBB) {
				/* ipd2 had handling routines for these packet prefixes,
				 * but underlying libdefunctpkt2 no longer parsed them */
				elog_complain(
						0,
						"main(): state=0, Unsupported packet prefix encountered: %x\n",
						cIn, cIn);
			} else {
				elog_complain(0,
						"main(): state=0, discarding character '%c' = %x\n",
						cIn, cIn);
			}
			break;

		case ST_READ_PKTTYPE:
			/* Copy packet type into packet */
			aBBAPkt[iPktCounter++] = cIn;

			switch (cIn) {
			case BBA_DAS_DATA:
			case BBA_DAS_STATUS:
			case BBA_DC_STATUS:
			case BBA_RTX_STATUS:
				iReadState = ST_READ_HEADER;
				break;

			default:
				iReadState = ST_WAIT_FOR_SYNC;
				elog_complain(
						0,
						"main: state=1, Unknown BBA Packet subtype - discarding character '%c' = %x\n",
						cIn, cIn);
				break;
			}
			break;
		case ST_READ_HEADER: /* Wait for packet length in the header */
			aBBAPkt[iPktCounter++] = cIn;
			if (iPktCounter == BBA_CTRL_COUNT) {
				usVal = 0;
				memcpy((char *) &usVal, &aBBAPkt[BBA_PSIZE_OFF], 2); /* packet size  */
				if (usVal == 0) {
					elog_complain(0,
							"Wrong header. Zero packet size detected.\n");
					hexdump(stderr, aBBAPkt, iPktCounter);
					iReadState = ST_WAIT_FOR_SYNC;
				} else {
					iPktLength = ntohs(usVal);
					iReadState = ST_READ_BODY;
				}
			}
			break;

		case ST_READ_BODY:
			aBBAPkt[iPktCounter++] = cIn; /* We have a packet length, so keep spooling packet from buffer */

			if (iPktCounter >= iPktLength) { /* packet complete */

				if (validateBBAChecksum(aBBAPkt, iPktLength) == RESULT_FAILURE) { /* Checksum's don't match */
					elog_complain(0, "discarding packet with bad checksum\n");
					hexdump(stderr, aBBAPkt, iPktLength);
				} else { /* Checksum's match, grab additional data from the packet */
					if (oConfig.bVerboseModeFlag)
						hexdump(stderr, aBBAPkt, iPktLength);

					if (parseBBAPacket(aBBAPkt, oPktInfo) == RESULT_SUCCESS) {
						/*
						 * If the parsing was successful, return here.
						 */
						return RESULT_SUCCESS;
					} else {
						/*
						 * Log an error and discard the packet
						 */
						elog_complain(0,
								"parseBBAPacket was unable to parse the packet. Skipping.\n");
					}
				}
				iReadState = ST_WAIT_FOR_SYNC;
			}
			if (iPktCounter >= oConfig.iBBAPktBufSz) {
				elog_complain(
						0,
						"attempted to accumulate %d byte packet: too large for internal buffer\n",
						iPktCounter);
				iReadState = ST_WAIT_FOR_SYNC;
			}
			break;

		}

	}
	/*
	 * If we get here, it means we encountered an unrecoverable error while reading the file handle/socket
	 */
	elog_log(0, "readFromDC(): bnsget failed to read");
	return RESULT_FAILURE;
}

/*
 * Adds an orbheader to a BBA Packet read from the DC using data in oPktInfo. Does not modify the raw BBA Packet.
 *
 * Parameters:
 * 	PktInfo
 * 		Information about the packet extracted by parseBBAPacket
 * 	BBAPktIn
 * 		The raw packet read from the wire
 * 	OrbPktOut
 * 		Pointer to an array containing the orb packet
 *
 * Returns:
 * 	The length of OrbPacketOut. This will be 0 if an error occurred, and OrbPktOut will be null.
 */
int addOrbHeaderToBBA(struct stBBAPacketInfo *PktInfo,
		unsigned char *BBAPktIn, char **OrbPktOut) {

	char *oNewPkt = 0;
	double calib;
	float fcalib, samprate;
	struct stBBAPreHdr oPreHdr;
	unsigned char hdr[512];
	unsigned char hdrbuf[512];
	unsigned char *phdr;
	char sta[PKT_NAMESIZE], chan[512];
	int nbytes, psize, chanlen;
	short datatype, nsamp, nchan, chlen, hdrsize, hsiz;

	/*
	 * Initialization
	 */
	nbytes = 0; /* Initialize the byte counter */
	phdr = &hdrbuf[0]; /* Initialize header pointer */
	memset((char *) chan, 0, 64); /* Zero out the first 64 bytes of chan */

	/*
	 * Populate the pre-header struct
	 * The fourth field, hdrsiz, is generated below after we assemble the main header
	 */
	oPreHdr.pkttype = (unsigned short) htons (PktInfo->usRawPktType);
	oPreHdr.hdrtype = (unsigned short) htons (0xBBA); /* Fixed to BBA format */
	oPreHdr.pktsiz = htons(PktInfo->iPktSize);

	/*
	 * Prepare to assemble the header
	 * Retrieve channel information, station name, and calibration info
	 */
	nchan = PktInfo->iNChan;
	strcpy(chan, PktInfo->sChNames);
	chanlen = strlen(chan);
	strcpy(sta, PktInfo->oSrcname.src_sta);
	calib = 0; /* Always null for BBA packets, each channel has a cal factor instead */

	/*
	 * Start assembling the main header
	 */

	/* Put the calibration value into hdr */
	fcalib = (float) calib;
	htonfp(&fcalib, &fcalib);
	memcpy(phdr, &fcalib, sizeof(float));
	nbytes += sizeof(float);
	phdr += sizeof(float);

	/* Put the sample rate into the header */
	htonfp(&PktInfo->fSrate, &samprate);
	memcpy(phdr, &samprate, sizeof(float));
	nbytes += sizeof(float);
	phdr += sizeof(float);

	/* Get the Trace code (from tr.h) for the data type */
	if (strncmp(PktInfo->sDataType, "s2", 2) == 0) {
		datatype = (short) trSHORT;
	} else if (strncmp(PktInfo->sDataType, "s4", 2) == 0) {
		datatype = (short) trINT;
	} else if (strncmp(PktInfo->sDataType, "c0", 2) == 0) {
		datatype = 0;
	}
	/* Put the data type into the header */
	datatype = htons(datatype);
	memcpy(phdr, &datatype, sizeof(short));
	nbytes += sizeof(short);
	phdr += sizeof(short);

	/* Put the number of samples into the header */
	nsamp = (short) htons(PktInfo->iNSamp);
	memcpy(phdr, &nsamp, sizeof(short));
	nbytes += sizeof(short);
	phdr += sizeof(short);

	/* Put the number of channels into the header */
	nchan = (short) htons(PktInfo->iNChan);
	memcpy(phdr, &nchan, sizeof(short));
	nbytes += sizeof(short);
	phdr += sizeof(short);

	/* Put the BBA header size into the header */
	hdrsize = (short) htons(PktInfo->iHdrSize);
	memcpy(phdr, &hdrsize, sizeof(short));
	nbytes += sizeof(short);
	phdr += sizeof(short);

	/* Put the length of the channel names into the header */
	chlen = htons( (short) chanlen );
	memcpy(phdr, &chlen, sizeof(short));
	nbytes += sizeof(short);
	phdr += sizeof(short);

	/* Put the channel names into the header */
	memcpy(phdr, chan, chanlen);
	nbytes += chanlen;
	phdr += chanlen;

	/* Finalize the Pre Header */
	hsiz = nbytes + sizeof(struct stBBAPreHdr);
	psize = hsiz + ntohs(oPreHdr.pktsiz);
	oPreHdr.hdrsiz = htons (hsiz);

	/* Copy the pre header and the generated hdrbuf into hdr */
	memcpy(&hdr[0], (char *) &oPreHdr, sizeof(struct stBBAPreHdr));
	memcpy(&hdr[sizeof(struct stBBAPreHdr)], hdrbuf, nbytes);

	/*
	 * Build the ORB data packet
	 */
	allot( char *, oNewPkt, psize );
	memcpy(oNewPkt, hdr, hsiz); /* Add the entire header, including the PreHdr */
	memcpy(oNewPkt + hsiz, (char *) BBAPktIn, ntohs(oPreHdr.pktsiz)); /* Add data */

	/*
	 * If we get this far, the packet has been successfully generated
	 *
	 * Return the pointer to the newly generated packet in OrbPktOut
	 * Finally, return the size of the packet in psize
	 */
	*OrbPktOut = oNewPkt;
	return psize;
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
	oConfig.sParamFileName = DEFAULT_PARAM_FILE;
	oConfig.sStateFileName = NULL;
	oConfig.oSiteTbl = NULL;
	oConfig.oStaName = NULL;
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

	/* Close the orb connection */
	if (orbfd)
		orbclose(orbfd);
	/* Exit */
	exit(iExitCode);
}

/*
 * Initialize Station-Name-by-SID and Station-Channel Lookup Arrays
 * Helper function for paramFileRead
 */
int initSiteLookupArrays() {
	int iNumRows = 0, iRowIdx = 0; /* Used below while iterating through the Site Table */
	char *sRow = 0; /* Contains a row read from the Site Table */
	char sKey[32];
	struct stSiteEntry oSiteEntry, *oNewSiteEntry = 0;

	/* Initialize the Station Name lookup array */
	iNumRows = maxtbl(oConfig.oSiteTbl);

	if (iNumRows > 0) {
		if (oConfig.oStaName != NULL) {
			freearr(oConfig.oStaName, 0);
		}
		if (oConfig.oStaCh != NULL) {
			freearr(oConfig.oStaName, 0);
		}
		oConfig.oStaName = newarr(0);
		oConfig.oStaCh = newarr(0);
	} else {
		elog_log(0, "Can't get site parameters. Check parameter file!\n");
		return RESULT_FAILURE;
	}

	for (iRowIdx = 0; iRowIdx < iNumRows; iRowIdx++) {
		/* Read a row from the site table */
		sRow = gettbl(oConfig.oSiteTbl, iRowIdx);
		/* Parse it into it's component entries */
		sscanf(sRow, STE_SCS, STE_RVL(&oSiteEntry));
		free(sRow);

		/* Handle the StaName table */
		sprintf(sKey, "%d", oSiteEntry.iSID);

		if (getarr(oConfig.oStaName,sKey) == 0) {
			/* If we haven't seen this key before, add an entry in the array */
			setarr(oConfig.oStaName, sKey, strdup(oSiteEntry.sNAME));
		}

		/* Handle the StaCh table */
		sprintf(sKey, "%s_%d_%d", oSiteEntry.sDTYPE, oSiteEntry.iSID, oSiteEntry.iCOMP);

		if ( (struct stSiteEntry *) getarr( oConfig.oStaCh, sKey ) == 0 ) {
			allot( struct stSiteEntry *, oNewSiteEntry, sizeof(struct stSiteEntry));
			memcpy( oNewSiteEntry, &oSiteEntry, sizeof (struct stSiteEntry));
			setarr(oConfig.oStaCh, sKey, oNewSiteEntry);
		}
	}
	return RESULT_SUCCESS;
}

/*
 * Reads the parameter file and initializes several lookup tables.
 */
int paramFileRead(void) {
	int ret; /* Return value from pfupdate */
	static int iPFFirstRead = 1; /* Tracks whether or not this is the first read of the parameter file */

	/* Read the parameter file */
	if ((ret = pfupdate(oConfig.sParamFileName, &oConfig.oConfigPf)) < 0) {
		/* An error occurred reading the parameter file */
		elog_log(
				0,
				"pfupdate(\"%s\", oConfig.oConfigPf): failed to open config file.\n",
				oConfig.sParamFileName);
		return RESULT_FAILURE;
	} else if (ret == 1) {
		/* We were able to successfully read the parameter file */

		/* Notify that we've read our config file */
		if (iPFFirstRead)
			elog_notify(0, "config file loaded %s\n", oConfig.sParamFileName);
		else
			elog_notify(0, "updated config file loaded %s\n",
					oConfig.sParamFileName);

		/* Read in the Network Name from the parameter file */
		oConfig.sNetworkName = pfget_string(oConfig.oConfigPf, "Network_Name");
		if (oConfig.bVerboseModeFlag == TRUE)
			elog_notify(0, "Network Name set to %s", oConfig.sNetworkName);

		/* Read in the Site Table from the parameter file */
		if (oConfig.oSiteTbl)
			freetbl(oConfig.oSiteTbl, 0);
		oConfig.oSiteTbl = pfget_tbl(oConfig.oConfigPf, "Site");

		/* Initialize the Site lookup arrays */
		if (initSiteLookupArrays() == RESULT_FAILURE)
			return RESULT_FAILURE;

		/* Read in the Das_Stat array */
		if (oConfig.oDasID)
			freearr(oConfig.oDasID, 0);
		oConfig.oDasID = pfget_arr(oConfig.oConfigPf, "Das_Stat");

		/* Read in the DC_Stat array */
		if (oConfig.oDcID)
			freearr(oConfig.oDcID, 0);
		oConfig.oDcID = pfget_arr(oConfig.oConfigPf, "DC_Stat");

		/* Read in the RTX_Stat array */
		if (oConfig.oRTXID)
			freearr(oConfig.oRTXID, 0);
		oConfig.oRTXID = pfget_arr(oConfig.oConfigPf, "RTX_Stat");

		/* Next read will no longer be the first */
		iPFFirstRead = 0;
	} /* pfudate returns 0 if nothing changed */

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
	result = getarr(oConfig.oStaName, sbuf);
	if (result == NULL) {
		elog_log(0, "getBBAStaFromSID: unable to find StaName for StaID %i",
				iStaID);
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
	elog_log(
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
		elog_log(0, "dcDataConnect(): Bad connection parameters.   "
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
		elog_log(
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

	/* Check for host already being a numeric IP address */
	if ((iNetAddress = inet_addr(sHost)) != INADDR_NONE)
		memcpy(&oAddress.sin_addr, &iNetAddress, min (sizeof (iNetAddress), sizeof(oAddress.sin_addr)));

	/* Else resolve name to IP address */
	else {
		oHostEnt = gethostbyname(sHost);
		if (oHostEnt == NULL) {
			elog_log(1,
					"dcDataConnectSocket(): Could not resovle address '%s'.",
					sHost);
			return RESULT_FAILURE;
		}
		memcpy(&oAddress.sin_addr, oHostEnt -> h_addr, min (oHostEnt -> h_length, sizeof (oAddress.sin_addr)));
		free(oHostEnt);
	}

	/* Create socket */
	if ((iFH = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		elog_log(1, "dcDataConnectSocket(): Could not create socket.");
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
		elog_log(1,
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
	unsigned short *pBBAPktChkSum; /* Pointer for calculating the checksum */

	/* Go through the data portion of the packet one unsigned short at a time.
	 * The checksum is calculated on the entire packet assuming
	 * that the checksum byte fields are 0. Thus, we include the packet sync bytes from the beginning
	 * of the packet (0xDAAB or similar) but skip over the checksum bytes*/

	/* Initialize variables for checksum loop */
	usCalcChksum = 0;
	pBBAPktChkSum = (unsigned short *) &aBBAPkt[0];

	/* Include the packet header bytes in usCalcChksum */
	usCalcChksum ^= ntohs(*pBBAPktChkSum++);

	/* Extract the checksum from the packet, and skip it in the calculated checksum */
	usPktChksum = ntohs(*pBBAPktChkSum++);

	/*
	 * We have now skipped forward enough that usChksumPtr should be pointing at the offset
	 * for the packet size, so we need to iterate through the remainder of the packet
	 */
	for (; (int) pBBAPktChkSum < ((int) &aBBAPkt[0] + iPktLength); pBBAPktChkSum++) {
		usCalcChksum ^= ntohs(*pBBAPktChkSum);
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
 *
 * Parameters:
 * 	aBBAPkt
 * 		The raw packet as read off the wire
 * 	oPktInfo
 * 		A pointer to a pre-allocated struct stBBAPacketInfo which will hold the extracted packet info
 *
 * Returns:
 * 	RESULT_FAILURE if there's an error
 */
int parseBBAPacket(unsigned char *aBBAPkt, struct stBBAPacketInfo* oPktInfo) {
	/* Variables */
	unsigned short usVal;
	double dPTime, dYTime, dSec;
	int iYear, iDay, iHour, iMin;
	unsigned long ysec;
	char sTMPNameCmpt[PKT_TYPESIZE];
	char *sChName, aChan[128];
	int iChId, iChBytes, iChIdx, iDataOffset, i;
	unsigned short usBBAPktType;

	/*
	 * Start extracting portions of the packet and putting the results into oPktInfo fields
	 */

	usBBAPktType = aBBAPkt[1];

	usVal = 0;
	memcpy((char *) &usVal, &aBBAPkt[BBA_PSIZE_OFF], 2); /* packet size */
	if (usVal == 0) {
		elog_log(0,
				"parseBBAPacket(): Wrong header. Zero packet size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iPktSize = ntohs(usVal);

	usVal = 0;
	memcpy((char *) &usVal, &aBBAPkt[BBA_STAID_OFF], 2); /* sta ID */
	if (oPktInfo->iPktSize == 0) {
		elog_log(0,
				"parseBBAPacket(): Wrong header. Zero packet size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iStaID = ntohs(usVal);
	usVal = 0;
	memcpy((char *) &usVal, &aBBAPkt[BBA_NSAMP_OFF], 2); /* # of samples */
	if (usVal == 0) {
		elog_log(0,
				"parseBBAPacket(): Wrong header. Zero number of samples detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iNSamp = ntohs(usVal);

	usVal = 0;
	memcpy((char *) &usVal, &aBBAPkt[BBA_SRATE_OFF], 2); /* Sample rate */
	if (usVal == 0) {
		elog_log(0,
				"parseBBAPacket(): Wrong header. Zero sample rate detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->fSrate = ntohs(usVal);

	usVal = 0;
	memcpy((char *) &usVal, &aBBAPkt[BBA_HSIZE_OFF], 2); /* header size */
	if (usVal == 0) {
		elog_log(
				0,
				"parseBBAPacket(): parseBBAPacket(): Wrong header. Zero header size detected.\n");
		return RESULT_FAILURE;
	} else
		oPktInfo->iHdrSize = ntohs(usVal);

	oPktInfo->iNChan = aBBAPkt[BBA_NCHAN_OFF]; /* Number of channels */
	if (oPktInfo->iNChan == 0) {
		elog_log(0,
				"parseBBAPacket(): Wrong header. Zero number of channels detected.\n");
		return RESULT_FAILURE;
	}

	/* Put raw packet type into oPktInfo */
	oPktInfo->usRawPktType = (aBBAPkt[0] * 256) + aBBAPkt[1];

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
		elog_log(0, "parseBBAPacket(): Can't recognize a data type %d(%02x)\n",
				aBBAPkt[BBA_DTYPE_OFF], aBBAPkt[BBA_DTYPE_OFF]);
		return RESULT_FAILURE;
	}

	/* Get packet time */
	dYTime = now();
	e2h(dYTime, &iYear, &iDay, &iHour, &iMin, &dSec);
	dPTime = epoch(iYear * 1000);

	ysec = 0;
	memcpy((char *) &ysec, &aBBAPkt[BBA_TIME_OFF], 4);
	oPktInfo->dPktTime = dPTime + ntohl(ysec);

	/*
	 * Fill in the SrcName fields
	 */

	/* Subcode and Station Name */
	switch (aBBAPkt[1]) {

	case BBA_DAS_DATA: /* Data Packets */

		/* Get the subcode from the sample rate */
		if (getBBADataTypeFromSRate(oPktInfo->fSrate, sTMPNameCmpt)
				== RESULT_FAILURE) {
			elog_log(0,
					"parseBBAPacket(): Lookup of Subcode from Sample Rate failed.\n");
			return RESULT_FAILURE;
		}
		strcpy(oPktInfo->oSrcname.src_subcode, sTMPNameCmpt);

		/* Get the station name, reusing sTMPNameCmpt */
		if (getBBAStaFromSID(oPktInfo->iStaID, sTMPNameCmpt) == RESULT_FAILURE) {
			elog_log(0, "parseBBAPacket(): Lookup of Station Name Failed.\n");
			return RESULT_FAILURE;
		}
		break;

	case BBA_DAS_STATUS: /* DAS Status Packets */

		strcpy(oPktInfo->oSrcname.src_subcode, "DAS");

		/* Get the station name */
		if (getBBAStaFromSID(oPktInfo->iStaID, sTMPNameCmpt) == RESULT_FAILURE) {
			elog_log(0, "parseBBAPacket(): Lookup of Station Name Failed.\n");
			return RESULT_FAILURE;
		}
		break;

	case BBA_DC_STATUS: /* DC Status Packets */

		strcpy(oPktInfo->oSrcname.src_subcode, "DC");

		/* Get the station name
		 * For whatever reason, the original ipd2 uses the raw Station ID field
		 * rather than doing a station name lookup
		 */
		sprintf(sTMPNameCmpt, "%d", oPktInfo->iStaID);
		break;

	case BBA_RTX_STATUS: /* RTX Status Packets */

		/* Get the station name */
		strcpy(oPktInfo->oSrcname.src_subcode, "RTX");
		if (getBBAStaFromSID(oPktInfo->iStaID, sTMPNameCmpt) == RESULT_FAILURE) {
			elog_log(0, "Lookup of Station Name Failed.\n");
			return RESULT_FAILURE;
		}
		break;
	default:
		elog_log(0, "Can't recognize a data packet type - %d(%02x)\n",
				aBBAPkt[1], aBBAPkt[1]);
		return RESULT_FAILURE;
	}

	strcpy(oPktInfo->oSrcname.src_net, oConfig.sNetworkName); /* Net */
	strcpy(oPktInfo->oSrcname.src_sta, sTMPNameCmpt); /* Sta */
	strcpy(oPktInfo->oSrcname.src_chan, ""); /* Chan (always null for these packets) */
	strcpy(oPktInfo->oSrcname.src_loc, ""); /* Loc (always null for these packets) */
	strcpy(oPktInfo->oSrcname.src_suffix, "BBA"); /* Suffix */

	join_srcname(&oPktInfo->oSrcname, oPktInfo->sSrcname);
	if (oConfig.bVerboseModeFlag)
		elog_debug(1, "parseBBAPacket(): Sourcename evaluates to %s\n",
				oPktInfo->sSrcname);

	/*
	 * Extract channel information
	 *
	 * chan = aChan
	 * chid = iChId
	 * chbytes = iChBytes
	 * chname = sChName
	 * len = iChIdx;
	 * doff=iDataOffset
	 */
	memset(aChan, 0, 128);
	memcpy(aChan, "_", strlen("_"));

	iDataOffset = oPktInfo->iHdrSize;
	for (i = 0, iChIdx = 0; i < oPktInfo->iNChan; i++) {
		//usVal = 0;
		//memcpy((char *) &usVal, &aBBAPkt[iDataOffset], 1); /* header size */
		//iChId = ntohs(usVal);
		iChId = aBBAPkt[iDataOffset];

		usVal = 0;
		memcpy((char *) &usVal, &aBBAPkt[iDataOffset + BBA_CHBYTES_OFF], 2);
		iChBytes = ntohs(usVal);

		sChName = getBBAChNameFromId(usBBAPktType,
				oPktInfo->oSrcname.src_subcode, oPktInfo->iStaID, iChId);
		if (sChName == 0) {
			elog_complain(0,
					"parseBBAPacket(): an error occured retrieving the channel name");
			return RESULT_FAILURE;
		}
		strcat(aChan, sChName);
		iChIdx += strlen(sChName);
		free(sChName);
		strcat(aChan, "_");
		iChIdx++;
		iDataOffset += BBA_CHHDR_SIZE + iChBytes;
	}
	aChan[iChIdx] = '\0';
	strcpy(oPktInfo->sChNames, aChan);

	/* If we got this far, everything parsed and looked up ok */
	return RESULT_SUCCESS;
}

/*
 * Look up channel names based on the SubCode, Station Id, and Channel Id
 *
 * Parameters:
 * 	usBBAPktType
 * 		one of: BBA_DAS_DATA, BBA_DAS_STATUS, BBA_DC_STATUS, BBA_RTX_STATUS
 * 	sSubCode
 * 		the translation of the BBAPktType to a string, usually the str_subcode
 * 		part of a Srcname structure
 * 	iStaId
 * 		the station Id (SID) field from the Site table
 * 	iChId
 * 		the Channel Id (COMP) field from the Site table
 *
 * Returns:
 * A pointer to a string containing the channel name. If usBBAPktType is
 * BBA_DAS_DATA and their is no corresponding entry in the Site table for the
 * combination of sSubCode, iStaId, and iChId, a dummy channel name is
 * generated based on sSubCode and iChId. For all other packet types, a null
 * pointer is returned if the lookup fails.
 */
char *getBBAChNameFromId(unsigned short usBBAPktType, char *sSubCode,
		int iStaId, int iChId) {
	char key[64], *name = 0, *cherr = 0;
	struct stSiteEntry *oSiteEntry;
	static Arr *oErrMsg;

	switch (usBBAPktType) {

	case BBA_DAS_DATA:

		sprintf(key, "BBA/%s_%d_%d", sSubCode, iStaId, iChId);
		if ((oSiteEntry = (struct stSiteEntry *) getarr(oConfig.oStaCh, key))
				!= 0) {
			name = oSiteEntry->sSENS;
		} else {
			/* We can't find an entry so we'll make up a fake channel Id */
			sprintf(&key[0], "%s_%d", sSubCode, iChId);
			name = key;

			/* To prevent excessive noise in the logs, only complain
			 * if we haven't tried looking this up before */
			if (oErrMsg == 0)
				oErrMsg = newarr(0);
			cherr = (char *) getarr(oErrMsg, key);
			if (cherr == 0) {
				elog_complain(
						0,
						"getBBAChNameFromId: can't get channel name (SubCode=%s StaID=%d, ChId=%d)\n",
						sSubCode, iStaId, iChId);
				cherr = strdup(key);
				setarr(oErrMsg, key, cherr);
			}
		}
		break;

	case BBA_DAS_STATUS:

		sprintf(key, "%d", iChId);
		if ((name = getarr(oConfig.oDasID, key)) == 0) {
			elog_log(
					0,
					"getBBAChNameFromId: can't get DAS parameter name (StaID=%d, ChId=%d)\n",
					iStaId, iChId);
		}
		break;

	case BBA_DC_STATUS:

		sprintf(key, "%d", iChId);
		if ((name = getarr(oConfig.oDcID, key)) == 0) {
			elog_log(
					0,
					"getBBAChNameFromId: can't get DC parameter name (StaID=%d, ChId=%d)\n",
					iStaId, iChId);
		}
		break;

	case BBA_RTX_STATUS:

		sprintf(key, "%d", iChId);
		if ((name = getarr(oConfig.oRTXID, key)) == 0) {
			elog_log(
					0,
					"getBBAChNameFromId: can't get RTX parameter name (StaID=%d, ChId=%d)\n",
					iStaId, iChId);
		}
		break;
	}

	if (name)
		return (strdup(name));
	else
		return (0);
}
