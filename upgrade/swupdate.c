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
*   swupdate.c - Software Update Daemon code
*/

/* Standard Linux headers */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/vfs.h>

/* Support Headers */
#include "common.h"
#include "zlib.h"

#include "xmlparser.h"
#include "log.h"

#include "upgradedatadefs.h"
#include "swupdate.h"

#define logError(fmt,args...)       (logOutput(LOG_VERBOSITY_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logWarning(fmt,args...)     (logOutput(LOG_VERBOSITY_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logInfo(fmt,args...)        (logOutput(LOG_VERBOSITY_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logDebug(fmt,args...)       (logOutput(LOG_VERBOSITY_DEBUG, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))


typedef enum
{
    LOG_VERBOSITY_NONE,
    LOG_VERBOSITY_ERROR,
    LOG_VERBOSITY_WARNING,
    LOG_VERBOSITY_INFO,
    LOG_VERBOSITY_DEBUG,
    LOG_VERBOSITY_MAX  /* Keep last */
} log_verbosity_t;


bool getImage(char *imageURL, char *path, unsigned long ic);

char *swupdateErrorMsgs[] = {   "Success.",
                                "Package CRC failure.",
                                "Out of temp file space.",
                                "Out of flash disk space.",
                                "Latest Software Version Already Installed.",
                                "Extraction of package file failed",
                                "Decryption of package file failed",
                                "No valid version found for installation.",
                                "Cannot create script file.",
                                "Get current software version failed.",
                                "No ic property for package file.",
                                "No upgrade file tag.",
                                "Failed to get package name from path.",
                                "Failed to get error message string.",
                                "Invalid Parameter",
                                "Cannot Process VersionInfo.xml",
                                "No Update tarball file tag",
                                "Cannot get disk free space",
                                "Cannot get time stamp value",
                                "Can't get release version value",
                                "Can't get previous version value",
                                "XML Parsing error, invalid file",
                                "Sofware Update Failed",
                                "Local versions.xml problem",
                                "Temp versions.xml problem",
                                "SD Card versions.xml problem",
                                "No versionList tag",
                                "Unable to build temp versions.xml file",
                                "Cannot Find Private Key",
                                "Cannot Find Secret Key",
                                "Out of memory",
                                "Software Update In Progress, Do not unplug",
                                "Bad Version Value",
                                "Software Update Pending",
                                NULL } ;


#define LOG_BUF_SIZE 8192

/*********************************************************
 *   NAME: logOutput
 *   DESCRIPTION: Currently just printf's to console
 *
 *   IN:    verb    - Verbosity.
 *          file    - Pointer to source file name.
 *          fcnt    - Pointer to function name
 *          line    - Line number in source file.
 *          format  - Pointer to format string
 *          ...     - Variable argument list.
 *
 *   OUT:   Nothing.
 **********************************************************/
void logOutput(int verb, char *file, const char *fctn, int line, const char *format, ...)
{
    int len;
    int size = LOG_BUF_SIZE;
    static char buf[LOG_BUF_SIZE];
    va_list ap;

    time_t rawtime;
    struct tm *ptm;
    time(&rawtime);
    ptm = gmtime(&rawtime);

    len = 0;

    len += snprintf(&buf[len], size, "[UTC: %04d-%02d-%02d %02d:%02d:%02d] File: %s Func: %s Line %d\n", ptm->tm_year + 1900,
                    ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
                    file, fctn, line);

    buf[len-2] = ']';
    buf[len-1] = '\0';
    len--;

    size = LOG_BUF_SIZE - len;

    va_start(ap, format);
    len += vsnprintf(&buf[len], size, format, ap);
    va_end(ap);

    printf("%s", buf);

}


/***********************************************************
 * NAME: getSoftwareUpdateErrorMsg()
 * DESCRIPTION: Using the passed in error code, returns
 *              the string for error reporting.
 *              If an unknown / invalid error code is
 *              passed then an Unknown Error code
 *              message is returned with the decimal
 *              value of the error code.
 *
 * IN:  swError - Error code to be processed.
 *
 * OUT: pointer to static buffer holding the error string.
 *      Note: This function is not intended to be thread safe.
 ***********************************************************/
