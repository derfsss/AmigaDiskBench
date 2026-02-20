/*
 * AmigaDiskBench - Disk Information Engine
 * Handles enumeration of physical drives, RDB scanning, and detailed geometry retrieval.
 */

#include "engine_diskinfo.h"
#include "engine_internal.h" // For LOG_DEBUG
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <libraries/diskio.h>
#include <libraries/expansion.h>
#include <scsi/devtypes.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include <stdio.h>
#include <string.h>

#define MAX_RDB_BLOCKS 16
#define BLOCK_SIZE_512 512

// --- Helper Functions ---

static void StripTrailingSpaces(char *str, uint32 len)
{
    if (len == 0)
        return;
    int32 i = len - 1;
    while (i >= 0 && (str[i] == ' ' || str[i] == '\0' || str[i] == '\r' || str[i] == '\n')) {
        str[i] = '\0';
        i--;
    }
}

static void SanitizeString(char *str)
{
    while (*str) {
        // Replace non-printable ascii characters with spaces to prevent graphics.library DSI crashes
        if ((unsigned char)*str < 32 || (unsigned char)*str > 126) {
            *str = ' ';
        }
        str++;
    }
}

const char *BusTypeToString(BusType type)
{
    switch (type) {
    case BUS_TYPE_PATA:
        return "PATA/IDE";
    case BUS_TYPE_SATA:
        return "SATA";
    case BUS_TYPE_SCSI:
        return "SCSI";
    case BUS_TYPE_USB:
        return "USB";
    case BUS_TYPE_NVME:
        return "NVMe";
    default:
        return "Unknown";
    }
}

const char *MediaTypeToString(MediaType type)
{
    switch (type) {
    case MEDIA_TYPE_HDD:
        return "Hard Disk (HDD)";
    case MEDIA_TYPE_SSD:
        return "Solid State (SSD)";
    case MEDIA_TYPE_CDROM:
        return "CD/DVD-ROM";
    case MEDIA_TYPE_FLOPPY:
        return "Floppy Disk";
    default:
        return "Unknown";
    }
}

const char *GetDosTypeString(uint32 dostype)
{
    static char buf[32];
    // Simple decoder for common types
    char c1 = (dostype >> 24) & 0xFF;
    char c2 = (dostype >> 16) & 0xFF;
    char c3 = (dostype >> 8) & 0xFF;
    char v = dostype & 0xFF;

    if (c1 >= 0x20 && c2 >= 0x20 && c3 >= 0x20) {
        snprintf(buf, sizeof(buf), "%c%c%c/%02X", c1, c2, c3, (uint8)v);
    } else {
        snprintf(buf, sizeof(buf), "0x%08X", (unsigned int)dostype);
    }
    return buf;
}

// --- SCSI Inquiry Logic ---

