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

#ifndef ENGINE_H
#define ENGINE_H

#include "version.h"
#include <dos/dos.h>
#include <exec/types.h>

/* Test types */
typedef enum
{
    TEST_SPRINTER = 0, /* Small files / Metadata */
    TEST_HEAVY_LIFTER, /* Large file / Big chunks */
    TEST_LEGACY,       /* Large file / Small chunks */
    TEST_DAILY_GRIND,  /* Pseudo-random mix */
    TEST_COUNT
} BenchTestType;

/* Result structure for a single test run */
typedef struct
{
    char result_id[64]; /* Unique ID for retrieval e.g. "20231027103005_A1B2" */
    BenchTestType type;
    uint32 total_bytes;
    float duration_secs; /* Precision timing from timer.device */
    float mb_per_sec;
    uint32 iops; /* Operations per second */
    char volume_name[32];
    char fs_type[64];   /* NGFS, FFS, Hex, etc. */
    char timestamp[32]; /* Date/Time of test e.g. "2023-10-27 10:30" */

    /* Hardware Info */
    char device_name[64]; /* e.g. ahci.device */
    uint32 device_unit;
    char vendor[32];
    char product[64];

    /* App version tracking */
    char app_version[16];

    /* Additional metadata for CSV and details view */
    uint32 passes;
    uint32 block_size;

    /* Persistence and Detailed Metrics */
    BOOL use_trimmed_mean;
    float min_mbps; /* Min/Max among non-trimmed passes if trimming active */
    float max_mbps;
    float total_duration;    /* Cumulative duration across all passes */
    uint64 cumulative_bytes; /* Cumulative bytes across all passes */
    uint32 effective_passes; /* Passes actually included in average */
} BenchResult;

/* Engine functions */
BOOL InitEngine(void);
void CleanupEngine(void);

/* Run a specific test on a target path */
/* Returns TRUE on success, FALSE on error or user abort */
/* passes: number of times to run the test (for averaging) */
BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, BOOL use_trimmed_mean,
                  BenchResult *out_result);

/* Identify filesystem of a path */
void GetFileSystemInfo(const char *path, char *out_name, uint32 name_size);

/* Retrieve hardware details for a volume path */
void GetHardwareInfo(const char *path, BenchResult *result);

/* Persistence helpers */
BOOL SaveResultToCSV(const char *filename, BenchResult *result);

typedef struct
{
    float avg_mbps;
    float max_mbps;
    uint32 total_runs;
} TestStats;

typedef struct
{
    TestStats stats[TEST_COUNT];
    uint32 total_benchmarks;
} GlobalReport;

BOOL GenerateGlobalReport(const char *filename, GlobalReport *report);

#endif /* ENGINE_H */