char *getSoftwareUpdateErrorMsg(SwUpdateError_t swError)
{
    static char tmpErrorMsg[SWUPDATE_TMP_ERRSZ] = { 0 };
    strcpy(tmpErrorMsg, "Software Update: ");

    if (swError > SWUPDATE_SUCCESS && swError < ERR_SWUPDATE_MAX_ERROR_NUM)
    {
        strcat(tmpErrorMsg, swupdateErrorMsgs[swError]);
    }
    else
    {
        sprintf(tmpErrorMsg, "Unknown Error Code: %d", swError);
    }

    return tmpErrorMsg;
}

/***********************************************************
 * NAME: printSwErrorMsg()
 * DESCRIPTION: Using the passed in error code, function name,
 *              and line number log the error and, optionally,
 *              send the message to the flash UI.
 *
 * IN:  swError - Error code to be processed.
 *      funcName - Pointer to name of function called from
 *      lineNum - Line number called from.
 *
 * OUT: 0 - Success or error code.
 ***********************************************************/
int printSwErrorMsg(SwUpdateError_t swError,
                    const char *funcName,
                    int lineNum)
{
    char *errStr;
    const char *funcNameStr = "(null)";

    if (funcName)
    {
        funcNameStr = funcName;
    }

    errStr = getSoftwareUpdateErrorMsg(swError);
    if (errStr)
    {
//        logMessage(LEVEL_ERROR, "%s-%d: %s", funcNameStr, lineNum, errStr);
        return SWUPDATE_SUCCESS;
    }

    return ERR_SWUPDATE_ERRPRINT;
}

/***********************************************************
 * NAME: getFreeDiskSpace()
 * DESCRIPTION: Returns the free space for the given
 *              file system based on path to a file.
 *
 * IN:  Pointer to pathname of file on targeted file system
 *      Pointer to long that holds the free space on return.
 *
 * OUT: 0 on success, error code otherwise
 ***********************************************************/
int getFreeDiskSpace(char *pathToFile, unsigned long *swDiskFree)
{
    int retval = SWUPDATE_SUCCESS;

    struct statfs fsStatus;

    retval = statfs(pathToFile, &fsStatus);
    if (retval == SWUPDATE_SUCCESS)
    {
       *swDiskFree = (unsigned long)((unsigned long)fsStatus.f_bfree * (unsigned long)fsStatus.f_bsize);
    }
    else
    {
        logInfo("%s-%d: Failed to get the status of file system for free space: %s",__func__, __LINE__, pathToFile);
        retval = ERR_SWUPDATE_CANTGET_FREESPACE;
    }
    return retval;
} /* getFreeDiskSpace() */

// Version number helper functions

/***********************************************************
 * NAME: pointVersionValue()
 * DESCRIPTION: Get the point value (in decimal) from
 *              the passed in version number string.
 *
 * IN:  Pointer to version string to extract point value from
 *
 * OUT: Point value from version string, or -1 for error
 ***********************************************************/
int pointVersionValue(const char *tmpRelVers)
{
    char *pos ;

    // Get pointer to last '.'
    pos = strrchr(tmpRelVers, '.');
    if (pos)
    {
        // Point to first char of the point value
        pos++;
        return atoi(pos);
    }
    return -1;
}

/***********************************************************
 * NAME: majorVersionValue()
 * DESCRIPTION: Get the major value (in decimal) from
 *              the passed in version number string.
 *
 * IN:  Pointer to version string to extract major value from
 *      The passed in string should not include the prefix character
 *
 * OUT: Major value from version string, or -1 for error
 ***********************************************************/
int majorVersionValue(const char *tmpRelVers)
{
    char *pos ;
    char tmpVer[SWUPDATE_WRK_BUFSZ];

    strcpy(tmpVer, tmpRelVers);
    pos = strchr(tmpVer, '.');
    if (pos)
    {
        *pos = '\0';
        return atoi(pos);
    }
    return -1;
}

/***********************************************************
 * NAME: minorVersionValue()
 * DESCRIPTION: Get the minor value (in decimal) from
 *              the passed in version number string.
 *
 * IN:  Pointer to version string to extract minor value from
 *
 * OUT: Minor value from version string, or -1 for error
 ***********************************************************/
