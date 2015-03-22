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
 * netlink.c
 *
 * Author: Burt Bicksler bbicksler@mlsw.biz
 *
 * Description: The udev notifications are processed here.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#include "common.h"
#include "netlink.h"
#include "upgrademanager.h"
#include "swupdate.h"

// How long to wait for the USB drive mount to happen
#define MOUNT_TIMEOUT_SECS 10

// Used for buffer for reading grep command result
#define MNT_DATA_SIZE 512

extern int nVendID, nProdID;

/*********************************************************
 * NAME: init_netlink_uevent_socket
 * DESCRIPTION: Initalizes the netlink uevent socket
 *             connection.
 * IN: Nothing.
 * OUT: Returns socket fd or -1 on error.
 *
 **********************************************************/
int init_netlink_uevent_socket(void)
{
    struct sockaddr_nl snl;
    int retval;
    int fd;

    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (fd == -1)
    {
        printf("error getting socket: %s\n", strerror(errno));
        return -1;
    }


    retval = bind(fd, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl));
    if (retval < 0)
    {
        printf("bind failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/*********************************************************
 * NAME: search_key
 * DESCRIPTION: Searches the udev notification buffer for
 *              the requested value associated with the
 *              search key.
 *
 * IN: searchkey pointer to key value being searched for.
 *     buf pointer to the udev notification buffer.
 *     buflen size of the buffer data.
 * OUT: Pointer to searchkey data value if found, or NULL.
 *
 **********************************************************/
const char *search_key(
    const char *searchkey,
    const char *buf,
    size_t buflen)
{
    size_t bufpos = 0;
    size_t searchkeylen = strlen(searchkey);

    while (bufpos < buflen)
    {
        const char *key;
        int keylen;

        key = &buf[bufpos];
        keylen = strlen(key);
        if (keylen == 0)
            break;

        if ((strncmp(searchkey, key, searchkeylen) == 0) && key[searchkeylen] == '=')
            return &key[searchkeylen + 1];

        bufpos += keylen + 1;
    }
    return NULL;
}

/*********************************************************
 * NAME: isSoftwareUpdatePackagePresent
 * DESCRIPTION: Checks for the presence of the software
 *              upgrade package.
 *
 * IN: Nothing.
 * OUT: true if software upgrade package is found.  false
 *      otherwise.
 *
 **********************************************************/
bool isSoftwareUpdatePackagePresent(void)
{
    struct stat statbuf;
    mode_t modes;
    char szSwUpdatePathName[256];
    int rc;

    // Check to see if the update package file is present
    sprintf(szSwUpdatePathName, "%s/%s", SWUPDATE_MOUNT_POINT, SWUPDATE_PACKAGE_FILENAME);

    rc = stat(szSwUpdatePathName, &statbuf);

    modes = statbuf.st_mode;

    if(rc >= 0 && S_ISREG(modes))
    {
        /*
         *  Have the software update package file.
         *
         */
        return true;
    } /* End of we had File */

    return false;
}


/*********************************************************
 * NAME: isMountComplete
 * DESCRIPTION: Checks to see if the USB drive is fully
 *              mounted.
 *
 * IN:  pDiskName pointer to disk device name
 *      pMountPoint pointer to mount point to look for
 * OUT: true if disk is mounted on mount point, false if not.
 *
 **********************************************************/
bool isMountComplete(char *pDiskName, char *pMountPoint)
{
    int rc;
    char commandResponseData[MNT_DATA_SIZE];
    char szSystemCmd[256];
    char *pResult;
    FILE *p;
    bool bMountFinished = false;
    struct timeval tv; /* Used for mount timeout handling */

    time_t currTime, startTime; /* Used for mount timeout handling. */

    // Here we need to check /proc/mounts to see if our mount point has appeared.
    // Allow 20 seconds for the mount to happen before giving up.
    // As soon as the mount has appeared we can proceed with the software update.
    sprintf(szSystemCmd, "grep -c '%s %s' /proc/mounts", szDiskName,
            SWUPDATE_MOUNT_POINT);

    bMountFinished = false;

    gettimeofday(&tv, NULL);
    startTime = tv.tv_sec;

    while(!bMountFinished)
    {
        if ((p = popen(szSystemCmd, "r")) != NULL)
        {
            memset(commandResponseData, 0, MNT_DATA_SIZE);
            pResult = fgets(commandResponseData, MNT_DATA_SIZE, p);
            if(!pResult)
            {
                // Failed to read
                perror("fgets from grep of /proc/mounts failed: ");
            }

            rc = pclose(p);
            if(rc)
            {
                // Log the error
                printf("Received %d from grep of /proc/mounts\n", WEXITSTATUS(rc));
            }

            if(*commandResponseData == '1')
            {
                bMountFinished = true;
                break;
            }
        }

        gettimeofday(&tv, NULL);
        currTime = tv.tv_sec;
        if (startTime + MOUNT_TIMEOUT_SECS <= currTime)
        {
            // We have timed out waiting for the mount to
            // happen,
            // Log the error currently cannot return an error code from here.

            return false;
        }
    } // End of while

    return true;
} // End of function

/*********************************************************
 * NAME: process_netlink_uevent_message
 * DESCRIPTION: Processes uevent notification
 *
 * IN: fd socket descriptor
 * OUT: Nothing.
 *
 **********************************************************/
void process_netlink_uevent_message(int fd)
{
    char buf[UEVENT_BUFFER_SIZE*2];
    int ret;
    ssize_t buflen;
    ssize_t keys;
    const char *action, *subsys;
    const char *devname, *devpath;
    
#ifdef DEBUG
//    printf("%s: called\n", __func__);
#endif
    buflen = recv(fd, &buf, sizeof(buf), 0);
    if (buflen <= 0)
    {
        printf("[%s] Error receiving netlink message: %s\n",
                  __func__, strerror(errno));
        return;
    }

    keys = strlen(buf) + 1; /* start of payload */
    subsys = search_key("SUBSYSTEM", &buf[keys], buflen);
    action = search_key("ACTION", &buf[keys], buflen);

    if (strcmp(subsys, "block") == 0)
    {
        char szSystemCmd[256];
        char szWorkBuffer[256];
        int rc;

        if (strcmp(action, "add") == 0)
        {
            devpath = search_key("DEVPATH", &buf[keys], buflen);
            devname = search_key("DEVNAME", &buf[keys], buflen);

            // Build the dev path for the block device.
            strcpy(szWorkBuffer, "/dev/");
            strcat(szWorkBuffer, devname);

            /*
            *   Check to see if this USB disk device has an upgrade file
            *   on it.  If it is not yet mounted then we will need to mount
            *   the device.
            */

            strcpy(szDiskName, szWorkBuffer);
#ifdef DEBUG
            printf("%s: added disk dev node %s\n", __func__, szDiskName);
#endif
            /*
            *   See if we have the disk mounted
            */

            // Do I need to find a better way?  In testing it is sd(x)1 for the
            // partition.
            if(strchr(szDiskName, '1') != NULL)
            {
                sleep(2);
                
                // Only try to mount and process if we have the partition, not the entire disk.
                sprintf(szSystemCmd, "mount %s %s >/dev/null 2>&1", szDiskName,
                        SWUPDATE_MOUNT_POINT);

#ifdef DEBUG
                printf("%s: running %s\n", __func__, szSystemCmd);
#endif
                ret = system(szSystemCmd);
#ifdef DEBUG
                printf("%s: system returned %d\n", __func__, ret);
#endif

                ret = WEXITSTATUS(ret);

                // Expand test later, e.g. 32 is mount failed, but any non-zero
                // means the mount didn't succeed so try unmounting and remount.
                if (ret != 0)
                {
#ifdef DEBUG
                    printf("%s: mount error in USB upgrade: %d...retrying.\n", __func__, ret);
#endif
                    // Try to unmount the
                    sprintf(szSystemCmd, "umount %s >/dev/null 2>&1", szDiskName);
                    ret = system(szSystemCmd);

                    ret = WEXITSTATUS(ret);
                    if(ret)
                    {
                        // Test and log if error
#ifdef DEBUG
                        printf("%s: umount error in USB upgrade: %d\n", __func__, ret);
#endif
                    }

                    // Now try to mount again, if still and error then
                    // log it and move on.
                    sprintf(szSystemCmd, "mount %s %s >/dev/null 2>&1", szDiskName,
                            SWUPDATE_MOUNT_POINT);

                    ret = system(szSystemCmd);

                    ret = WEXITSTATUS(ret);
                    if (ret)
                    {
                        // Test and log if error
#ifdef DEBUG
                        printf("%s: mount error on retry of mount in USB upgrade: %d\n", __func__, ret);
#endif
                    }
                }

#ifdef DEBUG
                printf("%s: checking mount status...\n", __func__);
#endif
                bDiskDeviceMounted = isMountComplete(szDiskName, SWUPDATE_MOUNT_POINT);

#ifdef DEBUG
                printf("%s: mount status %s\n", __func__, bDiskDeviceMounted ? "true":"false");
#endif
                if (!bDiskDeviceAttached && bDiskDeviceMounted)
                {
                    // Check for the upgrade package file.
#ifdef DEBUG
                    printf("%s: checking for package\n", __func__);
#endif
                    rc = isSoftwareUpdatePackagePresent();
                    if (rc == true)
                    {
                        char szUpgradePackageName[256];
                        sprintf(szUpgradePackageName, "%s%s", SWUPDATE_MOUNT_POINT,
                                SWUPDATE_PACKAGE_FILENAME);

#ifdef DEBUG
                        printf("%s: package found...kicking off update\n", __func__);
#endif
                        // Kick off the software update process, this will check versions, and
                        // do the upgrade if appropriate.
                        performUpdate(szUpgradePackageName);
                        // Unmount the drive to commit any changes.
                        sprintf(szSystemCmd, "umount %s >/dev/null 2>&1", szDiskName);

                        ret = system(szSystemCmd);

                        ret = WEXITSTATUS(ret);
                        if(ret)
                        {
                            // Test and log if error
                            printf("umount error after software upgrade: %d\n", ret);
                        }

                         // Mark as unmounted and not attached.
                        bDiskDeviceMounted = false;
                   }
                   else
                   {
                        // Did not find an upgrade package.
                        // Copy log files from the system to the USB drive.
#define LOGFILES_PATHNAME "/usr/scu/logs"
                        sprintf(szSystemCmd, "cp -r %s %s", LOGFILES_PATHNAME, SWUPDATE_MOUNT_POINT);
                        ret = system(szSystemCmd);

                        ret = WEXITSTATUS(ret);
                        if(ret)
                        {
                            // Error copying log files
                            // Log the error and return.
                            printf("cp error copying of log files: %d\n", ret);
                        }

                        // Unmount the USB drive, commit the files.
                        sprintf(szSystemCmd, "umount %s >/dev/null 2>&1", szDiskName);

                        ret = system(szSystemCmd);
                        ret = WEXITSTATUS(ret);
                        if(ret)
                        {
                            // Test and log if error
                            printf("umount error after copy of log files: %d\n", ret);
                        }

                         // Mark as unmounted and not attached.
                        bDiskDeviceMounted = false;
                    }
                }

                bDiskDeviceAttached = true;
            }

        } // End if add action
        else if (strcmp(action, "remove") == 0)
        {
            devpath = search_key("DEVPATH", &buf[keys], buflen);
            devname = search_key("DEVNAME", &buf[keys], buflen);

            if(bDiskDeviceAttached)
            {
                // See if this is the device that we attached earlier
                strcpy(szWorkBuffer, "/dev/");
                strcat(szWorkBuffer, devname);

                if(strcmp(szWorkBuffer, szDiskName) == 0)
                {
#ifdef DEBUG
printf("%s: unmounting %s\n", __func__, szDiskName);
#endif
                    // If we are mounted umount
                    sprintf(szSystemCmd, "umount %s >/dev/null 2>&1", szDiskName);

                    ret = system(szSystemCmd);

                    ret = WEXITSTATUS(ret);
                    if (ret)
                    {
                        printf("umount failed on remove of USB drive, may be normal\n");
                        // Test and log if error
                    }

                    // Mark as unmounted and not attached.
                    bDiskDeviceAttached = false;
                    bDiskDeviceMounted = false;
                }
            }
        }
    } // End subsystem is block
}

