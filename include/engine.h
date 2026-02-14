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
/**
 * @brief Enumeration of available benchmark test types.
 */
typedef enum
{
    TEST_SPRINTER = 0, /**< Small files / Metadata performance */
    TEST_HEAVY_LIFTER, /**< Large file / Big chunk transfer */
    TEST_LEGACY,       /**< Large file / Small chunk transfer (Simulates older apps) */
    TEST_DAILY_GRIND,  /**< Pseudo-random mix of operations */
    TEST_SEQUENTIAL,   /**< Professional: Pure Sequential I/O */
    TEST_RANDOM_4K,    /**< Professional: Random 4K I/O */
    TEST_PROFILER,     /**< Professional: Filesystem Profiler (Metadata) */
    TEST_COUNT         /**< Total number of test types */
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

    /* Comparison data (non-persisted, calculated on load/run) */
    float prev_mbps;
    uint32 prev_iops;
    float diff_per;
    char prev_timestamp[32];
} BenchResult;

/* Separate time-series data for graphing (kept out of BenchResult to save ~8KB per history entry) */
typedef struct
{
    BenchSample samples[MAX_SAMPLES];
    uint32 sample_count;
} BenchSampleData;

/**
 * @brief Get the canonical CSV name for a test type (e.g. "Random4K").
 */
const char *TestTypeToString(BenchTestType type);

/**
 * @brief Get the display name for a test type (e.g. "Random 4K").
 */
const char *TestTypeToDisplayName(BenchTestType type);

/**
 * @brief Parse a test type from a string (exact or substring match).
 * @return The matching type, or TEST_COUNT if not found.
 */
BenchTestType StringToTestType(const char *name);

/* Engine functions */

/**
 * @brief Initialize the benchmark engine and required resources.
 *
 * @return TRUE if initialization was successful, FALSE otherwise.
 */
BOOL InitEngine(void);

/**
 * @brief Cleanup engine resources and free memory.
 */
void CleanupEngine(void);

/**
 * @brief Run a specified benchmark test on a target path.
 *
 * This function handles the setup, execution, and result collection for a benchmark job.
 *
 * @param type The type of benchmark test to run (e.g., TEST_SPRINTER).
 * @param target_path The filesystem path to test (e.g., "DH0:").
 * @param passes Number of times to run the test for averaging.
 * @param block_size Block size to use for I/O operations (in bytes).
 * @param use_trimmed_mean If TRUE, discard best/worst runs before averaging.
 * @param flush_cache If TRUE, attempt to clear OS buffers before running.
 * @param out_result Pointer to a BenchResult structure to store the results.
 * @param out_samples Optional pointer to a BenchSampleData structure for time-series data (may be NULL).
 * @return TRUE if the benchmark completed successfully, FALSE on error or abort.
 */
BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, BOOL use_trimmed_mean,
                  BOOL flush_cache, BenchResult *out_result, BenchSampleData *out_samples);

/**
 * @brief Identify the filesystem of a given path.
 *
 * @param path The path to check (e.g., "DH0:").
 * @param out_name Buffer to store the filesystem name (e.g., "NGF/00").
 * @param name_size Size of the output buffer.
 */
void GetFileSystemInfo(const char *path, char *out_name, uint32 name_size);

/**
 * @brief Retrieve hardware details for a volume path.
 *
 * Queries the device associated with the path (e.g., via SCSI_INQUIRY)
 * to obtain vendor, product, and version information.
 * Uses a caching mechanism to avoid redundant hardware queries.
 *
 * @param path The path to query.
 * @param result Pointer to the BenchResult structure to populate with hardware info.
 */
void GetHardwareInfo(const char *path, BenchResult *result);

/**
 * @brief Clear the internal hardware info cache.
 *
 * Invalidates all cached hardware information, forcing a re-query on the next access.
 * Useful when drives are changed or added.
 */
void ClearHardwareInfoCache(void);

/* Persistence helpers */

/**
 * @brief Save a benchmark result to a CSV file.
 *
 * Appends the result to the specified file in a standardized CSV format.
 *
 * @param filename Path to the CSV file.
 * @param result Pointer to the BenchResult to save.
 * @return TRUE on success, FALSE on failure.
 */
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

/**
 * @brief Generate a global summary report from the CSV history.
 *
 * Aggregates statistics for all test types found in the history file.
 *
 * @param filename Path to the source CSV file.
 * @param report Pointer to a GlobalReport structure to populate.
 * @return TRUE if the report was generated successfully, FALSE otherwise.
 */
BOOL GenerateGlobalReport(const char *filename, GlobalReport *report);

#endif /* ENGINE_H */
