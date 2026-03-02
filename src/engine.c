/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"
#include "engine_warmup.h"
#include "engine_workloads.h"
#include <float.h>
#include <stdlib.h>
#include <time.h>

/* Global library bases and interfaces */
struct Device *BenchTimerBase = NULL;
struct TimerIFace *IBenchTimer = NULL;
struct MsgPort *BenchTimerPort = NULL;
struct TimeRequest *BenchTimerReq = NULL;

/*
 * Initializes the benchmark engine.
 * Sets up timer.device and the workload registry.
 */
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
                    InitWorkloadRegistry();
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

/*
 * Frees all engine resources.
 * Safe to call even if partially initialized.
 */
void CleanupEngine(void)
{
    LOG_DEBUG("Cleaning up engine...");
    CleanupWorkloadRegistry();
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

void GetMicroTime(struct TimeVal *tv)
{
    if (IBenchTimer) {
        IBenchTimer->GetSysTime(tv);
    }
}

float GetDuration(struct TimeVal *start, struct TimeVal *end)
{
    struct TimeVal delta = *end;
    IBenchTimer->SubTime(&delta, start);
    return (float)delta.Seconds + (float)delta.Microseconds / 1000000.0f;
}

/**
 * @brief Compare two floats for qsort() sorting in ascending order.
 */
static int CompareFloats(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb)
        return -1;
    if (fa > fb)
        return 1;
    return 0;
}


static void AddSample(BenchSampleData *sd, float time, float value)
{
    if (sd && sd->sample_count < MAX_SAMPLES) {
        sd->samples[sd->sample_count].time_offset = time;
        sd->samples[sd->sample_count].value = value;
        sd->sample_count++;
    }
}

BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, uint32 averaging_method,
                  BOOL flush_cache, ProgressCallback progress_cb, BenchResult *out_result, BenchSampleData *out_samples)
{
    if (passes == 0)
        passes = 1;
    if (passes > MAX_PASSES)
        passes = MAX_PASSES;

    LOG_DEBUG("RunBenchmark: Type=%d, Passes=%u, BS=%u, AvgMethod=%u, Flush=%d", type, (unsigned int)passes,
              (unsigned int)block_size, (unsigned int)averaging_method, (int)flush_cache);

    /* Force Block Size to 0 (Mixed) for fixed-behavior tests */
    if (type == TEST_DAILY_GRIND || type == TEST_PROFILER) {
        block_size = 0;
    }

    if (flush_cache) {
        FlushDiskCache(target_path);
    }

    /* Perform Warmup */
    RunWarmup(target_path);

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
    snprintf(out_result->volume_name, sizeof(out_result->volume_name), "%s", target_path);
    char *colon = strchr(out_result->volume_name, ':');
    if (colon)
        *colon = '\0';

    // LOG_DEBUG("RunBenchmark: target='%s' -> volume_name='%s'", target_path, out_result->volume_name);

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
    uint32 sum_iops = 0;
    float total_duration = 0;
    uint64 total_bytes = 0;

    const BenchWorkload *workload = GetWorkloadByType(type);
    if (!workload) {
        LOG_DEBUG("FAILED to find workload for type %d", type);
        LogUser("ERROR: Unknown test type %d - no workload registered", type);
        IExec->FreeVec(results);
        return FALSE;
    }

    void *workload_data = NULL;
    if (!workload->Setup(target_path, block_size, &workload_data)) {
        LOG_DEBUG("FAILED to setup workload '%s' on '%s'", workload->name, target_path);
        LogUser("ERROR: %s setup failed on '%s' - could not create test file, open device, or allocate %u-byte buffer",
                workload->name, target_path, (unsigned int)block_size);
        IExec->FreeVec(results);
        return FALSE;
    }

    for (uint32 i = 0; i < passes; i++) {
        uint32 pass_bytes = 0, pass_ops = 0;
        struct TimeVal start_tv, end_tv;

        GetMicroTime(&start_tv);
        BOOL success = workload->Run(workload_data, &pass_bytes, &pass_ops);
        GetMicroTime(&end_tv);

        if (success) {
            float duration = GetDuration(&start_tv, &end_tv);
            if (duration > 0) {
                results[valid_passes] = ((float)pass_bytes / (1024.0f * 1024.0f)) / duration;
                LOG_DEBUG("[Debug] Pass %u: %.2f MB/s", (unsigned int)valid_passes + 1, results[valid_passes]);
                sum_iops += pass_ops;
                valid_passes++;
                total_duration += duration;
                total_bytes += pass_bytes;

                /* Add a sample point for this pass */
                float val = (out_result->type == TEST_PROFILER) ? (float)pass_ops / duration
                                                                : ((float)pass_bytes / (1024.0f * 1024.0f)) / duration;
                AddSample(out_samples, total_duration, val);

                /* Report progress if callback provided */
                if (progress_cb) {
                    char progress_text[128];
                    if (out_result->type == TEST_PROFILER) {
                        snprintf(progress_text, sizeof(progress_text), "Pass %u/%u - %.0f IOPS", (unsigned int)(i + 1),
                                 (unsigned int)passes, val);
                    } else {
                        snprintf(progress_text, sizeof(progress_text), "Pass %u/%u - %.1f MB/s", (unsigned int)(i + 1),
                                 (unsigned int)passes, val);
                    }
                    progress_cb(progress_text, FALSE);
                }
            }
        }
    }

    workload->Cleanup(workload_data);

    if (valid_passes == 0) {
        LogUser("ERROR: %s - all %u passes produced zero bytes on '%s' (block %u)",
                workload->name, (unsigned int)passes, target_path, (unsigned int)block_size);
        IExec->FreeVec(results);
        return FALSE;
    }

    /* Track total work */
    out_result->total_duration = total_duration;
    out_result->cumulative_bytes = total_bytes;
    out_result->averaging_method = averaging_method;

    switch (averaging_method) {
    case AVERAGE_ALL_PASSES:
        /* Simple average of all passes */
        out_result->min_mbps = results[0];
        out_result->max_mbps = results[0];
        float all_sum = 0.0f;
        for (uint32 i = 0; i < valid_passes; i++) {
            all_sum += results[i];
            if (results[i] < out_result->min_mbps)
                out_result->min_mbps = results[i];
            if (results[i] > out_result->max_mbps)
                out_result->max_mbps = results[i];
        }
        out_result->mb_per_sec = all_sum / (float)valid_passes;
        out_result->effective_passes = valid_passes;
        LOG_DEBUG("[Average] All Passes: n=%u, avg=%.2f MB/s", valid_passes, out_result->mb_per_sec);
        break;

    case AVERAGE_TRIMMED_MEAN:
        /* Exclude best and worst results */
        if (valid_passes >= 3) {
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

            LOG_DEBUG("[Trimmed] Excluding Min (%.2f) and Max (%.2f)", min_val, max_val);

            float filtered_sum = 0;
            uint32 filtered_count = 0;
            out_result->min_mbps = FLT_MAX;
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
            /* Fallback to all passes if less than 3 */
            out_result->min_mbps = results[0];
            out_result->max_mbps = results[0];
            float trim_sum = 0.0f;
            for (uint32 i = 0; i < valid_passes; i++) {
                trim_sum += results[i];
                if (results[i] < out_result->min_mbps)
                    out_result->min_mbps = results[i];
                if (results[i] > out_result->max_mbps)
                    out_result->max_mbps = results[i];
            }
            out_result->mb_per_sec = trim_sum / (float)valid_passes;
            out_result->effective_passes = valid_passes;
        }
        break;

    case AVERAGE_MEDIAN:
        /* Select median value from sorted results */
        if (valid_passes >= 1) {
            /* Sort the results array */
            qsort(results, valid_passes, sizeof(float), CompareFloats);

            /* Calculate median index using rounding: round(passes/2) - 1 for 0-based indexing */
            uint32 median_idx = (uint32)round((double)valid_passes / 2.0) - 1;
            if (median_idx >= valid_passes)
                median_idx = valid_passes - 1;

            out_result->mb_per_sec = results[median_idx];
            out_result->min_mbps = results[0];                    /* Min of sorted array */
            out_result->max_mbps = results[valid_passes - 1];    /* Max of sorted array */
            out_result->effective_passes = 1;                    /* Only one value used */

            LOG_DEBUG("[Median] Passes: %u, Index: %u, Value: %.2f MB/s", valid_passes, median_idx,
                      results[median_idx]);
        } else {
            out_result->mb_per_sec = results[0];
            out_result->min_mbps = out_result->max_mbps = results[0];
            out_result->effective_passes = 1;
        }
        break;

    default:
        /* Fallback to all passes for unknown method */
        out_result->min_mbps = results[0];
        out_result->max_mbps = results[0];
        float default_sum = 0.0f;
        for (uint32 i = 0; i < valid_passes; i++) {
            default_sum += results[i];
            if (results[i] < out_result->min_mbps)
                out_result->min_mbps = results[i];
            if (results[i] > out_result->max_mbps)
                out_result->max_mbps = results[i];
        }
        out_result->mb_per_sec = default_sum / (float)valid_passes;
        out_result->effective_passes = valid_passes;
        LOG_DEBUG("[Average] Unknown method %u, using all passes", averaging_method);
    }


    out_result->iops = (total_duration > 0.0f) ? (uint32)((float)sum_iops / total_duration) : 0;
    IExec->FreeVec(results);

    LOG_DEBUG("Multi-pass benchmark (n=%u) completed. MB/s: %.2f", (unsigned int)valid_passes, out_result->mb_per_sec);
    return TRUE;
}
