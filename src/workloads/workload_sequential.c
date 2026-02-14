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

struct SequentialData
{
    char path[256];
    uint32 block_size;
    uint32 file_size;
};

static BOOL Setup_Sequential(const char *path, uint32 block_size, void **data)
{
    struct SequentialData *sd = IExec->AllocVecTags(sizeof(struct SequentialData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!sd)
        return FALSE;

    snprintf(sd->path, sizeof(sd->path), "%s", path);
    sd->block_size = block_size ? block_size : (1024 * 1024);
    sd->file_size = 256 * 1024 * 1024; /* 256MB standard */

    /* If we are on RAM:, use a smaller size to avoid OOM */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        sd->file_size = 32 * 1024 * 1024;
    }

    *data = sd;
    return TRUE;
}

static BOOL Run_Sequential(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct SequentialData *sd = (struct SequentialData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    snprintf(temp_file, sizeof(temp_file), "%sbench_seq.tmp", sd->path);
    total_bytes = WriteDummyFile(temp_file, sd->file_size, sd->block_size);
    IDOS->Delete(temp_file);

    *bytes_processed = total_bytes;
    *op_count = 1;
    return (total_bytes > 0);
}

static void Cleanup_Sequential(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Sequential(uint32 *block_size, uint32 *passes)
{
    *block_size = 1024 * 1024;
    *passes = 3;
}

const BenchWorkload Workload_Sequential = {.type = TEST_SEQUENTIAL,
                                           .name = "Sequential I/O",
                                           .description = "Sustained throughput: 256MB file, 1MB chunks",
                                           .Setup = Setup_Sequential,
                                           .Run = Run_Sequential,
                                           .Cleanup = Cleanup_Sequential,
                                           .GetDefaultSettings = GetDefaultSettings_Sequential};
