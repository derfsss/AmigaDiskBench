/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include "workload_interface.h"

#define SPRINTER_DEFAULT_BLOCK 4096
#define SPRINTER_FILE_SIZE 4096
#define SPRINTER_FILE_COUNT 100

struct SprinterData
{
    char path[MAX_PATH_LEN];
    uint32 block_size;
};

static BOOL Setup_Sprinter(const char *path, uint32 block_size, void **data)
{
    struct SprinterData *sd = IExec->AllocVecTags(sizeof(struct SprinterData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!sd)
        return FALSE;

    snprintf(sd->path, sizeof(sd->path), "%s", path);
    sd->block_size = block_size ? block_size : SPRINTER_DEFAULT_BLOCK;
    *data = sd;
    return TRUE;
}

static BOOL Run_Sprinter(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct SprinterData *sd = (struct SprinterData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    for (int i = 0; i < SPRINTER_FILE_COUNT; i++) {
        snprintf(temp_file, sizeof(temp_file), "%sbench_sprinter_%d.tmp", sd->path, i);
        total_bytes += WriteDummyFile(temp_file, SPRINTER_FILE_SIZE, sd->block_size);
        IDOS->Delete(temp_file);
    }

    *bytes_processed = total_bytes;
    *op_count = SPRINTER_FILE_COUNT * 2; /* Write + Delete per file */
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
    *block_size = SPRINTER_DEFAULT_BLOCK;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Sprinter = {
    .type = TEST_SPRINTER,
    .name = "Sprinter (Legacy)",
    .description = "Metadata stress: 100x 4KB file creations/deletions",
    .detailed_info =
        "Sprinter (Legacy)\n"
        "\n"
        "A quick metadata performance test that measures how fast the\n"
        "filesystem can create and delete small files.\n"
        "\n"
        "  Operations:     100 file creates + 100 file deletes\n"
        "  File size:      4 KB each\n"
        "  Metric:         IOPS (operations per second)\n"
        "  Block size:     Fixed (4 KB)\n"
        "  Default passes: 1\n"
        "\n"
        "This test exercises the filesystem's metadata handling -- the\n"
        "overhead of allocating and freeing directory entries, updating\n"
        "the bitmap, and flushing journal data. It is particularly\n"
        "sensitive to filesystem type (FFS vs SFS vs JXFS) and whether\n"
        "the volume has directory caching enabled.\n"
        "\n"
        "Good for: Quick assessment of filesystem responsiveness.\n"
        "Simulates: Application installers, temp file activity.\n",
    .Setup = Setup_Sprinter,
    .Run = Run_Sprinter,
    .Cleanup = Cleanup_Sprinter,
    .GetDefaultSettings = GetDefaultSettings_Sprinter};
