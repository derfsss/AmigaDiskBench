/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
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
    *op_count = (sd->block_size > 0) ? (total_bytes / sd->block_size) : 1;
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

const BenchWorkload Workload_Sequential = {
    .type = TEST_SEQUENTIAL_WRITE,
    .name = "Sequential Write",
    .description = "Sustained throughput: 256MB file",
    .detailed_info =
        "Sequential Write\n"
        "\n"
        "Measures sustained sequential write throughput by writing a\n"
        "single large file from start to finish using a configurable\n"
        "block size.\n"
        "\n"
        "  File size:      256 MB (32 MB on RAM:)\n"
        "  Block size:     Configurable (default 1 MB)\n"
        "  Metric:         MB/s (megabytes per second)\n"
        "  Default passes: 3\n"
        "\n"
        "This test stresses the drive's ability to write large\n"
        "contiguous data streams. Higher block sizes reduce per-\n"
        "operation overhead and generally yield higher throughput.\n"
        "Results are comparable to real-world file copy operations.\n"
        "\n"
        "The block size can be changed via the Block Size chooser\n"
        "to compare performance at different I/O granularities.\n"
        "\n"
        "Good for: Peak write throughput measurement.\n"
        "Simulates: Large file copies, video rendering output.\n",
    .Setup = Setup_Sequential,
    .Run = Run_Sequential,
    .Cleanup = Cleanup_Sequential,
    .GetDefaultSettings = GetDefaultSettings_Sequential};
