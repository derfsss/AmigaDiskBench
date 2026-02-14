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
#include <stdlib.h>

/*
 * Seed for "The Daily Grind" to ensure deterministic random behavior
 */
#define FIXED_SEED 1985

uint32 WriteDummyFile(const char *path, uint32 size, uint32 chunk_size)
{
    BPTR file = IDOS->Open(path, MODE_NEWFILE);
    if (!file)
        return 0;

    uint8 *buffer = IExec->AllocVecTags(chunk_size, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!buffer) {
        IDOS->Close(file);
        return 0;
    }

    /* Fill with non-zero data to avoid sparse file optimizations if any */
    memset(buffer, 0xAA, chunk_size);

    uint32 written = 0;
    while (written < size) {
        uint32 to_write = size - written;
        if (to_write > chunk_size)
            to_write = chunk_size;

        if (IDOS->Write(file, buffer, to_write) != (int32)to_write)
            break;
        written += to_write;
    }

    IExec->FreeVec(buffer);
    IDOS->Close(file);
    return written;
}

void CleanUpWorkloadArtifacts(const char *target_path)
{
    /* Helper to clean up any left-over tmp files if needed */
}
