/*
 *  Copyright (c) 2022, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file pdm_ram_storage_glue.c
 * File used for the glue between PDM and RAM Buffer
 *
 */

#include "pdm_ram_storage_glue.h"
#include "PDM.h"
#include "platform-k32w.h"
#include "ram_storage.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <utils/code_utils.h>
#include <openthread/platform/memory.h>

#if PDM_SAVE_IDLE
#include "fsl_os_abstraction.h"

#if defined(USE_RTOS) && (USE_RTOS == 1)
#define mutex_lock OSA_MutexLock
#define mutex_unlock OSA_MutexUnlock
#else
#define mutex_lock(...)
#define mutex_unlock(...)
#endif

#define MAX_QUEUE_SIZE (16)
#define PDM_PAGE_SIZE (4096 - 256) // Page size (4096) - 256 to take into account segment header. TODO: compute reserved size for payload in a segment

typedef struct
{
    ramBufferDescriptor *pvDataBuffer;
    uint16_t             u16IdValue;
} tsQueueEntry;

static tsQueueEntry asQueue[MAX_QUEUE_SIZE];
static uint8_t      u8QueueWritePtr;
static uint8_t      u8QueueReadPtr;
static osaMutexId_t asQueueMutex;
static bool_t       asQueueMutexTaken;

static uint8_t u8IncrementQueuePtr(uint8_t u8CurrentValue);

#endif /* PDM_SAVE_IDLE */

#if !ENABLE_STORAGE_DYNAMIC_MEMORY
#ifndef PDM_BUFFER_SIZE
#define PDM_BUFFER_SIZE (1024 + kRamDescSize) /* kRamBufferInitialSize is 1024 */
#endif
static uint8_t sPdmBuffer[PDM_BUFFER_SIZE] __attribute__((aligned(4))) = {0};

#if PDM_ENCRYPTION
static uint8_t sPdmStagingBuffer[PDM_BUFFER_SIZE] __attribute__((aligned(4))) = {0};
#endif

#endif /* !ENABLE_STORAGE_DYNAMIC_MEMORY */

extern void *otPlatRealloc(void *ptr, size_t aSize);

#if PDM_ENCRYPTION

static PDM_portConfig_t pdm_PortContext = {NULL, 0, NULL, 0};

#if ENABLE_STORAGE_DYNAMIC_MEMORY

/* Buffer used to temporary copy RAM buffer data in order to sync save it. */
static uint8_t sPageBuffer[PDM_PAGE_SIZE];

/* Alloc/Realloc staging buffer in PDM encryption context */
static rsError stagingBufferResize(PDM_portConfig_t *pdm_PortContext, uint16_t newSize)
{
    rsError  err = RS_ERROR_NONE;
    uint8_t *ptr = NULL;

    otEXPECT_ACTION((pdm_PortContext != NULL), err = RS_ERROR_PDM_ENC);

    if (pdm_PortContext->staging_buf_size < newSize)
    {
        ptr = (uint8_t *)otPlatRealloc((void *)pdm_PortContext->pStaging_buf, newSize);
        otEXPECT_ACTION((NULL != ptr), err = RS_ERROR_NO_BUFS);
        pdm_PortContext->pStaging_buf     = ptr;
        pdm_PortContext->staging_buf_size = newSize;
    }

exit:
    return err;
}

#endif /* ENABLE_STORAGE_DYNAMIC_MEMORY */

