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
#include <stdlib.h>

#define FIXED_SEED 1985

#define GRIND_ITERATIONS 45

struct GrindData
{
    char path[MAX_PATH_LEN];
};

static BOOL Setup_Grind(const char *path, uint32 block_size, void **data)
{
    struct GrindData *gd = IExec->AllocVecTags(sizeof(struct GrindData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!gd)
        return FALSE;

    snprintf(gd->path, sizeof(gd->path), "%s", path);
    *data = gd;
    return TRUE;
}

static BOOL Run_Grind(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct GrindData *gd = (struct GrindData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;
    uint32 total_ops = 0;

    srand(FIXED_SEED);
    for (int i = 0; i < GRIND_ITERATIONS; i++) {
        uint32 size, chunk;
        if (i < 5)
            size = (2 + (rand() % 9)) * 1024 * 1024;
        else
            size = (1 + (rand() % 64)) * 1024;
        chunk = (512 << (rand() % 6));
        snprintf(temp_file, sizeof(temp_file), "%sbench_grind_%d.tmp", gd->path, i);
        total_bytes += WriteDummyFile(temp_file, size, chunk);
        IDOS->Delete(temp_file);
        total_ops += 2;
    }

    *bytes_processed = total_bytes;
    *op_count = total_ops;
    return (total_bytes > 0);
}

static void Cleanup_Grind(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Grind(uint32 *block_size, uint32 *passes)
{
    *block_size = 0; /* Not applicable */
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Grind = {.type = TEST_DAILY_GRIND,
                                             .name = "The Daily Grind (Legacy)",
                                             .description = "Pseudo-random mix of file sizes and chunk sizes",
                                             .Setup = Setup_Grind,
                                             .Run = Run_Grind,
                                             .Cleanup = Cleanup_Grind,
                                             .GetDefaultSettings = GetDefaultSettings_Grind};
