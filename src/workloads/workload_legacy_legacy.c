/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include "workload_interface.h"

#define LEGACY_DEFAULT_BLOCK 512
#define LEGACY_FILE_SIZE (50 * 1024 * 1024) /* 50MB */

struct LegacyData
{
    char path[MAX_PATH_LEN];
    uint32 block_size;
};

static BOOL Setup_Legacy(const char *path, uint32 block_size, void **data)
{
    struct LegacyData *ld = IExec->AllocVecTags(sizeof(struct LegacyData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!ld)
        return FALSE;

    snprintf(ld->path, sizeof(ld->path), "%s", path);
    ld->block_size = block_size ? block_size : LEGACY_DEFAULT_BLOCK;
    *data = ld;
    return TRUE;
}

static BOOL Run_Legacy(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct LegacyData *ld = (struct LegacyData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    snprintf(temp_file, sizeof(temp_file), "%sbench_legacy.tmp", ld->path);
    total_bytes = WriteDummyFile(temp_file, LEGACY_FILE_SIZE, ld->block_size);
    IDOS->Delete(temp_file);

    *bytes_processed = total_bytes;
    *op_count = (ld->block_size > 0) ? (total_bytes / ld->block_size) : 1;
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
    *block_size = LEGACY_DEFAULT_BLOCK;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Legacy = {.type = TEST_LEGACY,
                                              .name = "Legacy",
                                              .description = "Old standard: 50MB file with 512B chunks",
                                              .Setup = Setup_Legacy,
                                              .Run = Run_Legacy,
                                              .Cleanup = Cleanup_Legacy,
                                              .GetDefaultSettings = GetDefaultSettings_Legacy};
