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

#include "engine_internal.h"
#include <devices/scsidisk.h>

/* Helper to strip trailing spaces from SCSI strings */
static void StripTrailingSpaces(char *str, uint32 len)
{
    if (len == 0)
        return;
    int32 i = len - 1;
    while (i >= 0 && (str[i] == ' ' || str[i] == '\0')) {
        str[i] = '\0';
        i--;
    }
}

static void GetScsiHardwareInfo(const char *device_name, uint32 unit, BenchResult *result)
{
    struct MsgPort *port = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    if (!port)
        return;

    struct IOStdReq *ior = (struct IOStdReq *)IExec->AllocSysObjectTags(ASOT_IOREQUEST, ASOIOR_ReplyPort, port,
                                                                        ASOIOR_Size, sizeof(struct IOStdReq), TAG_DONE);
    if (!ior) {
        IExec->FreeSysObject(ASOT_PORT, port);
        return;
    }

    if (IExec->OpenDevice(device_name, unit, (struct IORequest *)ior, 0) == 0) {
        LOG_DEBUG("GetScsiHardwareInfo: Opened %s unit %d", device_name, unit);
        /* buffer for inquiry data */
        uint8 *inq = IExec->AllocVecTags(256, AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
        if (inq) {
            memset(inq, 0, 256); /* Ensure buffer is clear even if AVT_ClearWithValue is flaking */
            struct SCSICmd cmd;
            uint8 cdb[6];

            /* Standard Inquiry */
            memset(&cmd, 0, sizeof(struct SCSICmd));
            memset(cdb, 0, sizeof(cdb));
            cdb[0] = SCSI_CMD_INQUIRY;
            cdb[4] = SCSI_INQ_STD_LEN;

            cmd.scsi_Data = (APTR)inq;
            cmd.scsi_Length = SCSI_INQ_STD_LEN;
            cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
            cmd.scsi_Command = (APTR)cdb;
            cmd.scsi_CmdLength = 6;

            ior->io_Command = HD_SCSICMD;
            ior->io_Data = &cmd;
            ior->io_Length = sizeof(struct SCSICmd);

            int32 io_err = IExec->DoIO((struct IORequest *)ior);
            if (io_err == 0 && cmd.scsi_Status == 0) {
                /* Vendor: bytes 8-15 */
                memcpy(result->vendor, &inq[8], 8);
                result->vendor[8] = '\0';
                StripTrailingSpaces(result->vendor, 8);

                /* Product: bytes 16-31 */
                memcpy(result->product, &inq[16], 16);
                result->product[16] = '\0';
                StripTrailingSpaces(result->product, 16);

                /* Firmware: bytes 32-35 */
                memcpy(result->firmware_rev, &inq[32], 4);
                result->firmware_rev[4] = '\0';
                StripTrailingSpaces(result->firmware_rev, 4);
                LOG_DEBUG("SCSI Inquiry: Ven='%s', Prod='%s', Rev='%s'", result->vendor, result->product,
                          result->firmware_rev);
            } else {
                LOG_DEBUG("SCSI Inquiry Std Failed: Err=%d, Status=%d", io_err, cmd.scsi_Status);
            }

            /* Inquiry VPD 0x80 (Serial Number) */
            memset(inq, 0, 256);
            memset(cdb, 0, sizeof(cdb));
            cdb[0] = SCSI_CMD_INQUIRY;
            cdb[1] = 0x01; /* EVPD */
            cdb[2] = 0x80; /* Unit Serial Number */
            cdb[4] = SCSI_INQ_VPD_LEN;

            cmd.scsi_Data = (APTR)inq;
            cmd.scsi_Length = SCSI_INQ_VPD_LEN;
            cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
            cmd.scsi_Command = (APTR)cdb;
            cmd.scsi_CmdLength = 6;

            io_err = IExec->DoIO((struct IORequest *)ior);
            if (io_err == 0 && cmd.scsi_Status == 0) {
                uint8 len = inq[3];
                if (len > 0) {
                    if (len > sizeof(result->serial_number) - 1)
                        len = sizeof(result->serial_number) - 1;
                    memcpy(result->serial_number, &inq[4], len);
                    result->serial_number[len] = '\0';
                    StripTrailingSpaces(result->serial_number, len);
                    LOG_DEBUG("SCSI VPD Serial: '%s'", result->serial_number);
                } else {
                    LOG_DEBUG("SCSI VPD Serial: Length was 0");
                    snprintf(result->serial_number, sizeof(result->serial_number), "%s", "N/A");
                }
            } else {
                LOG_DEBUG("SCSI Inquiry VPD Failed: Err=%d, Status=%d", io_err, cmd.scsi_Status);
                snprintf(result->serial_number, sizeof(result->serial_number), "%s", "N/A");
            }

            IExec->FreeVec(inq);
        }
        IExec->CloseDevice((struct IORequest *)ior);
    } else {
        LOG_DEBUG("GetScsiHardwareInfo: Failed to open %s unit %d", device_name, unit);
    }

    IExec->FreeSysObject(ASOT_IOREQUEST, ior);
    IExec->FreeSysObject(ASOT_PORT, port);
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
                    snprintf(out_name, name_size, "%s", hex);
                }
            }
        } else {
            snprintf(out_name, name_size, "%s", "Invalid Path");
        }
        IDOS->FreeDosObject(DOS_INFODATA, info);
    } else {
        LOG_DEBUG("FAILED to allocate DOS_INFODATA");
        snprintf(out_name, name_size, "%s", "Out of Mem");
    }
    out_name[name_size - 1] = '\0';
    LOG_DEBUG("FS info for %s: %s (limit: %u)", path, out_name, (unsigned int)name_size);
}