int minorVersionValue(const char *tmpRelVers)
{
    char *pos1 ;
    char *pos2 ;
    char tmpVer[SWUPDATE_WRK_BUFSZ];

    strcpy(tmpVer, tmpRelVers);
    // Get location of first '.'
    pos1 = strchr(tmpVer, '.');
    if (pos1)
    {
        // Have first '.' move one character past
        ++pos1;

         // Get last(2nd) '.'
        pos2 = strrchr(tmpVer, '.');
        if (pos2)
        {
            // Terminate substring for minor value at 2nd '.'
            *pos2 = '\0';
            return atoi(pos1);
        }
        else
        {
            return -1;
        }
    }
    return -1;
}


/***********************************************************
 * NAME: versionCompare()
 * DESCRIPTION: Compare two version number strings
 *              If version1 < version2 return -1.
 *              If version1 > version2 return 1.
 *              If version1 == version2 return 0.
 *
 *  Major, Minor and point values are used for compare
 *      no string compares are used for the greater than
 *      or less than cases.
 *
 * IN:  Pointer to version string1 to compare should be past prefix char
 *      Pointer to version string2 to compare should be past prefix char
 *
 * OUT: 0 if equal, -1 if version1 < version2 +1 if version1 > version2
 ***********************************************************/
int versionCompare(const char *passedVersion1, const char *passedVersion2)
{

    int majorVerVal1;
    int majorVerVal2;
    int minorVerVal1;
    int minorVerVal2;
    int pointVerVal1;
    int pointVerVal2;

    const char *version1 = passedVersion1;
    const char *version2 = passedVersion2;

    /*
    *   Use string compare for the equal case
    *   As that is the only one where it can
    *   safely be done, and will be faster than
    *   the conversions and numeric compares.
    *   Use strlen() to determine the compare
    *   for 0 length version strings.
    *
    *   Use numerical compares of the components
    *   of the version strings for the other
    *   GT/LT tests.
    */

    if (strcmp(version1, version2) == 0)
    {
        // Return 0 for equal
        return SWUPDATE_VER_EQ;
    }

    if (strlen(version1) == 0)
    {
        // Return -1 for less than
        return SWUPDATE_VER_LT;
    }

    if (strlen(version2) == 0)
    {
        // Return +1 for greater than
        return SWUPDATE_VER_GT;
    }

    /*
    *   Now do the numerically based compares
    *   of the major, minor and point components
    *
    *   Assumes that we have validated the version
    *   strings earlier in the code.  So we will
    *   not bother with testing for failure from the
    *   helpers that return the numeric values.
    */

    // Get the major version values
    majorVerVal1 = majorVersionValue(version1);

    majorVerVal2 = majorVersionValue(version2);

    // Get the minor version values
    minorVerVal1 = minorVersionValue(version1);
    minorVerVal2 = minorVersionValue(version2);

    // Get the point version values
    pointVerVal1 = pointVersionValue(version1);
    pointVerVal2 = pointVersionValue(version2);

    // Comparisons
    if (majorVerVal1 < majorVerVal2)
    {
        return SWUPDATE_VER_LT;
    }

    if (majorVerVal1 > majorVerVal2)
    {
        return SWUPDATE_VER_GT;
    }

    // Major values are the same

    if (minorVerVal1 < minorVerVal2)
    {
        return SWUPDATE_VER_LT;
    }

    if (minorVerVal1 > minorVerVal2)
    {
        return SWUPDATE_VER_GT;
    }

    if (pointVerVal1 < pointVerVal2)
    {
        return SWUPDATE_VER_LT;
    }

    if (pointVerVal1 > pointVerVal2)
    {
        return SWUPDATE_VER_GT;
    }

    /*
    *   We have to return something here to keep compiler happy and this would be
    *   the correct value if we were not handling the equal case with string compare
    *   at top of function.
    */
    return SWUPDATE_VER_EQ;
}



/***********************************************************
 * NAME: decryptPackage()
 * DESCRIPTION: Decrypts the passed in file, places the file
 *              in the passed in location with the passed in
 *              file name.
 *
 *
 * IN:  Pointer to pathname of encrypted package file
 *      Pointer to path where decrypted package file will be
 *      placed.
 *
 * OUT: 0 on success, error code otherwise
 ***********************************************************/