/* Initialize and enable PDM encryption context */
static rsError initPdmEncContext(PDM_portConfig_t *pdm_PortContext,
                                 uint8_t          *stagingBuffer,
                                 uint16_t          stagingBufferSize,
                                 uint32_t         *encKey,
                                 uint8_t           config_flags)
{
    rsError      err    = RS_ERROR_NONE;
    PDM_teStatus status = PDM_E_STATUS_OK;
    otEXPECT_ACTION((pdm_PortContext != NULL), err = RS_ERROR_PDM_ENC);

    if (config_flags == PDM_CNF_ENC_ENABLED)
    {
        if (stagingBuffer)
        {
            pdm_PortContext->pStaging_buf     = stagingBuffer;
            pdm_PortContext->staging_buf_size = stagingBufferSize;
        }
        else
        {
            err = stagingBufferResize(pdm_PortContext, stagingBufferSize);
            otEXPECT(err == RS_ERROR_NONE);
        }
    }
    else if (config_flags == (PDM_CNF_ENC_ENABLED | PDM_CNF_ENC_TMP_BUFF))
    {
        pdm_PortContext->pStaging_buf     = NULL;
        pdm_PortContext->staging_buf_size = 0;
    }
    else
    {
        err = RS_ERROR_PDM_ENC;
        otEXPECT(false);
    }

    pdm_PortContext->pEncryptionKey = encKey;
    pdm_PortContext->config_flags   = config_flags;

    status = PDM_SetEncryption((const PDM_portConfig_t *)pdm_PortContext);
    otEXPECT_ACTION((status == PDM_E_STATUS_OK), err = RS_ERROR_PDM_ENC);

exit:
    return err;
}

#endif /* PDM_ENCRYPTION */

#if ENABLE_STORAGE_DYNAMIC_MEMORY

static void HandleError(ramBufferDescriptor **buffer)
{
    if (*buffer != NULL)
    {
        otPlatFree(*buffer);
        *buffer = NULL;
    }
}

static bool_t doesDataExist(uint16_t id, ramBufferDescriptor * handle)
{
    bool_t res = FALSE;

    /* Check if dataset is present and get its size */
    if (PDM_bDoesDataExist(id, &handle->header.length))
    {
        res = TRUE;
        while (handle->header.length > handle->header.maxLength)
        {
            // increase size until NVM data fits
            handle->header.maxLength += kRamBufferReallocSize;
        }
    }

    return res;
}

static void loadData(uint16_t id, ramBufferDescriptor * handle)
{
    PDM_teStatus status =
        PDM_eReadDataFromRecord(id, handle->buffer, handle->header.maxLength, &handle->header.length);
    if ((PDM_E_STATUS_OK != status) || (handle->header.length > handle->header.maxLength))
    {
        handle->header.length = 0;
        PDM_vDeleteDataRecord(id);
    }
}

ramBufferDescriptor *getRamBuffer(uint16_t nvmId, uint16_t initialSize)
{
    rsError              err              = RS_ERROR_NONE;
    ramBufferDescriptor *ramDescr         = NULL;
    bool_t               bLoadDataFromNvm = FALSE;

    ramDescr = (ramBufferDescriptor *)otPlatCAlloc(1, kRamDescSize);
    otEXPECT_ACTION(ramDescr != NULL, HandleError(&ramDescr));

    ramDescr->header.maxLength = initialSize;
#if PDM_SAVE_IDLE
    ramDescr->header.mutexHandle = OSA_MutexCreate();
    otEXPECT_ACTION(ramDescr->header.mutexHandle != NULL, HandleError(&ramDescr));
#endif

    bLoadDataFromNvm = doesDataExist(nvmId, ramDescr);
    otEXPECT_ACTION(ramDescr->header.maxLength <= kRamBufferMaxAllocSize, HandleError(&ramDescr));

#if PDM_ENCRYPTION
#if PDM_SAVE_IDLE
    /* Don't allocate staging buffer, use in-place encryption.
     * Don't pass any encryption key, use the EFUSE key.
     */
    err = initPdmEncContext(&pdm_PortContext, NULL, 0, NULL, PDM_CNF_ENC_ENABLED | PDM_CNF_ENC_TMP_BUFF);
#else
    /* Allocate staging buffer.
     * Don't pass any encryption key, use the EFUSE key.
     */
    err = initPdmEncContext(&pdm_PortContext, NULL, ramDescr->header.maxLength, NULL, PDM_CNF_ENC_ENABLED);
#endif
    otEXPECT_ACTION(err == RS_ERROR_NONE, HandleError(&ramDescr));
#endif

    ramDescr->buffer = (uint8_t *)otPlatCAlloc(1, ramDescr->header.maxLength);
    otEXPECT_ACTION(ramDescr->buffer != NULL, HandleError(&ramDescr));

    if (bLoadDataFromNvm)
    {
        loadData(nvmId, ramDescr);
    }

exit:
    return ramDescr;
}

