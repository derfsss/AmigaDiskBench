/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include "workload_interface.h"
#include <stdlib.h>

#define RAND_READ_BLOCK_SIZE 4096
#define RAND_READ_FILE_SIZE (64 * 1024 * 1024)    /* 64MB data set */
#define RAND_READ_RAM_FILE_SIZE (8 * 1024 * 1024) /* 8MB for RAM: */
#define RAND_READ_NUM_IOS 4096
#define RAND_READ_RAM_NUM_IOS 1024
#define RAND_READ_FILL_CHUNK (128 * 1024) /* 128KB fill chunk */
#define RAND_READ_SECTOR_ALIGN 511        /* 512-byte alignment mask */

struct RandomReadData
{
    char path[MAX_PATH_LEN];
    char file_path[MAX_PATH_LEN * 2];
    BPTR file;
    uint8 *buffer;
    uint32 file_size;
    uint32 num_ios;
    uint32 block_size;
};

static BOOL Setup_Random4KRead(const char *path, uint32 block_size, void **data)
{
    struct RandomReadData *rd =
        IExec->AllocVecTags(sizeof(struct RandomReadData), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (!rd)
        return FALSE;

    snprintf(rd->path, sizeof(rd->path), "%s", path);
    rd->file_size = RAND_READ_FILE_SIZE;
    rd->num_ios = RAND_READ_NUM_IOS;
    rd->block_size = block_size ? block_size : RAND_READ_BLOCK_SIZE;

    /* If we are on RAM:, use a smaller size */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        rd->file_size = RAND_READ_RAM_FILE_SIZE;
        rd->num_ios = RAND_READ_RAM_NUM_IOS;
    }

    snprintf(rd->file_path, sizeof(rd->file_path), "%sbench_random_read.tmp", path);

    /* Pre-allocate and fill file */
    if (WriteDummyFile(rd->file_path, rd->file_size, RAND_READ_FILL_CHUNK) == 0) {
        IExec->FreeVec(rd);
        return FALSE;
    }

    rd->file = IDOS->Open(rd->file_path, MODE_OLDFILE);
    if (!rd->file) {
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
        return FALSE;
    }

    rd->buffer = IExec->AllocVecTags(rd->block_size, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!rd->buffer) {
        IDOS->Close(rd->file);
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
        return FALSE;
    }

    *data = rd;
    return TRUE;
}

static BOOL Run_Random4KRead(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct RandomReadData *rd = (struct RandomReadData *)data;
    uint64 total_bytes = 0;
    /* Guard against underflow when block_size >= file_size */
    if (rd->block_size >= rd->file_size) {
        *bytes_processed = 0;
        *op_count = 0;
        return FALSE;
    }
    uint32 max_offset = rd->file_size - rd->block_size;

    for (uint32 i = 0; i < rd->num_ios; i++) {
        uint32 offset = (uint32)rand() % max_offset;
        /* Align to 512-byte boundary for realistic disk performance */
        offset &= ~RAND_READ_SECTOR_ALIGN;

        /* ChangeFilePosition returns the old position, not a success flag.
         * A return of -1 indicates error; any other value (including 0) is valid. */
        if (IDOS->ChangeFilePosition(rd->file, offset, OFFSET_BEGINNING) != -1) {
            int32 bytes_read = IDOS->Read(rd->file, rd->buffer, rd->block_size);
            if (bytes_read > 0) {
                total_bytes += bytes_read;
            }
        }
    }

    /* Cap to uint32 max to avoid overflow — total_bytes can exceed 4GB with large block sizes */
    *bytes_processed = (total_bytes > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32)total_bytes;
    *op_count = rd->num_ios;
    return (total_bytes > 0);
}

static void Cleanup_Random4KRead(void *data)
{
    if (data) {
        struct RandomReadData *rd = (struct RandomReadData *)data;
        if (rd->file)
            IDOS->Close(rd->file);
        if (rd->buffer)
            IExec->FreeVec(rd->buffer);
        IDOS->Delete(rd->file_path);
        IExec->FreeVec(rd);
    }
}

static void GetDefaultSettings_Random4KRead(uint32 *block_size, uint32 *passes)
{
    *block_size = RAND_READ_BLOCK_SIZE;
    *passes = 3;
}

const BenchWorkload Workload_Random4KRead = {
    .type = TEST_RANDOM_READ,
    .name = "Random Read I/O",
    .description = "Seek performance: Random reads (User Block Size)",
    .detailed_info =
        "Random Read I/O\n"
        "\n"
        "Measures random read performance by seeking to pseudo-random\n"
        "positions within a pre-created file and reading data blocks.\n"
        "\n"
        "  File size:      64 MB (8 MB on RAM:)\n"
        "  Operations:     4096 random reads (1024 on RAM:)\n"
        "  Block size:     Configurable (default 4 KB)\n"
        "  Seek alignment: 512-byte sectors\n"
        "  Metric:         IOPS (I/O operations per second)\n"
        "  Default passes: 3\n"
        "\n"
        "Random reads are the bread and butter of application loading\n"
        "and multitasking. When the OS needs to load libraries, read\n"
        "configuration files, or access scattered data, it performs\n"
        "random reads. This test directly measures how responsive\n"
        "the drive feels during everyday use.\n"
        "\n"
        "Good for: Predicting real-world system responsiveness.\n"
        "Simulates: Application startup, library loading.\n",
    .Setup = Setup_Random4KRead,
    .Run = Run_Random4KRead,
    .Cleanup = Cleanup_Random4KRead,
    .GetDefaultSettings = GetDefaultSettings_Random4KRead};