static void PerformScsiInquiry(struct IOStdReq *ior, PhysicalDrive *drive)
{
    LOG_DEBUG("PerformScsiInquiry: Querying %s Unit %lu", drive->device_name, drive->unit_number);
    uint8 *buffer = IExec->AllocVecTags(256, AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (!buffer)
        return;

    struct SCSICmd cmd;
    uint8 cdb[6];
    uint8 sense_buffer[32];
    uint8 type = SCSI_TYPE_UNKNOWN;

    // 1. Standard Inquiry (Vendor, Product, Revision)
    memset(&cmd, 0, sizeof(cmd));
    memset(cdb, 0, sizeof(cdb));
    memset(sense_buffer, 0, sizeof(sense_buffer));
    cdb[0] = SCSI_CMD_INQUIRY;
    cdb[4] = SCSI_INQ_STD_LEN; // 36 bytes

    cmd.scsi_Data = (APTR)buffer;
    cmd.scsi_Length = SCSI_INQ_STD_LEN;
    cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
    cmd.scsi_SenseData = (UBYTE *)sense_buffer;
    cmd.scsi_SenseLength = sizeof(sense_buffer);
    cmd.scsi_Command = (APTR)cdb;
    cmd.scsi_CmdLength = 6;

    ior->io_Command = HD_SCSICMD;
    ior->io_Data = &cmd;
    ior->io_Length = sizeof(struct SCSICmd);

    if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Status == 0) {
        // Vendor (8-15)
        memcpy(drive->vendor, &buffer[8], 8);
        drive->vendor[8] = '\0';
        StripTrailingSpaces(drive->vendor, 8);

        // Product (16-31)
        memcpy(drive->product, &buffer[16], 16);
        drive->product[16] = '\0';
        StripTrailingSpaces(drive->product, 16);

        // Revision (32-35)
        memcpy(drive->revision, &buffer[32], 4);
        drive->revision[4] = '\0';
        StripTrailingSpaces(drive->revision, 4);

        // Peripheral Device Type (Byte 0, bits 0-4)
        type = buffer[0] & 0x1F;
        LOG_DEBUG("PerformScsiInquiry: Peripheral Device Type = 0x%02X", type);

        switch (type) {
        case SCSI_TYPE_DIRECTACCESS:
            if (drive->media_type == MEDIA_TYPE_UNKNOWN)
                drive->media_type = MEDIA_TYPE_HDD;
            break;
        case SCSI_TYPE_CDROM:
            drive->media_type = MEDIA_TYPE_CDROM;
            drive->is_removable = TRUE; // CD-ROMs are removable
            break;
        case SCSI_TYPE_OPTICAL_DISK:
        case SCSI_TYPE_SIMPLIFIED_DIRECT_ACCESS:
            if (drive->media_type == MEDIA_TYPE_UNKNOWN)
                drive->media_type = MEDIA_TYPE_HDD; // Treat RBC/Optical as disks for now
            break;
        default:
            // Explicitly unsupported SCSI device types (Printers, Scanners, Arrays, Processors)
            LOG_DEBUG("PerformScsiInquiry: Device explicitly rejected (Unsupported Type)");
            drive->media_type = MEDIA_TYPE_UNKNOWN;
            break;
        }
    } else {
        LOG_DEBUG("PerformScsiInquiry: Standard inquiry failed on %s %lu. Aborting VPD queries.", drive->device_name,
                  drive->unit_number);
        IExec->FreeVec(buffer);
        return;
    }

    // 2. VPD Page 0x80 (Serial Number)
    // SKIP for CD-ROM (SCSI_TYPE_CDROM) as many ATAPI drives hang on VPD queries
    if (type != SCSI_TYPE_CDROM) {
        memset(&cmd, 0, sizeof(cmd));
        memset(cdb, 0, sizeof(cdb));
        cdb[0] = SCSI_CMD_INQUIRY;
        cdb[1] = 0x01; // EVPD
        cdb[2] = 0x80; // Serial
        cdb[4] = 252;

        cmd.scsi_Data = (APTR)buffer;
        cmd.scsi_Length = 252;
        cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
        cmd.scsi_SenseData = (UBYTE *)sense_buffer;
        cmd.scsi_SenseLength = sizeof(sense_buffer);
        cmd.scsi_Command = (APTR)cdb;
        cmd.scsi_CmdLength = 6;

        // Clear buffer to avoid stale data
        memset(buffer, 0, 256);

        if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Status == 0) {
            uint8 page_len = buffer[3];
            if (page_len > 0) {
                if (page_len > 31)
                    page_len = 31;
                memcpy(drive->serial, &buffer[4], page_len);
                drive->serial[page_len] = '\0';
                StripTrailingSpaces(drive->serial, page_len);
            }
        }
    } else {
        LOG_DEBUG("PerformScsiInquiry: Skipping VPD Page 0x80 for CD-ROM");
    }

    // 3. VPD Page 0xB1 (Block Device Characteristics - Rotation Rate)
    // Only verify if not CDROM
    if (drive->media_type != MEDIA_TYPE_CDROM) {
        memset(&cmd, 0, sizeof(cmd));
        memset(cdb, 0, sizeof(cdb));
        cdb[0] = SCSI_CMD_INQUIRY;
        cdb[1] = 0x01; // EVPD
        cdb[2] = 0xB1; // Block Device Characteristics
        cdb[4] = 252;

        cmd.scsi_Data = (APTR)buffer;
        cmd.scsi_Length = 252;
        cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
        cmd.scsi_SenseData = (UBYTE *)sense_buffer;
        cmd.scsi_SenseLength = sizeof(sense_buffer);
        cmd.scsi_Command = (APTR)cdb;
        cmd.scsi_CmdLength = 6;

        memset(buffer, 0, 256);

        if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Status == 0) {
            // Medium Rotation Rate is at byte 4 (2 bytes)
            // 0000h = Non-rotating (SSD)
            // 0001h = Non-rotating reserved
            // Other = Rotation rate in rpm
            uint16 rotation = (buffer[4] << 8) | buffer[5];
            if (rotation == 0) {
                drive->media_type = MEDIA_TYPE_SSD;
            } else {
                drive->media_type = MEDIA_TYPE_HDD;
            }
        } else {
            // Fallback assumption if Page B1 not supported
            drive->media_type = MEDIA_TYPE_HDD;
        }
    }

    IExec->FreeVec(buffer);
}

