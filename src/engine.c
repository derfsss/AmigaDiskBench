#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "debug.h"
#ifndef ENGINE_H
#include "engine.h"
#endif
#include "gui.h"

/* Global library bases and interfaces */
struct Device *BenchTimerBase = NULL;
struct TimerIFace *IBenchTimer = NULL;
struct MsgPort *BenchTimerPort = NULL;
struct TimeRequest *BenchTimerReq = NULL;

/*
 * Seed for "The Daily Grind" to ensure deterministic random behavior
 * across different systems and runs.
 */
#define FIXED_SEED 1985

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

void GetFileSystemInfo(const char *path, char *out_name, uint32 name_size)
{
    struct InfoData *info = IDOS->AllocDosObject(DOS_INFODATA, NULL);
    if (info) {
        /* Use modern GetDiskInfoTags (V51+) for better compatibility */
        if (IDOS->GetDiskInfoTags(GDI_StringNameInput, path, GDI_InfoData, info, TAG_DONE)) {
            uint32 dostype = info->id_DiskType;
            char hex[16];
            snprintf(hex, sizeof(hex), "0x%08X", (unsigned int)dostype);

            /* Try to map to a friendly name */
            const char *friendly = NULL;
            switch (dostype) {
            case 0x4E474653:
                friendly = "NGFS";
                break;
            case 0x4E474600:
                friendly = "NGFS (0)";
                break;
            case 0x4E474601:
                friendly = "NGFS (1)";
                break;
            case 0x444F5300:
                friendly = "OFS";
                break;
            case 0x444F5301:
                friendly = "FFS";
                break;
            case 0x444F5302:
                friendly = "OFS (Intl)";
                break;
            case 0x444F5303:
                friendly = "FFS (Intl)";
                break;
            case 0x444F5304:
                friendly = "OFS (DirCache)";
                break;
            case 0x444F5305:
                friendly = "FFS (DirCache)";
                break;
            case 0x444F5306:
                friendly = "OFS (Longnames)";
                break;
            case 0x444F5307:
                friendly = "FFS (Longnames)";
                break;
            case 0x53465300:
                friendly = "SFS/00";
                break;
            case 0x53465302:
                friendly = "SFS/02";
                break;
            case 0x50465303:
                friendly = "PFS3";
                break;
            case 0x52414D01:
                friendly = "RAM Disk";
                break;
            case 0x43443031:
                friendly = "CDFS";
                break;
            case 0x46415432:
                friendly = "FAT32";
                break;
            case 0x46415458:
                friendly = "exFAT";
                break;
            case 0x4e544653:
                friendly = "NTFS";
                break;
            case 0x45585402:
                friendly = "ext2";
                break;
            case 0x48465300:
                friendly = "HFS";
                break;
            case 0x53574150:
                friendly = "Swap";
                break;
            case 0x454E5601:
                friendly = "ENV";
                break;
            case 0x41504401:
                friendly = "AppDir";
                break;
            case 0x42414D00:
                friendly = "BAD";
                break;
            case 0x42555359:
                friendly = "BUSY";
                break;
            case 0x4E444F53:
                friendly = "NDOS";
                break;
            case (uint32)-1:
                friendly = "EMPTY";
                break;
            }

            if (friendly) {
                snprintf(out_name, name_size, "%s (%s)", friendly, hex);
            } else {
                /* Try to decode DOSType chars (e.g. 'DOS\x07') */
                char c1 = (dostype >> 24) & 0xFF;
                char c2 = (dostype >> 16) & 0xFF;
                char c3 = (dostype >> 8) & 0xFF;
                char v = dostype & 0xFF;
                if (c1 >= ' ' && c2 >= ' ' && c3 >= ' ') {
                    snprintf(out_name, name_size, "%c%c%c/%u (%s)", c1, c2, c3, (unsigned int)v, hex);
                } else {
                    strncpy(out_name, hex, name_size);
                }
            }
        } else {
            strncpy(out_name, "Invalid Path", name_size);
        }
        IDOS->FreeDosObject(DOS_INFODATA, info);
    } else {
        LOG_DEBUG("FAILED to allocate DOS_INFODATA");
        strncpy(out_name, "Out of Mem", name_size);
    }
    out_name[name_size - 1] = '\0';
    LOG_DEBUG("FS info for %s: %s (limit: %u)", path, out_name, (unsigned int)name_size);
}

