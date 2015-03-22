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
 * upgradedatadefs.h
 *
 * Author: Burt Bicksler bbicksler@mlsw.biz
 *
 * Description: Definitions for update manager daemon
 *
 */

#ifndef __UPDATE_DATADEF_H__
#define __UPDATE_DATADEF_H__

#define UPGRADEMANAGER_IP "127.0.0.1"
#define UPGRADEMANAGER_PORT 3379

/* Upgrade Manager Message Structure, Command and Error Code Definitions */
#define UPGRADE_PROTOCOL_VERSION 1
typedef enum
{
    SWUPDATE_SUCCESS = 0,
    ERR_SWUPDATE_CRC_FAILURE,
    ERR_SWUPDATE_NO_TMPSPACE,
    ERR_SWUPDATE_NO_DISK_SPACE,
    ERR_SWUPDATE_CURRVERS_ALREADY,
    ERR_SWUPDATE_EXTRACTION,
    ERR_SWUPDATE_DECRYPTION,
    ERR_SWUPDATE_NO_VALID_VERSION,
    ERR_SWUPDATE_CANNOT_CREATE_SCRIPT,
    ERR_SWUPDATE_GET_CURRVERS,
    ERR_SWUPDATE_NO_IC_PROP,
    ERR_SWUPDATE_NO_UPGFILE_TAG,
    ERR_SWUPDATE_GET_PKGNAME,
    ERR_SWUPDATE_ERRPRINT,
    ERR_SWUPDATE_INVALIDPARAM,
    ERR_SWUPDATE_INFOREAD,
    ERR_SWUPDATE_NO_UPDATEFILE_TAG,
    ERR_SWUPDATE_CANTGET_FREESPACE,
    ERR_SWUPDATE_CANTGET_TIMESTAMP,
    ERR_SWUPDATE_CANTGET_RELVERS,
    ERR_SWUPDATE_CANTGET_PREVVERS,
    ERR_SWUPDATE_XML_PARSE,
    ERR_SWUPDATE_FAILURE,
    ERR_SWUPDATE_LOCAL_XML,
    ERR_SWUPDATE_TEMP_XML,
    ERR_SWUPDATE_SDCARD_XML,
    ERR_SWUPDATE_NOVERSTAG,
    ERR_SWUPDATE_CANNOT_BUILD_VERSIONS_FILE,
    ERR_SWUPDATE_PRIV_KEY,
    ERR_SWUPDATE_SECRET_KEY,
    ERR_SWUPDATE_OUT_OF_MEMORY,
    ERR_SWUPDATE_BAD_VERSION_VALUE,
    SWUPDATE_IN_PROGRESS,
    SWUPDATE_PENDING,
    UPGRADE_MANAGER_UNIMPLEMENTED_ERROR,
    ERR_PKG_FILE_NOT_FOUND,
    ERR_PKG_FILE_READ,
    ERR_PKG_FILE_CRC,
    ERR_PKG_ENC_FILE_OPEN,
    ERR_PKG_ENC_FILE_READ,
    ERR_INVALID_PKG_NAME,
    ERR_INVALID_PKG,
    ERR_VERSION_IS_CURRENT,
    ERR_UPGRADE_ERROR,
    ERR_SWUPDATE_MAX_ERROR_NUM
} SwUpdateError_t;