rsError ramStorageResize(ramBufferDescriptor *pBuffer, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    rsError        err            = RS_ERROR_NONE;
    uint16_t       allocSize      = pBuffer->header.maxLength;
    const uint16_t newBlockLength = sizeof(struct settingsBlock) + aValueLength;
    uint8_t       *ptr            = NULL;

    otEXPECT_ACTION((NULL != pBuffer), err = RS_ERROR_NO_BUFS);

    if (allocSize < pBuffer->header.length + newBlockLength)
    {
        while ((allocSize < pBuffer->header.length + newBlockLength))
        {
            /* Need to realocate the memory buffer, increase size by kRamBufferReallocSize until NVM data fits */
            allocSize += kRamBufferReallocSize;
        }

        if (allocSize <= kRamBufferMaxAllocSize)
        {
            ptr = (uint8_t *)otPlatRealloc((void *)pBuffer->buffer, allocSize);
            otEXPECT_ACTION((NULL != ptr), err = RS_ERROR_NO_BUFS);
            pBuffer->buffer           = ptr;
            pBuffer->header.maxLength = allocSize;

#if PDM_ENCRYPTION && !PDM_SAVE_IDLE
            err = stagingBufferResize(&pdm_PortContext, allocSize);
            otEXPECT(err == RS_ERROR_NONE);
#endif
        }
        else
        {
            err = RS_ERROR_NO_BUFS;
        }
    }

exit:
    return err;
}

#else
ramBufferDescriptor *getRamBuffer(uint16_t nvmId, uint16_t initialSize)
{
    OT_UNUSED_VARIABLE(initialSize);
    rsError              err       = RS_ERROR_NONE;
    ramBufferDescriptor *ramDescr  = (ramBufferDescriptor *)&sPdmBuffer;
    uint16_t             bytesRead = 0;

    ramDescr->header.maxLength   = PDM_BUFFER_SIZE - kRamDescSize;
    ramDescr->buffer             = (uint8_t *)&sPdmBuffer[kRamDescSize];
#if PDM_SAVE_IDLE
    ramDescr->header.mutexHandle = OSA_MutexCreate();
    otEXPECT_ACTION((NULL != ramDescr->header.mutexHandle), ramDescr = NULL);
#endif

#if PDM_ENCRYPTION
    /* Use static staging buffer for encryption result.
     * Don't pass any encryption key, use the EFUSE key.
     */
    err = initPdmEncContext(&pdm_PortContext, sPdmStagingBuffer, PDM_BUFFER_SIZE, NULL, PDM_CNF_ENC_ENABLED);
    otEXPECT(err == RS_ERROR_NONE);
#endif

    if (PDM_bDoesDataExist(nvmId, &ramDescr->header.length))
    {
        loadData(nvmId, ramDescr);
    }

exit:
    return ramDescr;
}
#endif

#if PDM_SAVE_IDLE

uint8_t u8IncrementQueuePtr(uint8_t u8CurrentValue)
{
    uint8_t u8IncrementedPtr;

    u8IncrementedPtr = u8CurrentValue + 1;
    if (u8IncrementedPtr == MAX_QUEUE_SIZE)
    {
        /* Wrap pointer back to start */
        u8IncrementedPtr = 0;
    }

    return u8IncrementedPtr;
}

