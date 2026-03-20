/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include <stdlib.h>

/*
 * Writes a dummy file of the specified size using the given chunk size.
 * Returns the number of bytes actually written, or 0 on failure.
 */
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