#define UPGRADE_ERROR_STR \
    {SWUPDATE_SUCCESS,                              "PASS"}, \
    {ERR_SWUPDATE_CRC_FAILURE,                      "Upgrade CRC Error"}, \
    {ERR_SWUPDATE_NO_TMPSPACE,                      "Out of Temp Disk Space"}, \
    {ERR_SWUPDATE_NO_DISK_SPACE,                    "Out of Disk Space"}, \
    {ERR_SWUPDATE_CURRVERS_ALREADY,                 "Current Version Already Running"}, \
    {ERR_SWUPDATE_EXTRACTION,                       "Error extracking package files"}, \
    {ERR_SWUPDATE_DECRYPTION,                       "Error decrypting package"}, \
    {ERR_SWUPDATE_NO_VALID_VERSION,                 "No valid version number in package"}, \
    {ERR_SWUPDATE_CANNOT_CREATE_SCRIPT,             "Cannot create upgrade scripts"}, \
    {ERR_SWUPDATE_GET_CURRVERS,                     "Error getting current version"}, \
    {ERR_SWUPDATE_NO_IC_PROP,                       "No IC property in XML file"}, \
    {ERR_SWUPDATE_NO_UPGFILE_TAG,                   "No upgrade file tag"}, \
    {ERR_SWUPDATE_GET_PKGNAME,                      "Error getting package name"}, \
    {ERR_SWUPDATE_INVALIDPARAM,                     "Error invalid parameter"}, \
    {ERR_SWUPDATE_INFOREAD,                         "Info read error"}, \
    {ERR_SWUPDATE_NO_UPDATEFILE_TAG,                "No update file tag"}, \
    {ERR_SWUPDATE_CANTGET_FREESPACE,                "Error getting free space"}, \
    {ERR_SWUPDATE_CANTGET_TIMESTAMP,                "Error getting timestamp"}, \
    {ERR_SWUPDATE_CANTGET_RELVERS,                  "Error getting release version"}, \
    {ERR_SWUPDATE_CANTGET_PREVVERS,                 "Error getting previous version"}, \
    {ERR_SWUPDATE_XML_PARSE,                        "XML parsing error"}, \
    {ERR_SWUPDATE_FAILURE,                          "Upgrade failure"}, \
    {ERR_SWUPDATE_LOCAL_XML,                        "Local XML error"}, \
    {ERR_SWUPDATE_TEMP_XML,                         "Temp XML error"}, \
    {ERR_SWUPDATE_NOVERSTAG,                        "No version tag"}, \
    {ERR_SWUPDATE_CANNOT_BUILD_VERSIONS_FILE,       "Error building versions file"}, \
    {ERR_SWUPDATE_PRIV_KEY,                         "Private key error"}, \
    {ERR_SWUPDATE_SECRET_KEY,                       "Secret key error"}, \
    {ERR_SWUPDATE_OUT_OF_MEMORY,                    "Out of memory"}, \
    {ERR_SWUPDATE_BAD_VERSION_VALUE,                "Bad version number"}, \
    {SWUPDATE_IN_PROGRESS,                          "Upgrade in progress"}, \
    {SWUPDATE_PENDING,                              "Upgrade pending"}, \
    {UPGRADE_MANAGER_UNIMPLEMENTED_ERROR,           "Unimplemented command"}, \
    {ERR_PKG_FILE_NOT_FOUND,                        "Package file not found"}, \
    {ERR_PKG_FILE_READ,                             "Package file read error"}, \
    {ERR_PKG_FILE_CRC,                              "Package file CRC error"}, \
    {ERR_PKG_ENC_FILE_OPEN,                         "Encrypted file open error"}, \
    {ERR_PKG_ENC_FILE_READ,                         "Encrypted file read error"}, \
    {ERR_INVALID_PKG_NAME,                          "Invalid package name"}, \
    {ERR_INVALID_PKG,                               "Invalid Upgrade Package"}, \
    {ERR_VERSION_IS_CURRENT,                        "Latest Software Version Already Running"}, \
    {ERR_UPGRADE_ERROR,                             "An Error Occurred During Upgrading"}, \
    {ERR_SWUPDATE_MAX_ERROR_NUM,                    ""}


typedef enum
{
    UPGRADE_MANAGER_CANCEL_CMD = 0,
    UPGRADE_MANAGER_OTA_UPGRADE_CMD,
    UPGRADE_MANAGER_CONN_TYPE,
    UPGRADE_MANAGER_EXIT
} swu_card_manager_cmds;


typedef enum
{
    swu_Command,
    swu_Async,
    swu_Monitor
} swu_connection_type_t;

/*********************************************************
 * NAME:   upgradeMgrCmd
 * DESCRIPTION: Data Structure typedef used by the Kiosk
 *              application to send command requests to the
 *              Upgrade Manager application.
 *
 *********************************************************/

typedef struct _upgradeMgrCmd
{
    swu_card_manager_cmds command;  // Card Manager command
    swu_connection_type_t connType; // swu_Command, swu_Async, or swu_Monitor
    int protocolVersion;            // Version of communication protocol
} swu_upgradeMgrCmd_t;

typedef struct _otaUpgradeCmd
{
    swu_upgradeMgrCmd_t swu_command;
    char upgradePkgPathname[256];
} swu_upgradeOTACmd_t;

/*********************************************************
 * NAME:   upgradeMgrResult
 * DESCRIPTION: Data Structure typedef used by the Upgrade
 *              Manager application to send command return
 *              status and optional data to the Kiosk
 *              application.
 *
 *********************************************************/

typedef struct _upgradeMgrResult
{
    swu_card_manager_cmds command;  // Card Manager command
    int protocolVersion;            // Version of communication protocol
    int error;                      // Set to 1 if error, 0 if success
    int status;                     // Status/error code
} swu_upgradeMgrResult_t;

#endif // __UPDATE_DATADEF_H__
