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

const BenchWorkload Workload_Legacy_Legacy = {
    .type = TEST_LEGACY,
    .name = "Legacy",
    .description = "Old standard: 50MB file with 512B chunks",
    .detailed_info =
        "Legacy\n"
        "\n"
        "Writes a 50 MB file using very small 512-byte chunks,\n"
        "simulating the I/O pattern of older Amiga applications that\n"
        "used minimal buffer sizes.\n"
        "\n"
        "  File size:      50 MB\n"
        "  Block size:     Fixed (512 bytes)\n"
        "  Metric:         MB/s (megabytes per second)\n"
        "  Default passes: 1\n"
        "\n"
        "With 512-byte writes, each I/O operation carries significant\n"
        "per-call overhead relative to the data transferred. This test\n"
        "highlights the cost of small-buffer I/O and is useful for\n"
        "comparing how different filesystems and device drivers handle\n"
        "high-frequency write requests.\n"
        "\n"
        "Good for: Worst-case throughput analysis.\n"
        "Simulates: Classic Amiga software, sector-level access.\n",
    .Setup = Setup_Legacy,
    .Run = Run_Legacy,
    .Cleanup = Cleanup_Legacy,
    .GetDefaultSettings = GetDefaultSettings_Legacy};
