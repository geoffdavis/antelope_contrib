/******************************************************************************
 *
 * dcbba2orb/dcbba2orb.h
 *
 * INclude file of constatns, macors, function declarations, etc.
 *
 *
 * Author: Geoff Davis
 * UCSD, IGPP
 * gadavis@ucsd.edu
 ******************************************************************************/

#ifndef dcbba2orb_h_included
#define dcbba2orb_h_included

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "Pkt.h"
#include "orb.h"
#include "forb.h"
#include "stock.h"
/*
 * Constants
 */
#define CONNECT_SERIAL          1
#define CONNECT_TCP_IP          2
#define MAX_CONNECT_ATTEMPTS    10  /* Max tries to connect via serial/TCP */
#define DEFAULT_REPEAT_INTERVAL 3600
#define DEFAULT_SERIAL_SPEED    "19200"
#define DEFAULT_DC_DATA_PORT 	"5000"
#define DEFAULT_DC_CONTROL_PORT "5001"
#define RESULT_SUCCESS          0
#define RESULT_FAILURE          -1
#define INVALID_HANDLE          -1
#define min(a, b)               (a < b ? a : b)
#define strsame(s1, s2)          (!strcmp ((s1), (s2)))
#define FALSE                   0
#define TRUE                    1
#define MAX_BUFFER_SIZE         5000

struct stConfigData {
	int bVerboseModeFlag;
	char *sNetworkName;
	char *sOrbName;
	char *sDCAddress;
	char *sDCDataPort;
	char *sDCControlPort;
	char *sParamFileName;
	char *sStateFileName;
	Arr *aSites;
};
#endif
