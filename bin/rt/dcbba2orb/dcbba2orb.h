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
#define DEFAULT_RTDAS_DATA_PORT "5000"
#define FLUSH_WAIT              3   /* number of seconds to wait for data
                                       before flushing data file descriptor. 
                                       This will cause the program to wait 
                                       this many seconds even if no data 
                                       arrives. */

#define MAXDAVISDELAY             10 /* number of seconds to wait for
                                        an expected response before failing.
                                        if the response is sooner, we will exit
                                        before this time expires. */
#define RESULT_SUCCESS          0
#define RESULT_FAILURE          -1
#define INVALID_HANDLE          -1
#define min(a, b)               (a < b ? a : b)
#define strsame(s1, s2)          (!strcmp ((s1), (s2)))
#define FALSE                   0
#define TRUE                    1
#define MAX_SERIAL_TIME_MS      255
#define MAX_BUFFER_SIZE         5000


struct stConfigData {
  int   bVerboseModeFlag;
  char  *sNetworkName;
  char  *sOrbName;
  char  *sConnectionParams [2];
  char  *sParamFileName;
  char  *sStateFileName;
  char  *sDCHostName;   /* Hostname of the Data Concentrator */
  char  *sDCDataPort;   /* Port number to connect to for data */
};
#endif
