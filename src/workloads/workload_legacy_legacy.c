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
#include <string.h>

struct LegacyData
{
    char path[256];
    uint32 block_size;
};

static BOOL Setup_Legacy(const char *path, uint32 block_size, void **data)
{
    struct LegacyData *ld = IExec->AllocVecTags(sizeof(struct LegacyData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!ld)
        return FALSE;

    strncpy(ld->path, path, sizeof(ld->path) - 1);
    ld->block_size = block_size ? block_size : 512;
    *data = ld;
    return TRUE;
}

static BOOL Run_Legacy(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct LegacyData *ld = (struct LegacyData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    snprintf(temp_file, sizeof(temp_file), "%sbench_legacy.tmp", ld->path);
    total_bytes = WriteDummyFile(temp_file, 50 * 1024 * 1024, ld->block_size);
    IDOS->Delete(temp_file);

    *bytes_processed = total_bytes;
    *op_count = 1;
    return (total_bytes > 0);
}

static void Cleanup_Legacy(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Legacy(uint32 *block_size, uint32 *passes)
{
    *block_size = 512;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Legacy = {.type = TEST_LEGACY,
                                              .name = "Legacy",
                                              .description = "Old standard: 50MB file with 512B chunks",
                                              .Setup = Setup_Legacy,
                                              .Run = Run_Legacy,
                                              .Cleanup = Cleanup_Legacy,
                                              .GetDefaultSettings = GetDefaultSettings_Legacy};
