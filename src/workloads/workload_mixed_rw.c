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

#define MIXED_BLOCK_SIZE 4096
#define MIXED_FILE_SIZE (64 * 1024 * 1024)    /* 64MB data set */
#define MIXED_RAM_FILE_SIZE (8 * 1024 * 1024) /* 8MB for RAM: */
#define MIXED_NUM_OPS 2048
#define MIXED_RAM_NUM_OPS 512
#define MIXED_FILL_CHUNK (128 * 1024) /* 128KB fill chunk */
#define MIXED_READ_RATIO 70           /* 70% reads, 30% writes */
#define MIXED_SECTOR_ALIGN 511        /* 512-byte alignment mask */

struct MixedRWData
{
    char path[MAX_PATH_LEN];
    char file_path[MAX_PATH_LEN * 2];
    BPTR file;
    uint8 *buffer;
    uint32 file_size;
    uint32 num_ops;
};

static BOOL Setup_MixedRW(const char *path, uint32 block_size, void **data)
{
    struct MixedRWData *md =
        IExec->AllocVecTags(sizeof(struct MixedRWData), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (!md)
        return FALSE;

    snprintf(md->path, sizeof(md->path), "%s", path);
    md->file_size = MIXED_FILE_SIZE;
    md->num_ops = MIXED_NUM_OPS;

    /* If we are on RAM:, use a smaller size */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        md->file_size = MIXED_RAM_FILE_SIZE;
        md->num_ops = MIXED_RAM_NUM_OPS;
    }

    snprintf(md->file_path, sizeof(md->file_path), "%sbench_mixed_rw.tmp", path);

    /* Pre-allocate and fill file for read operations */
    if (WriteDummyFile(md->file_path, md->file_size, MIXED_FILL_CHUNK) == 0) {
        IExec->FreeVec(md);
        return FALSE;
    }

    md->file = IDOS->Open(md->file_path, MODE_READWRITE);
    if (!md->file) {
        IDOS->Delete(md->file_path);
        IExec->FreeVec(md);
        return FALSE;
    }

    md->buffer = IExec->AllocVecTags(MIXED_BLOCK_SIZE, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!md->buffer) {
        IDOS->Close(md->file);
        IDOS->Delete(md->file_path);
        IExec->FreeVec(md);
        return FALSE;
    }
    memset(md->buffer, 0xAA, MIXED_BLOCK_SIZE);

    *data = md;
    return TRUE;
}

static BOOL Run_MixedRW(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct MixedRWData *md = (struct MixedRWData *)data;
    uint32 total_bytes = 0;
    uint32 max_offset = md->file_size - MIXED_BLOCK_SIZE;

    for (uint32 i = 0; i < md->num_ops; i++) {
        uint32 offset = (uint32)rand() % max_offset;
        /* Align to 512-byte boundary for realistic disk performance */
        offset &= ~MIXED_SECTOR_ALIGN;

        /* 70% reads, 30% writes */
        BOOL is_read = ((rand() % 100) < MIXED_READ_RATIO);

        if (IDOS->ChangeFilePosition(md->file, offset, OFFSET_BEGINNING)) {
            if (is_read) {
                /* Read operation */
                int32 bytes_read = IDOS->Read(md->file, md->buffer, MIXED_BLOCK_SIZE);
                if (bytes_read > 0) {
                    total_bytes += bytes_read;
                }
            } else {
                /* Write operation */
                if (IDOS->Write(md->file, md->buffer, MIXED_BLOCK_SIZE) == MIXED_BLOCK_SIZE) {
                    total_bytes += MIXED_BLOCK_SIZE;
                }
            }
        }
    }

    *bytes_processed = total_bytes;
    *op_count = md->num_ops;
    return (total_bytes > 0);
}

static void Cleanup_MixedRW(void *data)
{
    if (data) {
        struct MixedRWData *md = (struct MixedRWData *)data;
        if (md->file)
            IDOS->Close(md->file);
        if (md->buffer)
            IExec->FreeVec(md->buffer);
        IDOS->Delete(md->file_path);
        IExec->FreeVec(md);
    }
}

static void GetDefaultSettings_MixedRW(uint32 *block_size, uint32 *passes)
{
    *block_size = MIXED_BLOCK_SIZE;
    *passes = 3;
}

const BenchWorkload Workload_MixedRW = {
    .type = TEST_MIXED_RW_70_30,
    .name = "Mixed R/W 70/30",
    .description = "Real-world: 2048 ops, 70% reads, 30% writes",
    .Setup = Setup_MixedRW,
    .Run = Run_MixedRW,
    .Cleanup = Cleanup_MixedRW,
    .GetDefaultSettings = GetDefaultSettings_MixedRW
};