int decryptPackage(const char *encryptedPackage,
                   const char *decryptedPackagePath)
{
    int retval = SWUPDATE_SUCCESS;
    char sysCmd[SWUPDATE_MAX_CMD];

    struct stat info;

    /*
    *   Verify that we have the private and encrypted secret keys
    *   present on the frame.  By definition in the design these
    *   keys are located in the /usr/scu/keys subdirectory.
    */

    retval = stat(PRIV_KEY, &info);
    if (retval)
    {
        printSwErrorMsg(ERR_SWUPDATE_PRIV_KEY, __func__, __LINE__);
        return ERR_SWUPDATE_PRIV_KEY;
    }

    retval = stat(SECRET_KEY, &info);
    if (retval)
    {
        printSwErrorMsg(ERR_SWUPDATE_PRIV_KEY, __func__, __LINE__);
        return ERR_SWUPDATE_PRIV_KEY ;
    }

    /* Build command to decrypt the encrypted private key using the RSA private key */
    sprintf(sysCmd, "openssl rsautl -decrypt -inkey %s -in %s -out %s", PRIV_KEY, SECRET_KEY, DECRYPT_KEY);

    retval = system(sysCmd);
    if (retval)
    {
        logInfo("%s-%d: Failed to DECRYPT SECRET KEY",__func__, __LINE__);
        printSwErrorMsg(ERR_SWUPDATE_DECRYPTION, __func__, __LINE__);
        return retval;
    }

    /*
    *   Make sure that the key is only readable by the owner
    *   May be overkill for embedded use, so can disuss
    *   removing.
    */
    sprintf(sysCmd, "chmod 600 %s", DECRYPT_KEY);
    retval = system(sysCmd);
    if (retval)
    {
        printf("chmod 600 of decrypt key returned %d\n", retval);
        // Log and handle error.
    }

    // Need to make sure that the decrypted Package Path exists
    sprintf(sysCmd, "mkdir -p %s", decryptedPackagePath);
    retval = system(sysCmd);

    /* Build command to decrypt the packge wrapper and extract the package file using the decrypted secret key */
    sprintf(sysCmd, "dd if=%s|openssl enc -d -blowfish -pass file:%s|tar xzf - -C %s", encryptedPackage,
            DECRYPT_KEY, decryptedPackagePath);
    retval = system(sysCmd);
    if (retval)
    {
        logInfo("%s-%d: Failed to DECRYPT package file %s to %s",__func__, __LINE__, encryptedPackage, decryptedPackagePath);
        retval = ERR_SWUPDATE_DECRYPTION;
        printSwErrorMsg(retval, __func__, __LINE__);
        goto Error1;
    }

    /*
    *   The calling function will be attempting to extract the package contents, so if there was
    *   some kind of failure that was not detected during decryption, e.g. a badly built package
    *   the extraction process will return a failure code.  So we do not need to jump though the
    *   hoops here to use tar -t to test the decrypted and extracted package file..
    */

Error1:
    unlink(DECRYPT_KEY);

    return retval;
}

/***********************************************************
 * NAME: shouldUpdate()
 * DESCRIPTION: Test the current versions.xml and upgrade
 *              package versions.xml release verseion numbers
 *              If the same then no upgrade required.
 *              Otherwise return true (upgrade required).
 *
 * IN:  Pointer to current software versions.xml file
 *      Pointer to release software versionx.xml file
 *      Pointer to buffer to receive the release version number
 *
 * OUT: true if version numbers are different, or no current
 *              versions.xml file.
 *      false if the version numbers are the same.
 ***********************************************************/
