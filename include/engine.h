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
    TEST_SEQUENTIAL,   /* Professional: Sequential I/O */
    TEST_RANDOM_4K,    /* Professional: Random 4K I/O */
    TEST_PROFILER,     /* Professional: Filesystem Profiler (Metadata) */
    TEST_COUNT
} BenchTestType;

/* Performance sample for graphing */
typedef struct
{
    float time_offset; /* Time since pass start */
    float value;       /* MB/s or IOPS at this point */
} BenchSample;

#define MAX_SAMPLES 1024
#define MAX_PASSES 20

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
    char serial_number[64];
    char firmware_rev[32];

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

    /* Time-series data for graphing */
    BenchSample samples[MAX_SAMPLES];
    uint32 sample_count;

    /* Comparison data (non-persisted, calculated on load/run) */
    float prev_mbps;
    uint32 prev_iops;
    float diff_per;
    char prev_timestamp[32];
} BenchResult;

/* Engine functions */
BOOL InitEngine(void);
void CleanupEngine(void);

/* Run a specific test on a target path */
/* Returns TRUE on success, FALSE on error or user abort */
/* passes: number of times to run the test (for averaging) */
BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, BOOL use_trimmed_mean,
                  BOOL flush_cache, BenchResult *out_result);

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
