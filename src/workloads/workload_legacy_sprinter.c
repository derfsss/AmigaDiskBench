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

struct SprinterData
{
    char path[256];
    uint32 block_size;
};

static BOOL Setup_Sprinter(const char *path, uint32 block_size, void **data)
{
    struct SprinterData *sd = IExec->AllocVecTags(sizeof(struct SprinterData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!sd)
        return FALSE;

    snprintf(sd->path, sizeof(sd->path), "%s", path);
    sd->block_size = block_size ? block_size : 4096;
    *data = sd;
    return TRUE;
}

static BOOL Run_Sprinter(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct SprinterData *sd = (struct SprinterData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    for (int i = 0; i < 100; i++) {
        snprintf(temp_file, sizeof(temp_file), "%sbench_sprinter_%d.tmp", sd->path, i);
        total_bytes += WriteDummyFile(temp_file, 4096, sd->block_size);
        IDOS->Delete(temp_file);
    }

    *bytes_processed = total_bytes;
    *op_count = 200; /* 100 Write + 100 Delete */
    return (total_bytes > 0);
}

static void Cleanup_Sprinter(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Sprinter(uint32 *block_size, uint32 *passes)
{
    *block_size = 4096;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Sprinter = {.type = TEST_SPRINTER,
                                                .name = "Sprinter (Legacy)",
                                                .description = "Metadata stress: 100x 4KB file creations/deletions",
                                                .Setup = Setup_Sprinter,
                                                .Run = Run_Sprinter,
                                                .Cleanup = Cleanup_Sprinter,
                                                .GetDefaultSettings = GetDefaultSettings_Sprinter};
