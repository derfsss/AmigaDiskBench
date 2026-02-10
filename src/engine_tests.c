/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (C) 2026 Team Derfs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "engine_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void GetMicroTime(struct TimeVal *tv)
{
    if (IBenchTimer) {
        IBenchTimer->GetSysTime(tv);
    }
}

float GetDuration(struct TimeVal *start, struct TimeVal *end)
{
    double s = (double)start->Seconds + (double)start->Microseconds / 1000000.0;
    double e = (double)end->Seconds + (double)end->Microseconds / 1000000.0;
    return (float)(e - s);
}

BOOL RunSingleBenchmark(BenchTestType type, const char *target_path, uint32 block_size, BenchResult *out_result)
{
    struct TimeVal start_tv, end_tv;
    char temp_file[256];
    uint32 total_bytes = 0;
    uint32 op_count = 0;

    GetMicroTime(&start_tv);

    switch (type) {
    case TEST_SPRINTER:
        for (int i = 0; i < 100; i++) {
            snprintf(temp_file, sizeof(temp_file), "%sbench_sprinter_%d.tmp", target_path, i);
            total_bytes += WriteDummyFile(temp_file, 4096, block_size ? block_size : 4096);
            IDOS->Delete(temp_file);
            op_count += 2;
        }
        break;

    case TEST_HEAVY_LIFTER:
        snprintf(temp_file, sizeof(temp_file), "%sbench_heavy.tmp", target_path);
        total_bytes = WriteDummyFile(temp_file, 50 * 1024 * 1024, block_size ? block_size : (128 * 1024));
        IDOS->Delete(temp_file);
        op_count = 1;
        break;

    case TEST_LEGACY:
        snprintf(temp_file, sizeof(temp_file), "%sbench_legacy.tmp", target_path);
        total_bytes = WriteDummyFile(temp_file, 50 * 1024 * 1024, block_size ? block_size : 512);
        IDOS->Delete(temp_file);
        op_count = 1;
        break;

    case TEST_DAILY_GRIND:
        srand(FIXED_SEED);
        for (int i = 0; i < 45; i++) {
            uint32 size, chunk;
            if (i < 5)
                size = (2 + (rand() % 9)) * 1024 * 1024;
            else
                size = (1 + (rand() % 64)) * 1024;
            chunk = (512 << (rand() % 6));
            snprintf(temp_file, sizeof(temp_file), "%sbench_grind_%d.tmp", target_path, i);
            total_bytes += WriteDummyFile(temp_file, size, chunk);
            IDOS->Delete(temp_file);
            op_count += 2;
        }
        break;

    default:
        break;
    }

    GetMicroTime(&end_tv);

    out_result->duration_secs = GetDuration(&start_tv, &end_tv);
    out_result->total_bytes = total_bytes;
    if (out_result->duration_secs > 0) {
        out_result->mb_per_sec = ((float)total_bytes / (1024.0f * 1024.0f)) / out_result->duration_secs;
        out_result->iops = (uint32)((float)op_count / out_result->duration_secs);
    }
    return (total_bytes > 0);
}
