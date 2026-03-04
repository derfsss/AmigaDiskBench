/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
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

const BenchWorkload Workload_Legacy_Grind = {
    .type = TEST_DAILY_GRIND,
    .name = "The Daily Grind (Legacy)",
    .description = "Pseudo-random mix of file sizes and chunk sizes",
    .detailed_info =
        "The Daily Grind (Legacy)\n"
        "\n"
        "A mixed-workload test that creates and deletes 45 files of\n"
        "varying sizes using randomly chosen chunk sizes, simulating\n"
        "a typical day of Amiga usage.\n"
        "\n"
        "  Iterations:     45 (5 large + 40 small files)\n"
        "  Large files:    2-11 MB each (random)\n"
        "  Small files:    1-65 KB each (random)\n"
        "  Chunk sizes:    Random from 512B, 1K, 2K, 4K, 8K, 16K, 32K\n"
        "  Metric:         Mixed (throughput + metadata)\n"
        "  Block size:     N/A (randomised internally)\n"
        "  Default passes: 1\n"
        "\n"
        "Uses a fixed random seed (1985) for reproducible results\n"
        "across runs. The mix of large sequential writes and many\n"
        "small file operations exercises both throughput and metadata\n"
        "paths simultaneously.\n"
        "\n"
        "Good for: Overall system performance snapshot.\n"
        "Simulates: General desktop usage, compiling, web browsing.\n",
    .Setup = Setup_Grind,
    .Run = Run_Grind,
    .Cleanup = Cleanup_Grind,
    .GetDefaultSettings = GetDefaultSettings_Grind};