void GetHardwareInfo(const char *path, BenchResult *result)
{
    strncpy(result->app_version, APP_VERSION_STR, sizeof(result->app_version));

    /* Initialize with defaults */
    strncpy(result->device_name, "Unknown", sizeof(result->device_name));
    result->device_unit = 0;
    strncpy(result->vendor, "Standard", sizeof(result->vendor));
    strncpy(result->product, "Storage Device", sizeof(result->product));

    /* v1.8.4 Fix 5: Resolve logical label to canonical device ID (e.g. 'RAM Disk:' -> 'RAM:')
       to ensure GetDiskFileSystemData() can reliably identify the physical disk. */
    BPTR lock = IDOS->Lock(path, SHARED_LOCK);
    char canonical[64];
    strncpy(canonical, path, sizeof(canonical) - 1);

    if (lock) {
        if (IDOS->DevNameFromLock(lock, canonical, sizeof(canonical), DN_DEVICEONLY)) {
            LOG_DEBUG("GetHardwareInfo: Resolved '%s' to canonical '%s'", path, canonical);
        }
        IDOS->UnLock(lock);
    }

    struct FileSystemData *fsd = IDOS->GetDiskFileSystemData(canonical);
    if (fsd) {
        if (fsd->fsd_DeviceName) {
            strncpy(result->device_name, fsd->fsd_DeviceName, sizeof(result->device_name) - 1);
            result->device_name[sizeof(result->device_name) - 1] = '\0';
        } else {
            strncpy(result->device_name, "Generic Disk", sizeof(result->device_name));
        }
        result->device_unit = fsd->fsd_DeviceUnit;
        IDOS->FreeDiskFileSystemData(fsd);
    } else {
        /* Catch-all fallback for special/virtual volumes.
           Try to get the unit number via legacy DosList lookup. */
        char search_name[64];
        strncpy(search_name, canonical, sizeof(search_name) - 1);
        search_name[sizeof(search_name) - 1] = '\0';
        char *c = strchr(search_name, ':');
        if (c)
            *c = '\0';

        struct DosList *dl = IDOS->LockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
        if (dl) {
            /* First try to find as a device directly since DN_DEVICEONLY was used */
            struct DosList *node = IDOS->FindDosEntry(dl, search_name, LDF_DEVICES);
            if (!node) {
                /* Fallback to volume search if device lookup failed */
                node = IDOS->FindDosEntry(dl, search_name, LDF_VOLUMES);
            }

            if (node) {
                uint32 startup = 0;
                if (node->dol_Type == DLT_DEVICE) {
                    startup = (uint32)node->dol_misc.dol_device.dol_Startup;
                } else if (node->dol_Type == DLT_VOLUME) {
                    startup = (uint32)node->dol_misc.dol_handler.dol_Startup;
                }

                if (startup > 0 && startup <= 64) {
                    /* Small integer is the unit number */
                    result->device_unit = startup;
                } else if (startup > 64) {
                    /* Message block contains the unit */
                    struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)BADDR(startup);
                    if (fssm)
                        result->device_unit = fssm->fssm_Unit;
                }
            }
            IDOS->UnLockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
        }

        if (strcasecmp(canonical, "RAM:") == 0 || strcasecmp(canonical, "RAM Disk:") == 0) {
            strncpy(result->device_name, "ramdrive.device", sizeof(result->device_name));
        } else {
            strncpy(result->device_name, "Generic Disk", sizeof(result->device_name));
        }
    }
}

static uint32 WriteDummyFile(const char *path, uint32 size, uint32 chunk_size)
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

static void GetMicroTime(struct TimeVal *tv)
{
    if (IBenchTimer) {
        IBenchTimer->GetSysTime(tv);
    }
}

static float GetDuration(struct TimeVal *start, struct TimeVal *end)
{
    double s = (double)start->Seconds + (double)start->Microseconds / 1000000.0;
    double e = (double)end->Seconds + (double)end->Microseconds / 1000000.0;
    return (float)(e - s);
}

static BOOL RunSingleBenchmark(BenchTestType type, const char *target_path, uint32 block_size, BenchResult *out_result)
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

BOOL RunBenchmark(BenchTestType type, const char *target_path, uint32 passes, uint32 block_size, BOOL use_trimmed_mean,
                  BenchResult *out_result)
{
    if (passes == 0)
        passes = 1;
    if (passes > 20)
        passes = 20; /* Expanded safety limit */

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

    /* v1.9.11: Generate unique ID */
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

    /* Trimmed Mean: remove min and max if passes >= 3 and enabled */
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

        LOG_DEBUG("[Debug] Trimmed Mean: Excluding Min (%.2f) at index %d and Max (%.2f) at index %d", min_val,
                  min_idx + 1, max_val, max_idx + 1);

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
        LOG_DEBUG("[Debug] Filtered Average: %.2f / %u = %.2f MB/s", filtered_sum, (unsigned int)filtered_count,
                  out_result->mb_per_sec);
    } else {
        /* Simple average */
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
        LOG_DEBUG("[Debug] Simple Average: %.2f / %u = %.2f MB/s", sum_mbps, (unsigned int)valid_passes,
                  out_result->mb_per_sec);
    }

    out_result->iops = sum_iops / valid_passes;

    IExec->FreeVec(results);

    LOG_DEBUG("Multi-pass benchmark (n=%u) completed. MB/s: %.2f", (unsigned int)valid_passes, out_result->mb_per_sec);
    return TRUE;
}

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

        char timestamp[32], type[64], disk[32], fs[64], mbs_str[32], iops_str[32];
        /* CSV layout v1.8.4: DateTime,Type,Volume,FS,MB/s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize */
        /* CSV layout v1.8.4: DateTime,Type,Volume,FS,MB/s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize
           [v1.9.14] Use %[^,\r\n] for the last field to handle trailing commas/whitespace robustly. */
        char device[64], unit[32], app_ver[32], passes_str[16], bs_str[32];
        int fields = sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,\r\n]", timestamp,
                            type, disk, fs, mbs_str, iops_str, device, unit, app_ver, passes_str, bs_str);

        if (fields < 7) {
            LOG_DEBUG("GenerateGlobalReport: Skipping malformed line (fields=%d): %s", fields, line);
            continue;
        }

        float mbs = (float)atof(mbs_str);
        int t_idx = -1;

        if (strcmp(type, "Sprinter") == 0)
            t_idx = TEST_SPRINTER;
        else if (strcmp(type, "HeavyLifter") == 0)
            t_idx = TEST_HEAVY_LIFTER;
        else if (strcmp(type, "Legacy") == 0)
            t_idx = TEST_LEGACY;
        else if (strcmp(type, "DailyGrind") == 0)
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