int shouldUpdate(char *currentVersionsFile,
                 char *upgradeVersionsFile,
                 char *releaseVersion)
{
    char currentSWVersion[SWUPDATE_WRK_BUFSZ] = {0};
    char *workStr;

    int retval = SWUPDATE_SUCCESS;

    xmlNodePtr versionNode;

    xmlNodePtr root;

    xmlDocPtr versDoc;

    // Parse the current versions XML file, get the version number
    versDoc = xmlReadFile(currentVersionsFile, NULL, XML_PARSE_RECOVER);
    if (versDoc == NULL )
    {
        // If no versions.xml file in the current /usr/scu then we will do the upgrade
        return 1;
    }

    root = xmlDocGetRootElement(versDoc);
    if ( root == NULL )
    {
        retval = ERR_SWUPDATE_NOVERSTAG;
        goto Exit1;
    }

    versionNode = getNodeByName(root, "version");
    if (versionNode == NULL)
    {
        /* There is a problem with the versions.xml file */
        retval = ERR_SWUPDATE_XML_PARSE;
        goto Exit1;
    }

    while(versionNode)
    {
        /* reset the retval to be no valid version each
        *   time we start the processing again.
        *   retval will be set to success by each function that is
        *   called if there was no error, so we want to accurately
        *   report no valid version found when we drop out of the loop at
        *   end of XML file.
        */
        retval = ERR_SWUPDATE_NO_VALID_VERSION;

        // Now get releaseVersion
        workStr = getNodeContentByName(versionNode, "releaseVersion");
        if (workStr)
        {
            int tmpLen = strlen(workStr);
            if (tmpLen >= SWUPDATE_MIN_VER_LEN)
            {
                strcpy(currentSWVersion, workStr);
                xmlFree(workStr);
            }
            else
            {
                retval = ERR_SWUPDATE_BAD_VERSION_VALUE;
                xmlFree(workStr);
                break;
            }
        }
        else
        {
            retval = ERR_SWUPDATE_CANTGET_RELVERS;
            break;
        }

        versionNode = versionNode->next;
    } // End of loop for processing version nodes

    if (versDoc)
    {
        xmlFreeDoc(versDoc); // Effectively close the XML file
    }

    // Parse the upgrade versions XML file, get the version number
    versDoc = xmlReadFile(upgradeVersionsFile, NULL, XML_PARSE_RECOVER);
    if (versDoc == NULL )
    {
        // No temp versions.xml file found, or error opening it
        logInfo("%s-%d: Error reading: %s", __func__, __LINE__, upgradeVersionsFile);
        return ERR_SWUPDATE_TEMP_XML;
    }

    // Should have currentSWVersion now.  but it may be none
    // (as well as being empty).  If it is none set to to empty.
    if (strcmp(currentSWVersion, "none" ) == 0)
    {
        currentSWVersion[0] = '\0';
    }

    root = xmlDocGetRootElement(versDoc);
    if ( root == NULL )
    {
        retval = ERR_SWUPDATE_NOVERSTAG;
        goto Exit1;
    }

    versionNode = getNodeByName(root, "version");
    if (versionNode == NULL)
    {
        /* There is a problem with the versions.xml file */
        retval = ERR_SWUPDATE_XML_PARSE;
        goto Exit1;
    }

    while(versionNode)
    {
        /* reset the retval to be no valid version each
        *   time we start the processing again.
        *   retval will be set to success by each function that is
        *   called if there was no error, so we want to accurately
        *   report no valid version found when we drop out of the loop at
        *   end of XML file.
        */
        retval = ERR_SWUPDATE_NO_VALID_VERSION;

        // Now get releaseVersion
        workStr = getNodeContentByName(versionNode, "releaseVersion");
        if (workStr)
        {
            int tmpLen = strlen(workStr);
            if (tmpLen >= SWUPDATE_MIN_VER_LEN)
            {
                strcpy(releaseVersion, workStr);
                xmlFree(workStr);
            }
            else
            {
                retval = ERR_SWUPDATE_BAD_VERSION_VALUE;
                xmlFree(workStr);
                break;
            }
        }
        else
        {
            retval = ERR_SWUPDATE_CANTGET_RELVERS;
            break;
        }

        // Pass in the version strings excluding the prefix character
        if (versionCompare(&releaseVersion[1], &currentSWVersion[1]) == 0)
        {
            retval = ERR_SWUPDATE_CURRVERS_ALREADY;
            goto Exit1; // Common exit/error point.
        }

// All we care about here is that the versions are different, so
// We can now do the upgrade.  We don't need to extract anything from
// the file for this we are done and can return true.
        if (versDoc)
        {
            free(versDoc);
            return 1; // True we have different version numbers.
        }

        versionNode = versionNode->next;
    } // End of loop for processing version nodes

    if (versDoc)
    {
        xmlFreeDoc(versDoc);
        versDoc = NULL;
    }

Exit1:
    if (versDoc)
    {
        xmlFreeDoc(versDoc);
    }
    // else return false (0)
    return 0;
}