// --- RDB Scanning ---

static BOOL ScanForRDB(struct IOStdReq *ior, PhysicalDrive *drive)
{
    uint8 *block_buffer = IExec->AllocVecTags(BLOCK_SIZE_512, AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!block_buffer)
        return FALSE;

    BOOL rdb_found = FALSE;
    LOG_DEBUG("ScanForRDB: Scanning %s Unit %lu...", drive->device_name, drive->unit_number);

    // Scan first 16 blocks
    for (int i = 0; i < MAX_RDB_BLOCKS; i++) {
        ior->io_Command = CMD_READ;
        ior->io_Data = block_buffer;
        ior->io_Length = BLOCK_SIZE_512;
        ior->io_Offset = i * BLOCK_SIZE_512;

        if (IExec->DoIO((struct IORequest *)ior) == 0) {
            struct RigidDiskBlock *rdb = (struct RigidDiskBlock *)block_buffer;

            // Check ID 'RDSK'
            if (rdb->rdb_ID == IDNAME_RIGIDDISK) {
                LOG_DEBUG("RDB found on %s Unit %d at block %d", drive->device_name, drive->unit_number, i);

                // Copy RDB data
                memcpy(&drive->rdb, rdb, sizeof(struct RigidDiskBlock));
                drive->rdb_found = TRUE;
                rdb_found = TRUE;

                // Extract RDB String Data
                if (rdb->rdb_DiskVendor[0]) {
                    memcpy(drive->vendor, rdb->rdb_DiskVendor, 8);
                    drive->vendor[8] = '\0';
                    StripTrailingSpaces(drive->vendor, 8);
                }
                if (rdb->rdb_DiskProduct[0]) {
                    memcpy(drive->product, rdb->rdb_DiskProduct, 16);
                    drive->product[16] = '\0';
                    StripTrailingSpaces(drive->product, 16);
                }
                if (rdb->rdb_DiskRevision[0]) {
                    memcpy(drive->revision, rdb->rdb_DiskRevision, 4);
                    drive->revision[4] = '\0';
                    StripTrailingSpaces(drive->revision, 4);
                }

                // Extract Geometry
                drive->cylinders = rdb->rdb_Cylinders;
                drive->heads = rdb->rdb_Heads;
                drive->sectors = rdb->rdb_Sectors;
                drive->block_bytes = rdb->rdb_BlockBytes;

                // Calculate Capacity
                drive->capacity_bytes = (uint64)drive->cylinders * drive->heads * drive->sectors * drive->block_bytes;

                // Determine media type hint if unknown
                if (drive->media_type == MEDIA_TYPE_UNKNOWN) {
                    drive->media_type = MEDIA_TYPE_HDD;
                }

                break; // Found it
            }
        }
    }

    IExec->FreeVec(block_buffer);
    LOG_DEBUG("ScanForRDB: Result %d", rdb_found);
    return rdb_found;
}

// --- Geometry Fallback ---