/* Cache structure for hardware info to reduce SCSI_INQUIRY overhead.
 * Field sizes match BenchResult to avoid truncation during copy. */
struct CachedHWInfo
{
    char device_name[64];
    uint32 device_unit;
    char vendor[32];
    char product[64];
    char serial_number[64];
    char firmware_rev[32];
};

struct DeviceCacheNode
{
    struct DeviceCacheNode *next;
    char path_key[MAX_PATH_LEN];
    struct CachedHWInfo info;
};

static struct DeviceCacheNode *hardware_cache = NULL;

void ClearHardwareInfoCache(void)
{
    struct DeviceCacheNode *node = hardware_cache;
    while (node) {
        struct DeviceCacheNode *next = node->next;
        IExec->FreeVec(node);
        node = next;
    }
    hardware_cache = NULL;
    LOG_DEBUG("Hardware Info Cache cleared.");
}

void GetHardwareInfo(const char *path, BenchResult *result)
{
    snprintf(result->app_version, sizeof(result->app_version), "%s", APP_VERSION_STR);

    /* 1. Check Cache */
    struct DeviceCacheNode *node = hardware_cache;
    while (node) {
        if (strcasecmp(node->path_key, path) == 0) {
            /* Hit! Copy cached data */
            snprintf(result->device_name, sizeof(result->device_name), "%s", node->info.device_name);
            result->device_unit = node->info.device_unit;
            snprintf(result->vendor, sizeof(result->vendor), "%s", node->info.vendor);
            snprintf(result->product, sizeof(result->product), "%s", node->info.product);
            snprintf(result->serial_number, sizeof(result->serial_number), "%s", node->info.serial_number);
            snprintf(result->firmware_rev, sizeof(result->firmware_rev), "%s", node->info.firmware_rev);
            /* Logging reduced to avoid spam, or keep for debug validation?
               Let's log a small "hit" message for verification */
            LOG_DEBUG("GetHardwareInfo: Cache Hit for '%s'", path);
            return;
        }
        node = node->next;
    }

    /* 2. Cache Miss - Perform Full Query */
    LOG_DEBUG("GetHardwareInfo: Cache Miss for '%s' - Querying...", path);

    /* Initialize with defaults */
    snprintf(result->device_name, sizeof(result->device_name), "%s", "Unknown");
    result->device_unit = 0;
    snprintf(result->vendor, sizeof(result->vendor), "%s", "Standard");
    snprintf(result->product, sizeof(result->product), "%s", "Storage Device");

    /* Resolve logical label to canonical device ID */
    BPTR lock = IDOS->Lock(path, SHARED_LOCK);
    char canonical[64];
    snprintf(canonical, sizeof(canonical), "%s", path);

    if (lock) {
        if (IDOS->DevNameFromLock(lock, canonical, sizeof(canonical), DN_DEVICEONLY)) {
            LOG_DEBUG("GetHardwareInfo: Resolved '%s' to canonical '%s'", path, canonical);
        }
        IDOS->UnLock(lock);
    }

    struct FileSystemData *fsd = IDOS->GetDiskFileSystemData(canonical);
    if (fsd) {
        if (fsd->fsd_DeviceName) {
            snprintf(result->device_name, sizeof(result->device_name), "%s", fsd->fsd_DeviceName);
            result->device_name[sizeof(result->device_name) - 1] = '\0';
        } else {
            snprintf(result->device_name, sizeof(result->device_name), "%s", "Generic Disk");
        }
        result->device_unit = fsd->fsd_DeviceUnit;
        IDOS->FreeDiskFileSystemData(fsd);
    } else {
        /* Catch-all fallback for special/virtual volumes.
           Try to get the unit number via legacy DosList lookup. */
        char search_name[64];
        snprintf(search_name, sizeof(search_name), "%s", canonical);
        search_name[sizeof(search_name) - 1] = '\0';
        char *c = strchr(search_name, ':');
        if (c)
            *c = '\0';

        struct DosList *dl = IDOS->LockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
        if (dl) {
            /* First try to find as a device directly since DN_DEVICEONLY was used */
            struct DosList *dnode = IDOS->FindDosEntry(dl, search_name, LDF_DEVICES);
            if (!dnode) {
                /* Fallback to volume search if device lookup failed */
                dnode = IDOS->FindDosEntry(dl, search_name, LDF_VOLUMES);
            }

            if (dnode) {
                uint32 startup = 0;
                if (dnode->dol_Type == DLT_DEVICE) {
                    startup = (uint32)dnode->dol_misc.dol_device.dol_Startup;
                } else if (dnode->dol_Type == DLT_VOLUME) {
                    startup = (uint32)dnode->dol_misc.dol_handler.dol_Startup;
                }

                if (startup > 0 && startup <= 64) {
                    /* Small integer is the unit number - but we don't know the device name! */
                    result->device_unit = startup;
                } else if (startup > 64) {
                    /* Message block contains the unit AND the device name */
                    struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)BADDR(startup);
                    if (fssm) {
                        result->device_unit = fssm->fssm_Unit;

                        /* Extract device name from BSTR */
                        if (fssm->fssm_Device) {
                            /* BSTR to C-String conversion */
                            /* Note: Do not use IDOS->CopyStringBSTRToC if we want to be safe with older startups
                               or if we just want a simple extract. Let's use the manual method which is
                               robust for standard BSTRs found in startup messages. */
                            UBYTE *bstr = (UBYTE *)BADDR(fssm->fssm_Device);
                            if (bstr) {
                                uint32 len = *bstr;
                                if (len > sizeof(result->device_name) - 1)
                                    len = sizeof(result->device_name) - 1;

                                memcpy(result->device_name, bstr + 1, len);
                                result->device_name[len] = '\0';

                                LOG_DEBUG("GetHardwareInfo: Resolved fallback device '%s' unit %d", result->device_name,
                                          result->device_unit);
                            }
                        }
                    }
                }
            }
            IDOS->UnLockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
        }

        if (strcasecmp(canonical, "RAM:") == 0 || strcasecmp(canonical, "RAM Disk:") == 0) {
            snprintf(result->device_name, sizeof(result->device_name), "%s", "ramdrive.device");
        } else {
            snprintf(result->device_name, sizeof(result->device_name), "%s", "Generic Disk");
        }
    }

    snprintf(result->serial_number, sizeof(result->serial_number), "%s", "N/A");
    snprintf(result->firmware_rev, sizeof(result->firmware_rev), "%s", "N/A");

    /* If we have a real device name, attempt low-level inquiry */
    if (result->device_name[0] && strcmp(result->device_name, "Generic Disk") != 0 &&
        strcmp(result->device_name, "ramdrive.device") != 0) {
        GetScsiHardwareInfo(result->device_name, result->device_unit, result);
    }

    /* 3. Add to Cache */
    struct DeviceCacheNode *new_node =
        IExec->AllocVecTags(sizeof(struct DeviceCacheNode), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (new_node) {
        snprintf(new_node->path_key, sizeof(new_node->path_key), "%s", path);
        new_node->path_key[sizeof(new_node->path_key) - 1] = '\0';

        snprintf(new_node->info.device_name, sizeof(new_node->info.device_name), "%s", result->device_name);
        new_node->info.device_name[sizeof(new_node->info.device_name) - 1] = '\0';

        new_node->info.device_unit = result->device_unit;

        snprintf(new_node->info.vendor, sizeof(new_node->info.vendor), "%s", result->vendor);
        new_node->info.vendor[sizeof(new_node->info.vendor) - 1] = '\0';

        snprintf(new_node->info.product, sizeof(new_node->info.product), "%s", result->product);
        new_node->info.product[sizeof(new_node->info.product) - 1] = '\0';

        snprintf(new_node->info.serial_number, sizeof(new_node->info.serial_number), "%s", result->serial_number);
        new_node->info.serial_number[sizeof(new_node->info.serial_number) - 1] = '\0';

        snprintf(new_node->info.firmware_rev, sizeof(new_node->info.firmware_rev), "%s", result->firmware_rev);
        new_node->info.firmware_rev[sizeof(new_node->info.firmware_rev) - 1] = '\0';

        new_node->next = hardware_cache;
        hardware_cache = new_node;
    }
}

BOOL GetDeviceFromVolume(const char *volume, char *out_device, uint32 device_size, uint32 *out_unit)
{
    if (!volume || !out_device || !out_unit)
        return FALSE;

    /* Create a dummy BenchResult to reuse GetHardwareInfo logic */
    BenchResult res;
    memset(&res, 0, sizeof(BenchResult));

    GetHardwareInfo(volume, &res);

    if (res.device_name[0] != '\0') {
        snprintf(out_device, device_size, "%s", res.device_name);
        *out_unit = res.device_unit;
        return TRUE;
    }

    return FALSE;
}
