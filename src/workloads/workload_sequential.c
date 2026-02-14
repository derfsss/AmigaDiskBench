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

#define SEQ_DEFAULT_BLOCK (1024 * 1024)      /* 1MB default block */
#define SEQ_FILE_SIZE (256 * 1024 * 1024)    /* 256MB standard */
#define SEQ_RAM_FILE_SIZE (32 * 1024 * 1024) /* 32MB for RAM: */

struct SequentialData
{
    char path[MAX_PATH_LEN];
    uint32 block_size;
    uint32 file_size;
};

static BOOL Setup_Sequential(const char *path, uint32 block_size, void **data)
{
    struct SequentialData *sd = IExec->AllocVecTags(sizeof(struct SequentialData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!sd)
        return FALSE;

    snprintf(sd->path, sizeof(sd->path), "%s", path);
    sd->block_size = block_size ? block_size : SEQ_DEFAULT_BLOCK;
    sd->file_size = SEQ_FILE_SIZE;

    /* If we are on RAM:, use a smaller size to avoid OOM */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        sd->file_size = SEQ_RAM_FILE_SIZE;
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
    *block_size = SEQ_DEFAULT_BLOCK;
    *passes = 3;
}

const BenchWorkload Workload_Sequential = {.type = TEST_SEQUENTIAL,
                                           .name = "Sequential I/O",
                                           .description = "Sustained throughput: 256MB file, 1MB chunks",
                                           .Setup = Setup_Sequential,
                                           .Run = Run_Sequential,
                                           .Cleanup = Cleanup_Sequential,
                                           .GetDefaultSettings = GetDefaultSettings_Sequential};
