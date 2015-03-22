/*
 * Copyright (C) 2009 MapleLeaf Software, Inc
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
 * swupdate.h
 *
 */
#ifndef _SWUPDATE_H
#define _SWUPDATE_H

#include <upgradedatadefs.h>
/*
 * Contains definitions for software update error codes
 * and related message strings.
 *
 * Also other enums, and defines that are specific to the
 * software update logic.
 *
 * There is a one to one correspondence between the
 * Error status codes and the corresponding char pointer
 * to the associated message.
 *
 * The MAX_ERROR_NUM value is used by the
 * getSoftwareUpdateErrorMsg() function to ensure
 * that only valid, known error messages are returned.
*/

#define UPGRADE_PKG_BASENAME "kioskupgrade"

#define KEYDIR "/usr/scu/keys"
#define TEMPKEYDIR "/tmp/usr/scu/keys"
#define UPDATE_TEMP_DIR "/tmp"
// Where to extract the upgrade archive for interim processing.
#define TMPFILESDIR "/tmp/"

// Need these keys for decrypting
#define PRIV_KEY KEYDIR"/upgrade.prv"
#define SECRET_KEY KEYDIR"/secret.key"

// This key is the decrypted secret key name.
#define DECRYPT_KEY KEYDIR"/secret.decryp"

#define SOFTWARE_UPDATE_CMD UPDATE_TEMP_DIR"/upgrade"

/* Names for dynamically generated script files */
#define UPDATE_BACKUP_SCRIPT "/tmp/usr/scu/dobackup"
#define UPDATE_DELETE_SCRIPT "/tmp/usr/scu/delfiles"
#define UPDATE_ADDEDDEL_SCRIPT "/sysrecovery/deladded"

/* Names for local and temp versions.xml files. */
#define LOCAL_VERSIONS_FILE "/usr/scu/etc/versions.xml"
#define TEMP_VERSIONS_FILE "/tmp/usr/scu/versions.xml"

#define UPDATE_SYSBACKUP_FILE "/sysrecovery/sysbackup.tgz"

/* The minimum length for a valid version string N.N.N */
#define SWUPDATE_MIN_VER_LEN 5

#define SWUPDATE_MAX_PATH   128
#define SWUPDATE_MAX_CMD    256
#define SWUPDATE_WRK_BUFSZ  64
#define SWUPDATE_TMP_ERRSZ  256

#define SWUPDATE_VER_LT -1
#define SWUPDATE_VER_GT 1
#define SWUPDATE_VER_EQ 0

/*
 * The following constants are used by the applications to access the
 * elements of the system configuration file.
 */

#define MAX_FILENAME_SIZE   256

char *getSoftwareUpdateErrorMsg(SwUpdateError_t swError);
int printSwErrorMsg(SwUpdateError_t swError, const char *funcName, int lineNum);
int getCurrentSoftwareVersion(char *currentSWVersion);
int performUpdate(char *upgradePackageName);

#endif


