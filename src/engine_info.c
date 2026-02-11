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
#include <stdio.h>
#include <string.h>

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
        /* buffer for inquiry data */
        uint8 *inq = IExec->AllocVecTags(256, AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
        if (inq) {
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

            if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Status == 0) {
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

            if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Status == 0) {
                uint8 len = inq[3];
                if (len > 0) {
                    if (len > sizeof(result->serial_number) - 1)
                        len = sizeof(result->serial_number) - 1;
                    memcpy(result->serial_number, &inq[4], len);
                    result->serial_number[len] = '\0';
                    StripTrailingSpaces(result->serial_number, len);
                }
            } else {
                strncpy(result->serial_number, "N/A", sizeof(result->serial_number));
            }

            IExec->FreeVec(inq);
        }
        IExec->CloseDevice((struct IORequest *)ior);
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

    /* Resolve logical label to canonical device ID */
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

    strncpy(result->serial_number, "N/A", sizeof(result->serial_number));
    strncpy(result->firmware_rev, "N/A", sizeof(result->firmware_rev));

    /* If we have a real device name, attempt low-level inquiry */
    if (result->device_name[0] && strcmp(result->device_name, "Generic Disk") != 0 &&
        strcmp(result->device_name, "ramdrive.device") != 0) {
        GetScsiHardwareInfo(result->device_name, result->device_unit, result);
    }
}
