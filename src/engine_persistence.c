/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include <stdlib.h>

BOOL SaveResultToCSV(const char *filename, BenchResult *result)
{
    LOG_DEBUG("SaveResultToCSV: Attempting to save to '%s'", filename);
    BPTR file = IDOS->FOpen(filename, MODE_OLDFILE, 0);

    if (!file) {
        LOG_DEBUG("SaveResultToCSV: Creating new file '%s'", filename);
        file = IDOS->FOpen(filename, MODE_NEWFILE, 0);
        if (file) {
            IDOS->FPuts(file, "ID,DateTime,Type,Volume,FS,MB/"
                              "s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize,Trimmed,Min,Max,Duration,TotalBytes,"
                              "Vendor,Product,Firmware,Serial\n");
        }
    } else {
        LOG_DEBUG("SaveResultToCSV: Appending to existing file");
        IDOS->ChangeFilePosition(file, 0, OFFSET_END);
    }

    if (file) {
        char line[2048];
        char *ptr = line;
        size_t remaining = sizeof(line);
        int written;
        BOOL overflow = FALSE;

        /* Helper macro to append to buffer with overflow detection */
#define APPEND_CSV(fmt, ...)                                                                                           \
    do {                                                                                                               \
        written = snprintf(ptr, remaining, fmt, __VA_ARGS__);                                                          \
        if (written > 0 && written < (int)remaining) {                                                                 \
            ptr += written;                                                                                            \
            remaining -= written;                                                                                      \
        } else {                                                                                                       \
            overflow = TRUE;                                                                                           \
        }                                                                                                              \
    } while (0)

        /* Construct CSV line incrementally to avoid massive varargs crash */

        // 1. ID, Timestamp, Type, Volume, FS
        APPEND_CSV("%s,%s,%s,%s,%s", result->result_id, result->timestamp, TestTypeToString(result->type),
                   result->volume_name, result->fs_type);

        // 2. Metrics (MB/s, IOPS)
        APPEND_CSV(",%.2f,%lu", result->mb_per_sec, (unsigned long)result->iops);

        // 3. Device Info (Name, Unit, Version)
        APPEND_CSV(",%s,%u,%s", result->device_name, (unsigned int)result->device_unit, result->app_version);

        // 4. Test Settings (Passes, BlockSize, AveragingMethod)
        const char *avg_method_str = (result->averaging_method == AVERAGE_TRIMMED_MEAN) ? "TrimmedMean" :
                                     (result->averaging_method == AVERAGE_MEDIAN) ? "Median" : "AllPasses";
        APPEND_CSV(",%u,%u,%s", (unsigned int)result->passes, (unsigned int)result->block_size,
                   avg_method_str);

        // 5. Detailed Stats (Min, Max, Duration)
        APPEND_CSV(",%.2f,%.2f,%.2f", result->min_mbps, result->max_mbps, result->total_duration);

        // 6. Cumulative Bytes (uint64)
        APPEND_CSV(",%llu", (unsigned long long)result->cumulative_bytes);

        // 7. Hardware Details (Vendor, Product, Firmware, Serial)
        APPEND_CSV(",%s,%s,%s,%s\n", result->vendor, result->product, result->firmware_rev, result->serial_number);

#undef APPEND_CSV

        if (overflow) {
            LOG_DEBUG("SaveResultToCSV: CSV line buffer overflow (>2048 bytes) for result %s — record skipped",
                      result->result_id);
            LogUser("WARNING: CSV record too large to save (result %s)", result->result_id);
        } else {
            IDOS->FPuts(file, line);
        }
        IDOS->FClose(file);
        return !overflow;
    }

    return FALSE;
}

BOOL GenerateGlobalReport(const char *filename, GlobalReport *report)
{
    BPTR file = IDOS->FOpen(filename, MODE_OLDFILE, 0);
    if (!file)
        return FALSE;

    memset(report, 0, sizeof(GlobalReport));
    char line[512];
    BOOL first = TRUE;

    while (IDOS->FGets(file, line, sizeof(line))) {
        if (first) {
            first = FALSE;
            continue;
        } /* Skip header */

        char id[64], timestamp[32], type[64], disk[32], fs[64], mbs_str[32], iops_str[32];
        char device[64], unit[32], app_ver[32], passes_str[16], bs_str[32];
        int fields = sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,\r\n]", id,
                            timestamp, type, disk, fs, mbs_str, iops_str, device, unit, app_ver, passes_str, bs_str);

        if (fields < 7) {
            LOG_DEBUG("GenerateGlobalReport: Skipping malformed line (fields=%d): %s", fields, line);
            continue;
        }

        float mbs = 0.0f;
        char *final_type = type;

        if (fields >= 12) {
            /* Current format (ID is field 0, Type is field 2) */
            mbs = (float)atof(mbs_str);
            final_type = type;
        } else {
            /* Legacy Format */
            mbs = (float)atof(fs);
            final_type = timestamp;
        }

        BenchTestType t_idx = StringToTestType(final_type);

        if (t_idx != TEST_COUNT) {
            TestStats *ts = &report->stats[t_idx];
            ts->avg_mbps += mbs;
            if (mbs > ts->max_mbps)
                ts->max_mbps = mbs;
            ts->total_runs++;
            report->total_benchmarks++;
        }
    }

    IDOS->FClose(file);

    /* Finalize averages */
    for (int i = 0; i < TEST_COUNT; i++) {
        if (report->stats[i].total_runs > 0) {
            report->stats[i].avg_mbps /= (float)report->stats[i].total_runs;
        }
    }
    LOG_DEBUG("Global report generated: %u benchmarks found", (unsigned int)report->total_benchmarks);
    return (report->total_benchmarks > 0);
}
