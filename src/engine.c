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
#include <time.h>

/* Global library bases and interfaces */
struct Device *BenchTimerBase = NULL;
struct TimerIFace *IBenchTimer = NULL;
struct MsgPort *BenchTimerPort = NULL;
struct TimeRequest *BenchTimerReq = NULL;

BOOL InitEngine(void)
{
    BenchTimerPort = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    if (BenchTimerPort) {
        BenchTimerReq = (struct TimeRequest *)IExec->AllocSysObjectTags(
            ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct TimeRequest), ASOIOR_ReplyPort, BenchTimerPort, TAG_DONE);
        if (BenchTimerReq) {
            if (IExec->OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)BenchTimerReq, 0) == 0) {
                BenchTimerBase = (struct Device *)BenchTimerReq->Request.io_Device;
                IBenchTimer =
                    (struct TimerIFace *)IExec->GetInterface((struct Library *)BenchTimerBase, "main", 1, NULL);
                if (IBenchTimer) {
                    LOG_DEBUG("Engine initialized successfully");
                    return TRUE;
                }
                LOG_DEBUG("FAILED to get Timer interface");
            } else {
                LOG_DEBUG("FAILED to open timer.device");
            }
        } else {
            LOG_DEBUG("FAILED to allocate TimeRequest");
        }
    } else {
        LOG_DEBUG("FAILED to allocate TimerPort");
    }
    CleanupEngine();
    return FALSE;
}

void CleanupEngine(void)
{
    LOG_DEBUG("Cleaning up engine...");
    if (IBenchTimer) {
        IExec->DropInterface((struct Interface *)IBenchTimer);
        IBenchTimer = NULL;
    }
    if (BenchTimerBase) {
        IExec->CloseDevice((struct IORequest *)BenchTimerReq);
        BenchTimerBase = NULL;
    }
    if (BenchTimerReq) {
        IExec->FreeSysObject(ASOT_IOREQUEST, BenchTimerReq);
        BenchTimerReq = NULL;
    }
    if (BenchTimerPort) {
        IExec->FreeSysObject(ASOT_PORT, BenchTimerPort);
        BenchTimerPort = NULL;
    }
}

BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, BOOL use_trimmed_mean,
                  BenchResult *out_result)
{
    if (passes == 0)
        passes = 1;
    if (passes > 20)
        passes = 20;

    LOG_DEBUG("RunBenchmark: Type=%d, Passes=%u, BS=%u, Trimmed=%d", type, (unsigned int)passes,
              (unsigned int)block_size, (int)use_trimmed_mean);

    float *results = IExec->AllocVecTags(sizeof(float) * passes, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!results)
        return FALSE;

    memset(out_result, 0, sizeof(BenchResult));
    out_result->type = type;
    out_result->passes = passes;
    out_result->block_size = block_size;

    GetFileSystemInfo(target_path, out_result->fs_type, sizeof(out_result->fs_type));
    GetHardwareInfo(target_path, out_result);

    /* Populate volume name */
    strncpy(out_result->volume_name, target_path, sizeof(out_result->volume_name) - 1);
    char *colon = strchr(out_result->volume_name, ':');
    if (colon)
        *colon = '\0';

    /* Capture timestamp */
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(out_result->timestamp, sizeof(out_result->timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    /* Generate unique ID */
    snprintf(out_result->result_id, sizeof(out_result->result_id), "%04d%02d%02d%02d%02d%02d_%04hX",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
             timeinfo->tm_sec, (unsigned short)(rand() & 0xFFFF));

    uint32 valid_passes = 0;
    float sum_mbps = 0;
    uint32 sum_iops = 0;
    float total_duration = 0;
    uint64 total_bytes = 0;

    for (uint32 i = 0; i < passes; i++) {
        BenchResult single_res;
        memset(&single_res, 0, sizeof(BenchResult));
        if (RunSingleBenchmark(type, target_path, block_size, &single_res)) {
            results[valid_passes] = single_res.mb_per_sec;
            LOG_DEBUG("[Debug] Pass %u: %.2f MB/s", (unsigned int)valid_passes + 1, results[valid_passes]);
            valid_passes++;
            sum_iops += single_res.iops;
            total_duration += single_res.duration_secs;
            total_bytes += single_res.total_bytes;
        }
    }

    if (valid_passes == 0) {
        IExec->FreeVec(results);
        return FALSE;
    }

    /* Track total work */
    out_result->total_duration = total_duration;
    out_result->cumulative_bytes = total_bytes;
    out_result->use_trimmed_mean = use_trimmed_mean;

    if (use_trimmed_mean && valid_passes >= 3) {
        float min_val = results[0], max_val = results[0];
        int min_idx = 0, max_idx = 0;
        for (uint32 i = 1; i < valid_passes; i++) {
            if (results[i] < min_val) {
                min_val = results[i];
                min_idx = i;
            }
            if (results[i] > max_val) {
                max_val = results[i];
                max_idx = i;
            }
        }

        LOG_DEBUG("[Debug] Trimmed Mean: Excluding Min (%.2f) and Max (%.2f)", min_val, max_val);

        float filtered_sum = 0;
        uint32 filtered_count = 0;
        out_result->min_mbps = 999999.0f;
        out_result->max_mbps = 0.0f;

        for (uint32 i = 0; i < valid_passes; i++) {
            if (i == min_idx || i == max_idx)
                continue;
            filtered_sum += results[i];
            if (results[i] < out_result->min_mbps)
                out_result->min_mbps = results[i];
            if (results[i] > out_result->max_mbps)
                out_result->max_mbps = results[i];
            filtered_count++;
        }
        out_result->mb_per_sec = filtered_sum / filtered_count;
        out_result->effective_passes = filtered_count;
    } else {
        out_result->min_mbps = results[0];
        out_result->max_mbps = results[0];
        for (uint32 i = 0; i < valid_passes; i++) {
            sum_mbps += results[i];
            if (results[i] < out_result->min_mbps)
                out_result->min_mbps = results[i];
            if (results[i] > out_result->max_mbps)
                out_result->max_mbps = results[i];
        }
        out_result->mb_per_sec = sum_mbps / (float)valid_passes;
        out_result->effective_passes = valid_passes;
    }

    out_result->iops = sum_iops / valid_passes;
    IExec->FreeVec(results);

    LOG_DEBUG("Multi-pass benchmark (n=%u) completed. MB/s: %.2f", (unsigned int)valid_passes, out_result->mb_per_sec);
    return TRUE;
}
