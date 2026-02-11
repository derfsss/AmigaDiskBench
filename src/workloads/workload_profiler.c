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

struct ProfilerData
{
    char base_path[256];
    uint32 num_dirs;
    uint32 files_per_dir;
};

/**
 * Setup_Profiler
 *
 * Prepares the metadata stress test by determining the target path
 * and scaling the number of operations based on the volume type (e.g. RAM:).
 */
static BOOL Setup_Profiler(const char *path, uint32 block_size, void **data)
{
    struct ProfilerData *pd = IExec->AllocVecTags(sizeof(struct ProfilerData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!pd)
        return FALSE;

    /* Store the target path for metadata ops */
    strncpy(pd->base_path, path, sizeof(pd->base_path) - 1);
    pd->num_dirs = 50;      /* Default: 50 directories */
    pd->files_per_dir = 10; /* Default: 10 files per directory */

    /* If we are on RAM:, we scale down slightly to avoid filling memory */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        pd->num_dirs = 20;
    }

    *data = pd;
    return TRUE;
}

/**
 * Run_Profiler
 *
 * Executes the metadata stress test:
 * 1. Creates a series of directories.
 * 2. Creates multiple files in each directory.
 * 3. Renames every 2nd file.
 * 4. Deletes all created files and directories.
 */
static BOOL Run_Profiler(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct ProfilerData *pd = (struct ProfilerData *)data;
    uint32 total_ops = 0;
    char dir_path[1024];
    char file_path[2048];
    char rename_path[2048];

    /* 1. Directory & File Creation Loop */
    for (uint32 d = 0; d < pd->num_dirs; d++) {
        snprintf(dir_path, sizeof(dir_path), "%sprof_dir_%u/", pd->base_path, (unsigned int)d);
        BPTR lock = IDOS->CreateDir(dir_path);
        if (lock) {
            IDOS->UnLock(lock);
            total_ops++;

            for (uint32 f = 0; f < pd->files_per_dir; f++) {
                snprintf(file_path, sizeof(file_path), "%sfile_%u.tmp", dir_path, (unsigned int)f);
                BPTR fh = IDOS->Open(file_path, MODE_NEWFILE);
                if (fh) {
                    /* Write a small amount of metadata info */
                    IDOS->Write(fh, "metadata stress test", 20);
                    IDOS->Close(fh);
                    total_ops++;

                    /* Every 2nd file, perform a Rename operation */
                    if (f % 2 == 0) {
                        snprintf(rename_path, sizeof(rename_path), "%sfile_%u_renamed.tmp", dir_path, (unsigned int)f);
                        if (IDOS->Rename(file_path, rename_path)) {
                            total_ops++;
                        }
                    }
                }
            }
        }
    }

    /* 2. Cleanup Loop: Delete everything created.
     * We iterate again to ensure we don't skip directories even if some file
     * operations failed earlier.
     */
    for (uint32 d = 0; d < pd->num_dirs; d++) {
        snprintf(dir_path, sizeof(dir_path), "%sprof_dir_%u/", pd->base_path, (unsigned int)d);

        /* AmigaOS Delete requires that a directory be empty */
        for (uint32 f = 0; f < pd->files_per_dir; f++) {
            /* Try deleting both the original (if rename failed) and renamed variants */
            snprintf(file_path, sizeof(file_path), "%sfile_%u.tmp", dir_path, (unsigned int)f);
            IDOS->Delete(file_path);

            snprintf(rename_path, sizeof(rename_path), "%sfile_%u_renamed.tmp", dir_path, (unsigned int)f);
            IDOS->Delete(rename_path);
            total_ops++;
        }
        /* Finally remove the directory */
        IDOS->Delete(dir_path);
        total_ops++;
    }

    /* We return the total number of metadata operations as the primary metric */
    *bytes_processed = total_ops * 20;
    *op_count = total_ops;
    return (total_ops > 0);
}

/**
 * Cleanup_Profiler
 *
 * Frees the private data structure allocated during Setup.
 */
static void Cleanup_Profiler(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Profiler(uint32 *block_size, uint32 *passes)
{
    *block_size = 0; /* Not used */
    *passes = 2;
}

const BenchWorkload Workload_Profiler = {.type = TEST_PROFILER,
                                         .name = "Full System Profiler",
                                         .description = "Metadata Stress: Creates, Renames, Deletes 500+ files/dirs",
                                         .Setup = Setup_Profiler,
                                         .Run = Run_Profiler,
                                         .Cleanup = Cleanup_Profiler,
                                         .GetDefaultSettings = GetDefaultSettings_Profiler};
