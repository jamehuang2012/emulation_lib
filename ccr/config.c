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

/*
*   config.c
*
*   Author: Burt Bicksler bbicksler@mlsw.com
*
*   Description: Provides support for
*   configuration file processing.
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <netinet/in.h>
#include <dirent.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#include "config.h"

struct Configuration theConfiguration;  // Working configuration
static struct Configuration theNewConfiguration; // Configuration read from config file.

/*********************************************************
*   NAME: setDefaults
*   DESCRIPTION: Sets up default configuration values.
*                For the case of no configuration file.
*   IN: Nothing.
*   OUT:  Nothing.
*
**********************************************************/
void setDefaults( void )
{
    theConfiguration.bDevelopmentReader = 1;
    theConfiguration.nLogLevel = DFLT_LOGLEVEL;
    theConfiguration.bDisplayDebugMsgs = DFLT_DISP_DEBUG;
    theConfiguration.nTCPServerPort = DFLT_TCPSERVER_PORT;

    strcpy( theConfiguration.szSerialPortNumber, DFLT_SERIALPORT_BASE_NUMBER);
    strcpy( theConfiguration.szSerialPortBaseName, DFLT_SERIALPORT_BASE_NAME);

    theNewConfiguration = theConfiguration;
}

/*********************************************************
*   NAME: updateConfiguration
*   DESCRIPTION: Copies the new configuration to the
*                working configuration.
*   IN: Nothing.
*   OUT: Nothing.
*
**********************************************************/
void updateConfiguration( void )
{
    theConfiguration = theNewConfiguration;
}

/*********************************************************
*   NAME: getConfigValue
*   DESCRIPTION: Very crude extraction of config value.
*
*   IN: pLine pointer to buffer.
*   OUT:    Pointer to byte after white space, or NULL.
*
**********************************************************/
char * getConfigValue( char *pLine )
{
    // First need to move to first white space
    while ( *pLine && ( *pLine != ' ' && *pLine != '\t' ) )
    {
        ++pLine;
    }

    // pLine points to first white space
    while ( *pLine && ( *pLine == ' ' || *pLine == '\t' ) )
    {
        ++pLine; // Skip whilte space.
    }

    // pLine should point to start of value in a properly formed config line
    return pLine;
}

/*********************************************************
*   NAME: processConfiguration
*   DESCRIPTION: Reads the simple configuration file
*                parses settings and applies values.
*   IN: fp pointer to open file.
*   OUT:    SUCCESS (0) or error code.
*
**********************************************************/
int processConfiguration( FILE *fp )
{
    char szBuffer[256];
    char *pLineBuff, *pWorkPtr, *pValue;
    char *pCh ;

    pLineBuff = "#\n";
    while ( pLineBuff )
    {
        pLineBuff = fgets( szBuffer, sizeof( szBuffer ) - 1, fp );

        if ( !pLineBuff )
        {
            break;
        }

        if ( *pLineBuff == '#' )
        {
            continue; // Skip comment lines
        }

        // Process the line take out ending newline.
        pCh = strchr( pLineBuff, '\n' );
        if ( pCh )
        {
            *pCh = 0;
        }

        pWorkPtr = strstr( pLineBuff, CFG_SERIALPORT_BASE_NAME );
        if ( pWorkPtr )
        {
            pValue = getConfigValue( pWorkPtr );
            if ( !pValue )
            {
                printf(
                          "%s Returning error. Failed to parse SerialPortBaseName\n",
                          __func__ );
                return -1;
            }

            // Set value in new config
            strcpy( theNewConfiguration.szSerialPortBaseName, pValue );
            continue; // Next line if any.
        }


        pWorkPtr = strstr( pLineBuff, CFG_SERIALPORT_NUMBER );
        if ( pWorkPtr )
        {
            pValue = getConfigValue( pWorkPtr );
            if ( !pValue )
            {
                printf(DFLT_SERIALPORT_BASE_NAME
                          "%s Returning error. Failed to parse SerialPortNumber\n",
                          __func__ );
                return -1;
            }

            // Set value in new config
            strcpy( theNewConfiguration.szSerialPortNumber, pValue );
            continue; // Next line if any.
        }

        pWorkPtr = strstr( pLineBuff, CFG_DISP_DEBUG );
        if ( pWorkPtr )
        {
            pValue = getConfigValue( pWorkPtr );
            if ( !pValue )
            {
                printf(
                          "%s Returning error. Failed to parse DisplayDebug\n",
                          __func__ );
                return -1;
            }

            // Set value in new config
            theNewConfiguration.bDisplayDebugMsgs = atoi(pValue);
            continue; // Next line if any.
        }

        pWorkPtr = strstr( pLineBuff, CFG_SERVER_PORT );
        if ( pWorkPtr )
        {
            pValue = getConfigValue( pWorkPtr );
            if ( !pValue )
            {
                printf(
                          "%s Returning error. Failed to parse TcpServerPort\n",
                          __func__ );
                return -1;
            }

            // Set value in new config
            theNewConfiguration.nTCPServerPort = atoi(pValue);
            continue; // Next line if any.
        }

        pWorkPtr = strstr( pLineBuff, CFG_DEVELOPMENT_READER );
        if( pWorkPtr )
        {
            pValue = getConfigValue( pWorkPtr );
            if ( !pValue )
            {
                printf(
                          "%s Returning error. Failed to parse CommDelay\n", __func__ );
                return -1;
            }
            // Set value in new config
            theNewConfiguration.bDevelopmentReader = atoi( pValue );
            continue; // Next line if any.
        }
    }

    return SUCCESS;
}


/*********************************************************
*   NAME: readConfig
*   DESCRIPTION: Opens the simple configuration file
*                then parses settings and applies values.
*   IN: Nothing.
*   OUT:    SUCCESS (0) or error code.
*
**********************************************************/
int readConfig( void )
{
    FILE *fp;
    int nRetVal = SUCCESS;

    // Always start with defaults filled in
    setDefaults();
    fp = fopen( CONFIG_FILE, "r" );
    if( fp == NULL )
    {
        printf( "%s No config file, using defaults.\n", __func__ );
        return nRetVal;
    }

    // Have the config file so process it.
    nRetVal = processConfiguration( fp );

    fclose( fp );
    return nRetVal;
}

