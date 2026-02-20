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

#define SEQ_READ_DEFAULT_BLOCK (1024 * 1024)  /* 1MB default block */
#define SEQ_READ_FILE_SIZE (256 * 1024 * 1024)    /* 256MB standard */
#define SEQ_READ_RAM_FILE_SIZE (32 * 1024 * 1024) /* 32MB for RAM: */

struct SequentialReadData
{
    char path[MAX_PATH_LEN];
    char file_path[MAX_PATH_LEN * 2];
    uint32 block_size;
    uint32 file_size;
    BPTR file;
    uint8 *buffer;
};

static BOOL Setup_SequentialRead(const char *path, uint32 block_size, void **data)
{
    struct SequentialReadData *sd = IExec->AllocVecTags(sizeof(struct SequentialReadData),
                                                         AVT_Type, MEMF_SHARED,
                                                         AVT_ClearWithValue, 0,
                                                         TAG_DONE);
    if (!sd)
        return FALSE;

    snprintf(sd->path, sizeof(sd->path), "%s", path);
    sd->block_size = block_size ? block_size : SEQ_READ_DEFAULT_BLOCK;
    sd->file_size = SEQ_READ_FILE_SIZE;

    /* If we are on RAM:, use a smaller size to avoid OOM */
    if (strncasecmp(path, "RAM:", 4) == 0) {
        sd->file_size = SEQ_READ_RAM_FILE_SIZE;
    }

    snprintf(sd->file_path, sizeof(sd->file_path), "%sbench_seq_read.tmp", path);

    /* Pre-create file with data to read */
    if (WriteDummyFile(sd->file_path, sd->file_size, sd->block_size) == 0) {
        IExec->FreeVec(sd);
        return FALSE;
    }

    /* Open file for reading */
    sd->file = IDOS->Open(sd->file_path, MODE_OLDFILE);
    if (!sd->file) {
        IDOS->Delete(sd->file_path);
        IExec->FreeVec(sd);
        return FALSE;
    }

    /* Allocate read buffer */
    sd->buffer = IExec->AllocVecTags(sd->block_size, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!sd->buffer) {
        IDOS->Close(sd->file);
        IDOS->Delete(sd->file_path);
        IExec->FreeVec(sd);
        return FALSE;
    }

    *data = sd;
    return TRUE;
}

static BOOL Run_SequentialRead(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct SequentialReadData *sd = (struct SequentialReadData *)data;
    uint32 total_bytes = 0;
    uint32 remaining = sd->file_size;

    /* Seek to beginning of file */
    IDOS->ChangeFilePosition(sd->file, 0, OFFSET_BEGINNING);

    /* Sequential read through the entire file */
    while (remaining > 0) {
        uint32 to_read = (remaining < sd->block_size) ? remaining : sd->block_size;
        int32 bytes_read = IDOS->Read(sd->file, sd->buffer, to_read);

        if (bytes_read <= 0)
            break;

        total_bytes += bytes_read;
        remaining -= bytes_read;
    }

    *bytes_processed = total_bytes;
    *op_count = 1;
    return (total_bytes > 0);
}

static void Cleanup_SequentialRead(void *data)
{
    if (data) {
        struct SequentialReadData *sd = (struct SequentialReadData *)data;
        if (sd->file)
            IDOS->Close(sd->file);
        if (sd->buffer)
            IExec->FreeVec(sd->buffer);
        IDOS->Delete(sd->file_path);
        IExec->FreeVec(sd);
    }
}

static void GetDefaultSettings_SequentialRead(uint32 *block_size, uint32 *passes)
{
    *block_size = SEQ_READ_DEFAULT_BLOCK;
    *passes = 3;
}

const BenchWorkload Workload_SequentialRead = {
    .type = TEST_SEQUENTIAL_READ,
    .name = "Sequential Read I/O",
    .description = "Read throughput: 256MB file, 1MB chunks",
    .Setup = Setup_SequentialRead,
    .Run = Run_SequentialRead,
    .Cleanup = Cleanup_SequentialRead,
    .GetDefaultSettings = GetDefaultSettings_SequentialRead
};