/***********************************************************
 * NAME: performUpdate()
 * DESCRIPTION: Test the current versions.xml and upgrade
 *              package versions.xml release verseion numbers
 *              If the same then no upgrade required.
 *              Otherwise return true (upgrade required).
 *
 * IN:  Pointer to upgrade package file pathname.
 *
 * OUT: 0 for success or error code on failure.
 ***********************************************************/
int performUpdate(char *upgradePackageName)
{
    uint32_t ret = 0;
    struct stat info;
    FILE *fp = NULL;
    uint8_t *buffer = NULL;
    ulong *pCRC;
    ulong calculatedCRC;
    char *pEncFilename = NULL;
    char sysCmd[512];
    char szUpgradeVersionName[512];
    char szCurrentVersionName[512];
    char szReleaseVersion[10] = {0};
    char decryptedPackagePath[SWUPDATE_MAX_PATH] = UPDATE_TEMP_DIR;
    char decryptedPackageFile[SWUPDATE_MAX_PATH];

    char extractedFilesDir[SWUPDATE_MAX_PATH] = UPDATE_TEMP_DIR;

#ifdef DEBUG
    printf("%s: found %s...validating...\n", __func__, upgradePackageName);
#endif /* DEBUG */

    // Read the package file with CRC attached.
    fp = fopen(upgradePackageName, "rb");
    if (fp)
    {
        ret = stat(upgradePackageName, &info);
        buffer = (uint8_t*)calloc(1, info.st_size);
        ret = fread(buffer, 1, info.st_size, fp);
        if (ret != info.st_size)
        {
            free(buffer);
            fclose(fp);
            return ERR_PKG_FILE_READ;
        }
        else
        {
            pCRC = (ulong *)(buffer + (info.st_size - sizeof(ulong)));
            ret = info.st_size;
            fclose(fp);
        }
    }
    else
    {
        return ERR_PKG_FILE_NOT_FOUND;
    }

    // Compute the CRC on the file proper, excluding the CRC at end.
    calculatedCRC = crc32(0L, Z_NULL, 0);
    calculatedCRC = crc32(calculatedCRC, buffer, info.st_size - sizeof(ulong));
    // If different then error
    if (*pCRC != calculatedCRC)
    {
        // CRC error corrupted file.
        return ERR_PKG_FILE_CRC;
    }

#ifdef DEBUG
    printf("%s: package %s valid...stripping CRC...\n", __func__, upgradePackageName);
#endif /* DEBUG */

    pEncFilename = calloc(1, strlen(upgradePackageName) + 1);
    if (pEncFilename)
    {
        char *pWork;
        strcpy(pEncFilename, upgradePackageName);
        pWork = strchr(pEncFilename , '.');
        if (pWork)
        {
            sprintf(pWork, ".enc");
        }
    }

    // Write the .enc encrypted file without CRC
    fp = fopen( pEncFilename, "wb");
    if (fp)
    {
        // Write the bytes up to the CRC to a .enc file
        ret = fwrite(buffer, 1, info.st_size - sizeof(ulong), fp);
        if (ret != (info.st_size - sizeof(ulong)))
        {
            // Error writing file.
            fclose(fp);
            free(buffer);
            /* remove the stripped encrypted file if present */
            unlink(pEncFilename);
            free(pEncFilename);
            return ERR_PKG_ENC_FILE_READ;
        }

        fclose(fp);
        // Need to sync to make sure that data is written to disk
        ret = system("sync");
        if (ret)
        {
            printf("sync system call returned %d\n", ret);
            // Log and handle error.
        }
    }
    else
    {
        return ERR_PKG_ENC_FILE_OPEN;
    }

#ifdef DEBUG
    printf("%s: package %s stripped...decrypting...\n", __func__, upgradePackageName);
#endif /* DEBUG */

    // Decrypt the .enc file to .tgz
    strcat(extractedFilesDir, "/");
    ret = decryptPackage(pEncFilename, decryptedPackagePath);

    if (ret)
    {
        // decryption operation failed.
        printSwErrorMsg(ERR_SWUPDATE_DECRYPTION, __func__, __LINE__);
        /* remove the stripped encrypted file */
        unlink(pEncFilename);
        if (buffer)
        {
            free(buffer);
        }

        if (pEncFilename)
        {
            free(pEncFilename);
        }
        return ERR_SWUPDATE_DECRYPTION;
    }

    /* We've sucessfully decrypted...remove the stripped encrypted file */
    unlink(pEncFilename);
    sprintf(sysCmd, "%s", "sync");
    ret = system(sysCmd); // Try to commit the change to the USB drive

#ifdef DEBUG
    printf("%s: package %s decrypted...determining upgrade viability...\n", __func__, upgradePackageName);
#endif /* DEBUG */

    sprintf(decryptedPackageFile, "%s/%s.tgz", decryptedPackagePath, UPGRADE_PKG_BASENAME);

    // Extract the upgrade package versions.xml file
    sprintf(sysCmd, "tar -xz -f %s -C %s usr/scu/default/versions.xml",
            decryptedPackageFile,
            TMPFILESDIR);

    ret = system(sysCmd);
    if (ret)
    {
        // Log error, clean up
        printSwErrorMsg(ERR_SWUPDATE_EXTRACTION, __func__, __LINE__);
        return ERR_SWUPDATE_EXTRACTION ;
    }

    // Upgrade versions.xml file is at /tmp/usr/scu/default/versions.xml

    // Compare the vers
    sprintf(szUpgradeVersionName, "%s/usr/scu/default/%s", TMPFILESDIR, "versions.xml");
    sprintf(szCurrentVersionName, "%s", "/usr/scu/default/versions.xml");

    ret = shouldUpdate(szCurrentVersionName,
                 szUpgradeVersionName,
                 szReleaseVersion);
    if (ret == 1)
    {
        pid_t pid;
        char szVupdate[256];

#ifdef DEBUG
    printf("%s: package %s needed...upgrading...\n", __func__, upgradePackageName);
#endif /* DEBUG */

        // extract the dobackup file to our temp directory.
        sprintf(sysCmd, "tar -xz -f %s -C %s usr/scu/bin/dobackup", decryptedPackageFile,
                TMPFILESDIR);
        ret = system(sysCmd);
        if (ret)
        {
            // Log error, clean up
            printSwErrorMsg(ERR_SWUPDATE_EXTRACTION, __func__, __LINE__);
            return ERR_SWUPDATE_EXTRACTION ;
        }

        // extract the upgrade file to our temp directory.
        sprintf(sysCmd, "tar -xz -f %s -C %s usr/scu/bin/upgrade", decryptedPackageFile,
                TMPFILESDIR);
        ret = system(sysCmd);
        if (ret)
        {
            // Log error, clean up
            printSwErrorMsg(ERR_SWUPDATE_EXTRACTION, __func__, __LINE__);
            return ERR_SWUPDATE_EXTRACTION;
        }

        sprintf(szVupdate, "%susr/scu/bin/upgrade", TMPFILESDIR);

        // Versions are Do the following:
        /*
        *   fork off a process that will run the vupdate script.
        *   That process will kill us off but stopping first scu
        *   and then svsd.
        *   Then the vupdate script will perform a backup using
        *   the dobackup script and finally will
        *   Extract the upgrade package file and copy back the
        *   current /usr/scu/etc configuration files.
        */

        char *prog1_argv[4];
        prog1_argv[0] = "/bin/sh";
        prog1_argv[1] = szVupdate;
        prog1_argv[2] = NULL;

        /* Fork our vupdate script processing, which will shut us down */
        pid = fork();
        switch (pid)
        {
            case -1:
                printf("[%s] fork of %s failed\n", __func__, prog1_argv[0]);
            break;

            case 0:
            {
                execvp(prog1_argv[0], prog1_argv);
            }
            break;

            default:
            break;
        }
    }
    else
    {
        // Log the fact that the versions are the same, no update will be done.
        // Return ERR_VERSION_IS_CURRENT
        if (buffer)
        {
            free(buffer);
        }

        if (pEncFilename)
        {
            free(pEncFilename);
        }
        return ERR_VERSION_IS_CURRENT;
    }

    if (buffer)
    {
        free(buffer);
    }

    if (pEncFilename)
    {
        free(pEncFilename);
    }

    return 0;
}