PDM_teStatus FS_eSaveRecordDataInIdleTask(uint16_t u16IdValue, ramBufferDescriptor *pvDataBuffer)
{
    tsQueueEntry *psQueueEntry;
    PDM_teStatus  status = PDM_E_STATUS_OK;

#if defined(USE_RTOS) && (USE_RTOS == 1)
    OSA_InterruptDisable();
    if (asQueueMutex == NULL)
    {
        asQueueMutex = OSA_MutexCreate();
        if (asQueueMutex == NULL)
        {
            status = PDM_E_STATUS_NOT_SAVED;
        }
    }
    OSA_InterruptEnable();

    if (status != PDM_E_STATUS_OK)
    {
        return status;
    }
#endif

    if (!OSA_InIsrContext())
    {
        mutex_lock(asQueueMutex, osaWaitForever_c);
        asQueueMutexTaken = TRUE;
    }

    /* Instead of updating PDM immediately we queue request until later. If
     * queue is full, performs first write in queue synchronously, to avoid
     * data loss. Queue is implemented as a wrap-around with read and write
     * pointers, so adding or removing item from queue is quick
     */

    /* Look ahead in Queue to find if the same u16IdValue is already engaged */
    uint8_t u8QueuePtr = u8QueueReadPtr;
    bool_t  already_in = FALSE;
    while (u8QueuePtr != u8QueueWritePtr)
    {
        psQueueEntry = &asQueue[u8QueuePtr];
        if (psQueueEntry->u16IdValue == u16IdValue)
        {
            already_in = TRUE;
            break;
        }
        u8QueuePtr = u8IncrementQueuePtr(u8QueuePtr);
    }
    if (already_in == FALSE)
    {
        /* Write new entry to queue */
        psQueueEntry = &asQueue[u8QueueWritePtr];

        psQueueEntry->u16IdValue   = u16IdValue;
        psQueueEntry->pvDataBuffer = pvDataBuffer;

        /* Update write pointer */
        u8QueueWritePtr = u8IncrementQueuePtr(u8QueueWritePtr);
    }

    if (!OSA_InIsrContext())
    {
        asQueueMutexTaken = FALSE;
        mutex_unlock(asQueueMutex);
    }

    return status;
}

/* Saving data synchronously uses a static buffer to copy
 * PDM_PAGE_SIZE bytes from a RAM buffer. Chunks of PDM_PAGE_SIZE
 * bytes are saved at different PDM ids, starting from the base PDM
 * id used in the RAM buffer entry. The PDM id user (e.g. Matter) should
 * take into account this limitation and choose the PDM ids accordingly, to
 * avoid the possibility of overwriting data.
 */
static void FS_SaveRecordData(tsQueueEntry * entry)
{
    ramBufferDescriptor *handle = entry->pvDataBuffer;
    uint16_t             length = handle->header.length;
    uint8_t               pages = (length / PDM_PAGE_SIZE) + 1;

    for (uint8_t i = 0; i < segments; i++)
    {
        PDM_teStatus status = PDM_E_STATUS_INTERNAL_ERROR;
        uint16_t size = (length < PDM_PAGE_SIZE) ? length : PDM_PAGE_SIZE;

        if (osaStatus_Success == mutex_lock(handle->header.mutexHandle, 0))
        {
            memcpy(sPageBuffer, handle->buffer + i*PDM_PAGE_SIZE, size);
            mutex_unlock(handle->header.mutexHandle);

            status = PDM_eSaveRecordData(entry->u16IdValue + i, sPageBuffer, size);
        }

        if (status != PDM_E_STATUS_OK)
        {
            FS_eSaveRecordDataInIdleTask(entry->u16IdValue, handle);
            break;
        }

        length -= size;
    }
}

void FS_vIdleTask(uint8_t u8WritesAllowed)
{
    tsQueueEntry currentEntry;

    if (u8WritesAllowed > MAX_QUEUE_SIZE)
        u8WritesAllowed = MAX_QUEUE_SIZE;

    while ((u8QueueReadPtr != u8QueueWritePtr) && (u8WritesAllowed > 0))
    {
        mutex_lock(asQueueMutex, osaWaitForever_c);
        asQueueMutexTaken = TRUE;
        memcpy(&currentEntry, &asQueue[u8QueueReadPtr], sizeof(tsQueueEntry));
        u8QueueReadPtr    = u8IncrementQueuePtr(u8QueueReadPtr);
        asQueueMutexTaken = FALSE;
        mutex_unlock(asQueueMutex);

        FS_SaveRecordData(&currentEntry);

        u8WritesAllowed--;
    }
}

bool_t idleMutexIsTaken()
{
    return asQueueMutexTaken;
}

#endif /* PDM_SAVE_IDLE */
