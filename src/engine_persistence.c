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
                              "Vendor,Product\n");
        }
    } else {
        LOG_DEBUG("SaveResultToCSV: Appending to existing file");
        IDOS->ChangeFilePosition(file, 0, OFFSET_END);
    }

    if (file) {
        const char *typeName = "Unknown";
        switch (result->type) {
        case TEST_SPRINTER:
            typeName = "Sprinter";
            break;
        case TEST_HEAVY_LIFTER:
            typeName = "HeavyLifter";
            break;
        case TEST_LEGACY:
            typeName = "Legacy";
            break;
        case TEST_DAILY_GRIND:
            typeName = "DailyGrind";
            break;
        default:
            break;
        }

        char line[1024];
        snprintf(line, sizeof(line), "%s,%s,%s,%s,%s,%.2f,%u,%s,%u,%s,%u,%u,%d,%.2f,%.2f,%.2f,%llu,%s,%s\n",
                 result->result_id, result->timestamp, typeName, result->volume_name, result->fs_type,
                 result->mb_per_sec, (unsigned int)result->iops, result->device_name, (unsigned int)result->device_unit,
                 result->app_version, (unsigned int)result->passes, (unsigned int)result->block_size,
                 (int)result->use_trimmed_mean, result->min_mbps, result->max_mbps, result->total_duration,
                 (unsigned long long)result->cumulative_bytes, result->vendor, result->product);
        IDOS->FPuts(file, line);

        IDOS->FClose(file);
        return TRUE;
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

        int t_idx = -1;

        if (strcmp(final_type, "Sprinter") == 0)
            t_idx = TEST_SPRINTER;
        else if (strcmp(final_type, "HeavyLifter") == 0)
            t_idx = TEST_HEAVY_LIFTER;
        else if (strcmp(final_type, "Legacy") == 0)
            t_idx = TEST_LEGACY;
        else if (strcmp(final_type, "DailyGrind") == 0)
            t_idx = TEST_DAILY_GRIND;

        if (t_idx != -1) {
            TestStats *ts = &report->stats[t_idx];
            ts->avg_mbps += mbs;
            if (mbs > ts->max_mbps)
                ts->max_mbps = mbs;
            ts->total_runs++;
            report->total_benchmarks++;
        }
    }

    IDOS->Close(file);

    /* Finalize averages */
    for (int i = 0; i < TEST_COUNT; i++) {
        if (report->stats[i].total_runs > 0) {
            report->stats[i].avg_mbps /= (float)report->stats[i].total_runs;
        }
    }
    LOG_DEBUG("Global report generated: %u benchmarks found", (unsigned int)report->total_benchmarks);
    return (report->total_benchmarks > 0);
}
