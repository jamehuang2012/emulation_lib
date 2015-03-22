/*
 * Copyright (C) 2011, 2012 MapleLeaf Software, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /**************************************************************************

	CardReaderManager.c

	Creadit Card Reader Manager Application main processing

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>

#include "CardReaderManager.h"
#include "Connect.h"
#include "Disconnect.h"
#include "Command.h"
#include "Transmit.h"
#include "Download.h"
#include "DLE_EOT.h"


#include "config.h"
#include "cmgrapi.h"
#include "logger.h"
#ifndef bool
    #define bool unsigned char
    #define true 1
    #define false 0
#endif

/* The timeout interval to use for select() */
#define MAIN_SELECT_TIMEOUT_SECS 10


/* Allow a maximum of 2 connection at a time to the TCP/IP socket. */
#define MAXPENDING_CONNECTIONS 2


unsigned long		Port;
unsigned long		Baudrate;
extern bool bAcquireCardState;
extern bool bShutdown;
extern bool bThreadExiting;

unsigned int ClientCmdLen;


static bool doFork = true;
static bool createPIDFile = false;
static char *pidFile;
//extern ccr_cardMgrCmd_t cardMgrCmd;

extern pthread_cond_t cmd_cond;
extern pthread_cond_t icrw_cond;
extern pthread_mutex_t data_mutex;
extern pthread_mutex_t cmd_mutex;
extern pthread_mutex_t icrw_mutex ;
int RS232C_SetBaseDevName( char *pBaseDevName );

/*********************************************************
 * NAME: ReleaseResources()
 * DESCRIPTION: Closes any open handles, removes list
 *              entries. Unmounts mounted file system.
 *
 * IN: None.
 *
 * OUT: None.
 *********************************************************/
void ReleaseResources(void)
{
    bShutdown = true; // Kill backdown thread
    // Here we should wait for the thread to tell us it is on
    // it's way out.

    printf("exit .... \n");
    exit_ccr();
    while(!bThreadExiting)
    {
       sleep(1) ;
    }

    /*
    *   Here we should essentially have place the Card Reader
    *   into Standby state.  Which means blink the Green
    *   LED slowly.
    */



}

/*********************************************************
 * NAME: processSignals()
 * DESCRIPTION: Processes the HUP signal.  Reads
 * configuration file and applies any changes.
 *
 * IN: sig - The signal value.
 * OUT: Nothing.
 *********************************************************/
static void processSignals(int sig)
{
    switch (sig)
    {
        case SIGHUP:
            readConfig();
        break;

        case SIGINT:
        case SIGQUIT:
        case SIGUSR1:   /* Generated when svsd dies */
        case SIGTERM:
        {
            ReleaseResources();
            exit(0);
        }
        break;
    }
}

/*********************************************************
 * NAME: usage
 * DESCRIPTION: display command-line options
 * IN: name: program name
 * OUT: None
 *********************************************************/
static void usage(char *name)
{
    printf("Usage: %s [-d]\n", name);
    printf("\t-d: do not background\n");
    exit(1);
}



/*********************************************************
 * NAME:  processArgs
 * DESCRIPTION: process command-line arguments
 * IN: argc: argument count
 *     argv: argument list
 * OUT: None
 *********************************************************/
static void processArgs(int argc, char *argv[])
{
    int c = 0;

    while ((c = getopt(argc, argv, "dp:")) != -1)
    {
        switch (c)
        {
            case 'd':
                doFork = false;
            break;
            case 'p':
                createPIDFile = true;
                pidFile = optarg;
            break;
            default:
                usage(argv[0]);
            break;
        }
    }
}




// Parse the track data to extract and display the CC # and Exp/Date
int parseCCRTrackData(char *trackData)
{
    // Parse track data, if first byte is 'B' then we have track 1 data.
    char track_card_num[32];
    char track_exp_date[10];
    char *pWork1;
    char *pWork = (char *)trackData;

    memset(track_card_num, 0, sizeof(track_card_num));
    memset(track_exp_date, 0, sizeof(track_exp_date));

    if(*trackData == 'B')
    {
        // Have track 1 data
        ++pWork; // Points to PAN
        pWork1 = strchr(pWork, '^');
        if( pWork1 )
        {
            *pWork1 = 0;
            strcpy(track_card_num, pWork);
            *pWork1 = '^';
            pWork1++; // Points to Name
            pWork = strchr(pWork1, '^');
            if(pWork)
            {
                // Have terminator for end of name
                ++pWork; // Point to exp date Year
                track_exp_date[0] = *pWork++;
                track_exp_date[1] = *pWork++;
                track_exp_date[2] = '/';
                track_exp_date[3] = *pWork++;
                track_exp_date[4] = *pWork;
                track_exp_date[5] = 0;
                fprintf(stderr, "Scanned Card: %s \n"
                        "    Exp Date: %s (YY/MM)\n\n", track_card_num, track_exp_date);
            }
            else
            {
                // Possibly bad formmed track
                fprintf(stderr, "Possibly bad formed Track 1 Data.\n"
                        "Or non-credit card inserted.\n\n");
            }
        }
        else
        {
            // Possibly bad formmed track
            fprintf(stderr, "Possibly bad formed Track 2 Data.\n"
                    "Or non-credit card inserted.\n\n");
        }
    }
    else
    {
        // Should be track 2
        // Have track 1 data
        // pWork Points to PAN
        pWork1 = strchr(pWork, '=');
        if( pWork1 )
        {
            *pWork1 = 0;
            strcpy(track_card_num, pWork);
            *pWork1 = '=';
            pWork1++; // Points to Expire Date

            // Have terminator for end of name
            track_exp_date[0] = *pWork1++;
            track_exp_date[1] = *pWork1++;
            track_exp_date[2] = '/';
            track_exp_date[3] = *pWork1++;
            track_exp_date[4] = *pWork1;
            track_exp_date[5] = 0;
            fprintf(stderr, "Scanned Card: %s \n"
                    "    Exp Date: %s (YY/MM)\n\n", track_card_num, track_exp_date);
        }
        else
        {
            // Possibly bad formmed track
            fprintf(stderr, "Possibly bad formed Track 2 Data.\n"
                    "Or non-credit card inserted.\n\n");
        }
    }
    return 0;
}


