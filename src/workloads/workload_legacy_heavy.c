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
 * LIABILITY, WHETHER IN AN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "engine_internal.h"
#include "workload_interface.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

struct HeavyData
{
    char path[256];
    uint32 block_size;
};

static BOOL Setup_Heavy(const char *path, uint32 block_size, void **data)
{
    struct HeavyData *hd = IExec->AllocVecTags(sizeof(struct HeavyData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!hd)
        return FALSE;

    strncpy(hd->path, path, sizeof(hd->path) - 1);
    hd->block_size = block_size ? block_size : (128 * 1024);
    *data = hd;
    return TRUE;
}

static BOOL Run_Heavy(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct HeavyData *hd = (struct HeavyData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    snprintf(temp_file, sizeof(temp_file), "%sbench_heavy.tmp", hd->path);
    total_bytes = WriteDummyFile(temp_file, 50 * 1024 * 1024, hd->block_size);
    IDOS->Delete(temp_file);

    *bytes_processed = total_bytes;
    *op_count = 1;
    return (total_bytes > 0);
}

static void Cleanup_Heavy(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Heavy(uint32 *block_size, uint32 *passes)
{
    *block_size = 128 * 1024;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Heavy = {.type = TEST_HEAVY_LIFTER,
                                             .name = "Heavy Lifter (Legacy)",
                                             .description = "Throughput: 50MB file with 128KB chunks",
                                             .Setup = Setup_Heavy,
                                             .Run = Run_Heavy,
                                             .Cleanup = Cleanup_Heavy,
                                             .GetDefaultSettings = GetDefaultSettings_Heavy};