static void GetDriveGeometry(struct IOStdReq *ior, PhysicalDrive *drive)
{
    if (drive->rdb_found)
        return; // Already got it

    struct DriveGeometry geom;
    LOG_DEBUG("GetDriveGeometry: Querying %s Unit %lu", drive->device_name, drive->unit_number);
    ior->io_Command = TD_GETGEOMETRY;
    ior->io_Data = &geom;
    ior->io_Length = sizeof(struct DriveGeometry);

    if (IExec->DoIO((struct IORequest *)ior) == 0) {
        LOG_DEBUG("GetDriveGeometry: Success");
        drive->cylinders = geom.dg_Cylinders;
        drive->heads = geom.dg_Heads;
        drive->sectors = geom.dg_TrackSectors;
        drive->block_bytes = geom.dg_BufMemType; // Actually sector size in newer devices, check API
        // Wait, struct DriveGeometry definition:
        // dg_SectorSize is what we want.
        drive->block_bytes = geom.dg_SectorSize;

        drive->capacity_bytes = (uint64)drive->cylinders * drive->heads * drive->sectors * drive->block_bytes;
    } else {
        LOG_DEBUG("GetDriveGeometry: Failed to get geometry");
    }
    // Try extended drive status for type
    ior->io_Command = TD_GETDRIVETYPE;
    if (IExec->DoIO((struct IORequest *)ior) == 0) {
        uint32 type = ior->io_Actual;
        if (type == DRIVE3_5)
            drive->media_type = MEDIA_TYPE_FLOPPY;
        // Other types are legacy 3.5/5.25 logic usually
    }
}

// --- Main Enrichment ---

static void EnrichPhysicalDrive(PhysicalDrive *drive)
{
    // 1. Bus Type Heuristics
    if (strstr(drive->device_name, "a1ide"))
        drive->bus_type = BUS_TYPE_PATA;
    else if (strstr(drive->device_name, "sata"))
        drive->bus_type = BUS_TYPE_SATA;
    else if (strstr(drive->device_name, "scsi"))
        drive->bus_type = BUS_TYPE_SCSI;
    else if (strstr(drive->device_name, "usb"))
        drive->bus_type = BUS_TYPE_USB;
    else if (strstr(drive->device_name, "nvme"))
        drive->bus_type = BUS_TYPE_NVME;
    else
        drive->bus_type = BUS_TYPE_UNKNOWN;

    // 2. Open Device
    struct MsgPort *port = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    if (!port)
        return;

    struct IOStdReq *ior = IExec->AllocSysObjectTags(ASOT_IOREQUEST, ASOIOR_ReplyPort, port, ASOIOR_Size,
                                                     sizeof(struct IOExtTD), TAG_DONE);

    if (ior) {
        LOG_DEBUG("EnrichPhysicalDrive: Opening device %s Unit %lu...", drive->device_name, drive->unit_number);
        int32 err = IExec->OpenDevice(drive->device_name, drive->unit_number, (struct IORequest *)ior, 0);
        if (err == 0) {
            LOG_DEBUG("EnrichPhysicalDrive: OpenDevice success");

            // 3. Scan RDB
            if (!ScanForRDB(ior, drive)) {
                // Not an RDB disk or RDB not found
                GetDriveGeometry(ior, drive);
            }

            // 4. SCSI Inquiry (for Brand/SSD info) - works even if RDB present to get Serial
            // Now enabled for all drives (even empty CD-ROMs) to get Product Name
            LOG_DEBUG("EnrichPhysicalDrive: Performing SCSI Inquiry...");
            PerformScsiInquiry(ior, drive);

            IExec->CloseDevice((struct IORequest *)ior);
        } else {
            LOG_DEBUG("EnrichPhysicalDrive: OpenDevice failed with error %ld", err);
        }
        IExec->FreeSysObject(ASOT_IOREQUEST, ior);
    }
    IExec->FreeSysObject(ASOT_PORT, port);

    // Sanitize any garbage returned by arbitrary device drivers
    SanitizeString(drive->vendor);
    SanitizeString(drive->product);
    SanitizeString(drive->revision);

    // Final cleanup of strings
    if (strlen(drive->vendor) == 0 || strncmp(drive->vendor, "        ", 8) == 0)
        snprintf(drive->vendor, sizeof(drive->vendor), "Generic");
    if (strlen(drive->product) == 0 || strncmp(drive->product, "                ", 16) == 0)
        snprintf(drive->product, sizeof(drive->product), "Storage Device");
}

// --- Main Entry Point ---

// --- Main Entry Point ---