// Parse the track status
void printTrackDataStatus(int status)
{
    fprintf(stderr, "No Data: ");
    switch(status)
    {
        case 20:
             fprintf(stderr, "Card data not read or data buffer is cleared\n");
             break;

        case 21:
             fprintf(stderr, "No start sentinel on track\n");
             break;

        case 22:
             fprintf(stderr, "Parity error on track\n");
             break;

        case 23:
             fprintf(stderr, "No End Sentinel or\n"
                        "More data than ISO specification\n");
             break;

        case 24:
             fprintf(stderr, "LRC error on track\n");
             break;

        case 25:
             fprintf(stderr, "No magnetic data on track\n");
             break;

        case 27:
             fprintf(stderr, "No data block, only Start/End Sentinel\n");
             break;
    }
    fprintf(stderr, "\n");
}

int ccrTrackDataCallback(uint8_t msg_id, uint8_t dev_num, uint8_t *payload, uint16_t len)
{
    ccr_trackData_t *pTrackData = (ccr_trackData_t *)payload;

    if (pTrackData->cardMgrResult.error)
    {
        fprintf(stderr, "Card Reader Error Code: %d\n", pTrackData->cardMgrResult.readerStatus);
    }
    else
    {
        fprintf(stderr, "Card Reader Status Code: %d\n", pTrackData->cardMgrResult.readerStatus);
    }

    fprintf(stderr, "Track 1 Status: %d Length: %d Data:\n", pTrackData->track1Status, pTrackData->track1DataLen);
    if(pTrackData->track1DataLen)
    {
        fprintf(stderr, "%s\n", pTrackData->track1Data);
        parseCCRTrackData((char *)pTrackData->track1Data);
    }
    else
    {
        printTrackDataStatus(pTrackData->track1Status);
    }

    fprintf(stderr, "Track 2 Status: %d Length: %d Data:\n", pTrackData->track2Status, pTrackData->track2DataLen);
    if(pTrackData->track2DataLen)
    {
      fprintf(stderr, "%s\n", pTrackData->track2Data);
      parseCCRTrackData((char *)pTrackData->track2Data);
    }
    else
    {
        printTrackDataStatus(pTrackData->track2Status);
    }

    fprintf(stderr, "Track 3 Status: %d Length: %d Data:\n", pTrackData->track3Status, pTrackData->track3DataLen);
    if(pTrackData->track3DataLen)
       {
         fprintf(stderr, "%s\n", pTrackData->track3Data);
       }
       else
       {
           printTrackDataStatus(pTrackData->track3Status);
       }
    return 0;
}

/*********************************************************
 * NAME:   main
 * DESCRIPTION: Main processing function, including main
 *              loop.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int main( int argc, char** argv )
{
    int	nRetVal;
    struct timeval selectTimeout; /* Used for select() timeout */


    processArgs(argc, argv);

    signal(SIGINT, processSignals);
    signal(SIGQUIT, processSignals);
    signal(SIGTERM, processSignals);
    signal(SIGHUP, processSignals);

    prctl(PR_SET_PDEATHSIG, SIGUSR1);
    
    atexit(ReleaseResources);
    printf("1\n");
    setDefaults();
    nRetVal = readConfig();
    if(nRetVal)
    {
        // Need to log this and remove printf()
        printf("Error processing configuration file...exiting\n");
        exit(1);
    }

    printf("2\n");
    updateConfiguration();
    RS232C_SetBaseDevName( theConfiguration.szSerialPortBaseName);
    ccr_callback_register(ccrTrackDataCallback);

    ccr_init();




   	for ( ;; ) {

   		start_ccr();
   		sleep(10);
   		stop_ccr();
   		sleep(10);
   	}

    ReleaseResources();
    return 0;
}
