// --------------------------------------------------------------------------------------
// Module     : svsState
// Description: System state module
// Author     :
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <svsErr.h>
#include <svsLog.h>
#include <svsCommon.h>
//#include <svsMsg.h>
#include <svsApi.h>
#include <svsState.h>

static int svsStateMemoryInit(void);

static svsStateMem_t *svsStateMem = 0;

int svsStateInit(void)
{
    int rc;

    rc = svsStateMemoryInit();
    if(rc != 0)
    {
        return(rc);
    }

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    // add robust attribute in case the calling thread dies while inside the critical section
    // this will prevent other processes from being locked, but will generate errors and exit
    // XXX could find a way to recover, but at least other apps will not block
    // see: http://www.embedded-linux.co.uk/tutorial/mutex_mutandis
    pthread_mutexattr_setrobust_np(&mutexAttr, PTHREAD_MUTEX_ROBUST_NP);
    pthread_mutex_init(&svsStateMem->c.mutex, &mutexAttr);

    svsStateMem->c.initFlag = 1;

    return(rc);
}

void svsStateUninit(void)
{
    logDebug("");

    svsStateMem->c.initFlag = 0;

    pthread_mutex_destroy(&svsStateMem->c.mutex);
}

int svsStateIsInit(void)
{
    if(svsStateMem->c.initFlag)
    {
        return(1);
    }
    return(0);
}

static int svsStateMemoryInit(void)
{
    int status;

    logDebug("");

    /* Assume success */
    status = 0;

    /*   If the shared memory isn't mapped, then open the shared memory object and map it. */
    if(0 == svsStateMem)
    {
        int sharedMemHandle;

        logDebug("Shared memory not mapped");

        /* TODO: Wide open permissions to this shared object, is that OK? */
        sharedMemHandle = shm_open(SHARED_MEMORY_OBJECT_NAME, O_RDWR, 0777);

        /* Did the open fail ? */
        if(-1 == sharedMemHandle)
        {
            /*   If it failed because the shared memory does not yet exist
             * then create it.
             */
            logDebug("Shared memory does not exist");
            if(ENOENT == errno)
            {
                logDebug("Creating shared memory object");
                /* TODO: Wide open permissions to this shared object,
                 * is that OK?
                 */
                sharedMemHandle = shm_open(SHARED_MEMORY_OBJECT_NAME, (O_RDWR | O_CREAT), 0777);
                /* If the open with create fails, stop processing */
                if(-1 == sharedMemHandle)
                {
                    status = -1;
                    logError("shm_open: %s", strerror(errno));
                }
                else
                {   /*   After creating the object we have to give it a size */
                    logDebug("Setting shared memory object size to %d", sizeof(svsStateMem_t));
                    status = ftruncate(sharedMemHandle, sizeof(svsStateMem_t));
                    if(-1 == status)
                    {
                        logError("ftruncate: %s", strerror(errno));
                    }
                }
            }
            else
            {   /*   The open failed for some other reason than the shared
                 * memory object not existing already, stop processing
                 */
                logError("failed to open shared memory");
                status = -1;
                logError("shm_open: %s", strerror(errno));
            }
        } /* if(-1 == sharedMemHandle) */

        /*   If the shared memory object handle is now open, map some memory */
        if(-1 != sharedMemHandle)
        {
            svsStateMem = mmap(NULL, sizeof(svsStateMem_t),
                               (PROT_EXEC | PROT_READ | PROT_WRITE),
                               MAP_SHARED, sharedMemHandle, 0);

            if(MAP_FAILED == svsStateMem)
            {
                logError("failed to map memory");
                svsStateMem = NULL;
                logError("mmap: %s", strerror(errno));
                status = -1;
            }
            /*   Once the mmap() has been called, we don't need the
             * shared memory object handle any longer
             */
            close(sharedMemHandle);
        }
    } /* if(NULL == svsStateMem) */

    return(status);
}

int svsStateGet(size_t field_offset, uint16_t size, void *data)
{
    int rc;

//    if(field == 0)
//    {
//        logError("null pointer")
//        return(-1);
//    }
    if(data == 0)
    {
        logError("null pointer");
        return(-1);
    }
/*    if(((uint8_t *)field < (uint8_t *)&svsStateMem->d) ||
       ((uint8_t *)field > ((uint8_t *)&svsStateMem->d + (uint8_t *)sizeof(svsStateMemData_t))))
    {
        logError("field out of range")
        return(-1);
    }
*/
    // Perform atomic operation
    rc = pthread_mutex_lock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_lock: %s\n",  strerror(errno));
        exit(1);
    }

    memcpy(data, (uint8_t *)&svsStateMem->d + field_offset, size);

    rc = pthread_mutex_unlock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_unlock: %s\n",  strerror(errno));
        exit(1);
    }

    return(0);
}

int svsStateSet(size_t field_offset, uint16_t size, void *data)
{
    int rc;

//    if(field == 0)
//    {
//        logError("null pointer")
//        return(-1);
//    }
    if(data == 0)
    {
        logError("null pointer");
        return(-1);
    }
    /*if(((uint8_t *)field < (uint8_t *)&svsStateMem->d) ||
       ((uint8_t *)field > ((uint8_t *)&svsStateMem->d + (uint8_t *)sizeof(svsStateMemData_t))))
    {
        logError("field out of range")
        return(-1);
    }*/

    // Perform atomic operation
    rc = pthread_mutex_lock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_lock: %s\n",  strerror(errno));
        exit(1);
    }

    memcpy((uint8_t *)&svsStateMem->d + field_offset, data, size);

    rc = pthread_mutex_unlock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_unlock: %s\n",  strerror(errno));
        exit(1);
    }

    return(0);
}

// Get a pointer to the control part of the state, direct access no protection
int svsStateControlGet(size_t field_offset, void **data)
{
    if(data == 0)
    {
        logError("null pointer");
        return(-1);
    }

    *data = (uint8_t *)&svsStateMem->c + field_offset;

    return(0);
}

// Get a pointer to the data part of the state and lock access
int svsStateDataLockAndGet(svsStateMemData_t **data)
{
    int rc;

    if(data == 0)
    {
        logError("null pointer");
        return(-1);
    }

    // lock access
    rc = pthread_mutex_lock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_lock: %s\n",  strerror(errno));
        exit(1);
    }
    *data = &svsStateMem->d;

    return(0);
}

// Unlock access to pointer of the data part of the state
int svsStateDataUnlock(void)
{
    int rc;

    // unlock access
    rc = pthread_mutex_unlock(&svsStateMem->c.mutex);
    if(rc != 0)
    {
        fprintf(stderr, "ERROR: pthread_mutex_unlock: %s\n",  strerror(errno));
        exit(1);
    }

    return(0);
}