static PhysicalDrive *FindPhysicalDrive(struct List *list, const char *device, uint32 unit)
{
    struct Node *node;
    for (node = IExec->GetHead(list); node; node = IExec->GetSucc(node)) {
        PhysicalDrive *drive = (PhysicalDrive *)node;
        if (strcmp(drive->device_name, device) == 0 && drive->unit_number == unit) {
            return drive;
        }
    }
    return NULL;
}

struct List *ScanSystemDrives(void)
{
    LOG_DEBUG("ScanSystemDrives: Entry (REAL SCANNING)");
    struct List *driveList = IExec->AllocSysObjectTags(ASOT_LIST, TAG_DONE);
    if (!driveList) {
        return NULL;
    }

    struct DosList *dl = IDOS->LockDosList(LDF_DEVICES | LDF_READ);
    if (!dl) {
        LOG_DEBUG("ScanSystemDrives: Failed to lock DosList");
        return driveList; // Return empty list rather than NULL
    }

    // Traverse the DosList
    struct DosList *entry = IDOS->NextDosEntry(dl, LDF_DEVICES);
    while (entry) {
        // Safe-decode Name for logging
        char entryName[32] = {0};
        if (entry->dol_Name) {
            UBYTE *nbstr = (UBYTE *)((uint32)entry->dol_Name << 2);
            if (nbstr) {
                uint32 nlen = *nbstr;
                if (nlen > 31)
                    nlen = 31;
                memcpy(entryName, nbstr + 1, nlen);
                entryName[nlen] = '\0';
            }
        }
        LOG_DEBUG("ScanSystemDrives: Inspecting DosEntry '%s' @ %p", entryName, entry);

        // Check for FileSysStartupMsg
        // Heuristic: Startup must be a valid BPTR (not small int)
        if (entry->dol_misc.dol_handler.dol_Startup > 100) {
            // Startup is a BPTR to FileSysStartupMsg
            struct FileSysStartupMsg *fssm =
                (struct FileSysStartupMsg *)((uint32)entry->dol_misc.dol_handler.dol_Startup << 2);

            // Validate fssm pointer range (avoid Page 0)
            if ((uint32)fssm > 0x1000) {
                // Check fssm->fssm_Device is valid BPTR
                if (fssm->fssm_Device > 100) {
                    // Device is a BSTR (BPTR to Length-Prefixed String)
                    UBYTE *bstr = (UBYTE *)((uint32)fssm->fssm_Device << 2);

                    // Validate bstr pointer
                    if ((uint32)bstr > 0x1000) {
                        uint32 len = *bstr;
                        char devName[32];

                        // Cap length
                        if (len > 31)
                            len = 31;

                        // Copy content
                        memcpy(devName, bstr + 1, len);
                        devName[len] = '\0';

                        uint32 unit = fssm->fssm_Unit;
                        LOG_DEBUG("ScanSystemDrives: Found Handler Device '%s' Unit %lu", devName, unit);

                        PhysicalDrive *drive = FindPhysicalDrive(driveList, devName, unit);
                        if (!drive) {
                            // New Physical Drive Candidate
                            drive = IExec->AllocVecTags(sizeof(PhysicalDrive), AVT_Type, MEMF_SHARED,
                                                        AVT_ClearWithValue, 0, TAG_DONE);
                            if (drive) {
                                snprintf(drive->device_name, sizeof(drive->device_name), "%s", devName);
                                drive->unit_number = unit;
                                IExec->NewMinList(&drive->partitions);

                                // Initial Label
                                char labelBuf[64];
                                snprintf(labelBuf, sizeof(labelBuf), "%s Unit %lu", devName, unit);

                                drive->node.ln_Name =
                                    IExec->AllocVecTags(strlen(labelBuf) + 1, AVT_Type, MEMF_SHARED, TAG_DONE);
                                if (drive->node.ln_Name)
                                    strcpy(drive->node.ln_Name, labelBuf);

                                // Enrich with Real Data (Geometry, RDB, Inquiry)
                                EnrichPhysicalDrive(drive);

                                // Dynamic Validation: Only keep block storage devices
                                // If media_type is still UNKNOWN and capacity is 0, neither SCSI nor Trackdisk matched
                                // it
                                if (drive->media_type == MEDIA_TYPE_UNKNOWN && drive->capacity_bytes == 0) {
                                    LOG_DEBUG("ScanSystemDrives: Rejecting non-storage device '%s'", devName);
                                    if (drive->node.ln_Name)
                                        IExec->FreeVec(drive->node.ln_Name);
                                    IExec->FreeVec(drive);
                                    drive = NULL; // Prevent partition loop below
                                } else {
                                    IExec->AddTail(driveList, (struct Node *)drive);
                                }
                            }
                        }

                        // Add Partition Logic (Real Scanning)
                        if (drive) {
                            LogicalPartition *part = IExec->AllocVecTags(sizeof(LogicalPartition), AVT_Type,
                                                                         MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
                            if (part) {
                                // Use the DossHandler Name (e.g., DH0)
                                snprintf(part->dos_device_name, sizeof(part->dos_device_name), "%s", entryName);

                                // Volume Name & Info
                                char devicePath[64];
                                snprintf(devicePath, sizeof(devicePath), "%s:", entryName);

                                // Suppress system requesters (e.g. "No Disk in device CD0")
                                APTR oldWin = IDOS->SetProcWindow((APTR)-1);
                                BPTR lock = IDOS->Lock(devicePath, ACCESS_READ);
                                IDOS->SetProcWindow(oldWin);

                                if (!lock) {
                                    LOG_DEBUG("Failed to Lock '%s' (Empty or Unmounted)", devicePath);
                                }

                                if (lock) {
                                    struct InfoData id;
                                    if (IDOS->Info(lock, &id)) {
                                        part->dos_type = id.id_DiskType;
                                        part->disk_environment_type = id.id_DiskType;
                                        part->block_size = id.id_BytesPerBlock;
                                        part->blocks_per_drive = id.id_NumBlocks;

                                        // Calculate bytes (Careful with overflow on large drives, cast to uint64)
                                        part->size_bytes = (uint64)id.id_NumBlocks * (uint64)id.id_BytesPerBlock;
                                        part->used_bytes = (uint64)id.id_NumBlocksUsed * (uint64)id.id_BytesPerBlock;
                                        part->free_bytes = part->size_bytes - part->used_bytes;

                                        // Get Volume Name from InfoData (if attached)
                                        // Actually InfoData doesn't have Volume Name.
                                        // We need to check if the Lock refers to a volume.
                                        // For now, use the Device Name as default Volume Name
                                        snprintf(part->volume_name, sizeof(part->volume_name), "%s", entryName);

                                        // Try to get real Volume Name
                                        char volBuf[64];
                                        if (IDOS->NameFromLock(lock, volBuf, sizeof(volBuf))) {
                                            // NameFromLock returns "Volume:Path" or "Device:Path"
                                            // We just want the Volume part.
                                            char *colon = strchr(volBuf, ':');
                                            if (colon)
                                                *colon = '\0';
                                            snprintf(part->volume_name, sizeof(part->volume_name), "%s", volBuf);
                                        }
                                    }
                                    IDOS->UnLock(lock);
                                } else {
                                    // Failed to lock (maybe not mounted?)
                                    snprintf(part->volume_name, sizeof(part->volume_name), "Not Mounted");
                                }

                                IExec->AddTail((struct List *)&drive->partitions, (struct Node *)part);
                            }
                        }
                    }
                }
            }
        }
        entry = IDOS->NextDosEntry(entry, LDF_DEVICES);
    }

    IDOS->UnLockDosList(LDF_DEVICES | LDF_READ);
    return driveList;
}

void FreePhysicalDriveList(struct List *list)
{
    if (!list)
        return;

    struct Node *node;
    while ((node = IExec->RemHead(list))) {
        PhysicalDrive *drive = (PhysicalDrive *)node;

        // Free Partitions
        struct Node *pNode;
        while ((pNode = IExec->RemHead((struct List *)&drive->partitions))) {
            IExec->FreeVec(pNode);
        }

        if (drive->node.ln_Name)
            IExec->FreeVec(drive->node.ln_Name);
        IExec->FreeVec(drive);
    }

    IExec->FreeSysObject(ASOT_LIST, list);
}
