/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "engine_internal.h"
#include "workload_interface.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RandomData
{
    char path[256];
    char file_path[512];
    BPTR file;
    uint8 *buffer;
    uint32 file_size;
    uint32 num_ios;
};

static BOOL Setup_Random4K(const char *path, uint32 block_size, void **data)
{
    struct RandomData *rd = IExec->AllocVecTags(sizeof(struct RandomData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!rd)
        return FALSE;

    memset(rd, 0, sizeof(struct RandomData));
    strncpy(rd->path, path, sizeof(rd->path) - 1);
    rd->file_size = 64 * 1024 * 1024; /* 64MB data set */
    rd->num_ios = 4096;               /* Perform 4096 random I/Os per pass */

    /* If we are on RAM:, use a smaller size */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        rd->file_size = 8 * 1024 * 1024;
        rd->num_ios = 1024;
    }

    snprintf(rd->file_path, sizeof(rd->file_path), "%sbench_random.tmp", path);

    /* Pre-allocate and fill file */
    if (WriteDummyFile(rd->file_path, rd->file_size, 128 * 1024) == 0) {
        IExec->FreeVec(rd);
        return FALSE;
    }

    rd->file = IDOS->Open(rd->file_path, MODE_OLDFILE);
    if (!rd->file) {
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
        return FALSE;
    }

    rd->buffer = IExec->AllocVecTags(4096, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!rd->buffer) {
        IDOS->Close(rd->file);
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
        return FALSE;
    }
    memset(rd->buffer, 0x55, 4096);

    *data = rd;
    return TRUE;
}

static BOOL Run_Random4K(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct RandomData *rd = (struct RandomData *)data;
    uint32 total_bytes = 0;
    uint32 max_offset = rd->file_size - 4096;

    for (uint32 i = 0; i < rd->num_ios; i++) {
        uint32 offset = (uint32)rand() % max_offset;
        /* Align to 512-byte boundary for realistic disk performance */
        offset &= ~511;

        if (IDOS->ChangeFilePosition(rd->file, offset, OFFSET_BEGINNING)) {
            if (IDOS->Write(rd->file, rd->buffer, 4096) == 4096) {
                total_bytes += 4096;
            }
        }
    }

    *bytes_processed = total_bytes;
    *op_count = rd->num_ios;
    return (total_bytes > 0);
}

static void Cleanup_Random4K(void *data)
{
    if (data) {
        struct RandomData *rd = (struct RandomData *)data;
        if (rd->file)
            IDOS->Close(rd->file);
        if (rd->buffer)
            IExec->FreeVec(rd->buffer);
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
    }
}

static void GetDefaultSettings_Random4K(uint32 *block_size, uint32 *passes)
{
    *block_size = 4096;
    *passes = 3;
}

const BenchWorkload Workload_Random4K = {.type = TEST_RANDOM_4K,
                                         .name = "Random 4K I/O",
                                         .description = "Seek performance: 4096 random 4KB writes",
                                         .Setup = Setup_Random4K,
                                         .Run = Run_Random4K,
                                         .Cleanup = Cleanup_Random4K,
                                         .GetDefaultSettings = GetDefaultSettings_Random4K};
