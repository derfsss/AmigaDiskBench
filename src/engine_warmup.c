/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * Engine Warmup Module
 */

#include "engine_warmup.h"
#include "debug.h"
#include "engine_internal.h"
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdlib.h>
#include <string.h>

#define WARMUP_FILE_NAME "warmup.tmp"
#define WARMUP_SIZE (1024 * 1024) /* 1 MB */
#define BUFFER_SIZE 65536         /* 64 KB chunks */
#define WARMUP_THRESHOLD_SECS 5.0f

static char last_warmup_path[256] = "";
static struct TimeVal last_warmup_time = {0, 0};

void RunWarmup(const char *target_path)
{
    struct TimeVal current_time;
    GetMicroTime(&current_time);

    /* Check for recent warmup session to avoid redundancy */
    if (last_warmup_path[0] != '\0' && strncmp(last_warmup_path, target_path, sizeof(last_warmup_path)) == 0) {
        float delta = GetDuration(&last_warmup_time, &current_time);
        if (delta < WARMUP_THRESHOLD_SECS) {
            LOG_DEBUG("Warmup: Skipped (Recent warmup on '%s' %.2fs ago matches threshold < %.1fs)", target_path, delta,
                      WARMUP_THRESHOLD_SECS);
            return;
        }
    }

    LOG_DEBUG("Warmup: Starting for target '%s'...", target_path);

    /* Construct full path to warmup file */
    char full_path[256];
    strncpy(full_path, target_path, sizeof(full_path) - 1);

    /* Ensure path ends with a separator if needed (simple check) */
    size_t len = strlen(full_path);
    if (len > 0 && full_path[len - 1] != ':' && full_path[len - 1] != '/') {
        strncat(full_path, "/", sizeof(full_path) - len - 1);
    }
    strncat(full_path, WARMUP_FILE_NAME, sizeof(full_path) - strlen(full_path) - 1);

    /* Allocate buffer */
    void *buffer = IExec->AllocVecTags(BUFFER_SIZE, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!buffer) {
        LOG_DEBUG("Warmup: Failed to allocate buffer.");
        return;
    }

    /* Fill buffer with random data */
    uint32 *p = (uint32 *)buffer;
    for (int i = 0; i < BUFFER_SIZE / 4; i++) {
        p[i] = rand();
    }

    /* 1. WRITE Phase */
    BPTR file = IDOS->Open(full_path, MODE_NEWFILE);
    if (file) {
        uint32 bytes_written = 0;
        while (bytes_written < WARMUP_SIZE) {
            int32 res = IDOS->Write(file, buffer, BUFFER_SIZE);
            if (res == -1)
                break;
            bytes_written += res;
        }
        IDOS->Close(file);
    } else {
        LOG_DEBUG("Warmup: Failed to open file for writing: %s", full_path);
        IExec->FreeVec(buffer);
        return;
    }

    /* 2. READ Phase */
    file = IDOS->Open(full_path, MODE_OLDFILE);
    if (file) {
        uint32 bytes_read = 0;
        while (bytes_read < WARMUP_SIZE) {
            int32 res = IDOS->Read(file, buffer, BUFFER_SIZE);
            if (res == -1 || res == 0)
                break;
            bytes_read += res;
        }
        IDOS->Close(file);
    } else {
        LOG_DEBUG("Warmup: Failed to open file for reading: %s", full_path);
    }

    /* 3. CLEANUP Phase */
    int32 result = IDOS->Delete(full_path);
    if (result == 0) {
        LOG_DEBUG("Warmup: Warning - Failed to delete warmup file.");
    }

    IExec->FreeVec(buffer);

    /* Update Session Cache */
    strncpy(last_warmup_path, target_path, sizeof(last_warmup_path) - 1);
    last_warmup_path[sizeof(last_warmup_path) - 1] = '\0';
    GetMicroTime(&last_warmup_time);

    LOG_DEBUG("Warmup: Complete.");
}
