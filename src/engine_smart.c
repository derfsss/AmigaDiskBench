/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * S.M.A.R.T. Data Retrieval Engine
 */

#include "engine_internal.h"
#include <devices/scsidisk.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

/* S.M.A.R.T. Command Constants */
#define ATA_SMART_CMD 0xB0
#define ATA_SMART_READ_DATA 0xD0
#define ATA_SMART_READ_THRESHOLD 0xD1
#define ATA_SMART_ENABLE 0xD8

/* Common S.M.A.R.T. Attribute Names */
static struct
{
    uint8 id;
    const char *name;
} attr_names[] = {{1, "Raw Read Error Rate"},
                  {2, "Throughput Performance"},
                  {3, "Spin-Up Time"},
                  {4, "Start/Stop Count"},
                  {5, "Reallocated Sector Count"},
                  {7, "Seek Error Rate"},
                  {8, "Seek Time Performance"},
                  {9, "Power-On Hours"},
                  {10, "Spin Retry Count"},
                  {12, "Power Cycle Count"},
                  {194, "Temperature Celsius"},
                  {196, "Reallocation Event Count"},
                  {197, "Current Pending Sector Count"},
                  {198, "Offline Uncorrectable"},
                  {199, "UltraDMA CRC Error Count"},
                  {240, "Head Flying Hours"},
                  {241, "Total LBAs Written"},
                  {242, "Total LBAs Read"},
                  {0, NULL}};

static const char *GetAttributeName(uint8 id)
{
    for (int i = 0; attr_names[i].id != 0; i++) {
        if (attr_names[i].id == id)
            return attr_names[i].name;
    }
    return "Unknown Attribute";
}

BOOL GetSmartData(const char *device_name, uint32 unit, SmartData *out_data)
{
    if (!device_name || !out_data)
        return FALSE;

    memset(out_data, 0, sizeof(SmartData));
    out_data->overall_status = SMART_STATUS_UNKNOWN;
    snprintf(out_data->health_summary, sizeof(out_data->health_summary), "Retrieving S.M.A.R.T. data...");

    /*
     * IMPORTANT: S.M.A.R.T. commands on AmigaOS 4.1 often depend on the driver.
     * Most modern AHCI/PATA drivers support the SAT (SCSI to ATA Translation) layer.
     * We will try to send a S.M.A.R.T. READ DATA command.
     */

    struct MsgPort *port = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    struct IOStdReq *io = IExec->AllocSysObjectTags(ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct IOStdReq),
                                                    ASOIOR_ReplyPort, port, TAG_DONE);

    if (port && io) {
        if (IExec->OpenDevice(device_name, unit, (struct IORequest *)io, 0) == 0) {
            uint8 buffer[512];
            struct SCSICmd cmd;
            uint8 cdb[16];

            /* Prepare the SCSI command - Using ATA PASS-THROUGH (12) if supported,
               otherwise falling back to what the driver expects.
               Note: S.M.A.R.T. is very specific to ATA.
               We'll try a generic approach that works on most SAT-compliant Amiga drivers.
            */
            memset(&cmd, 0, sizeof(struct SCSICmd));
            memset(cdb, 0, sizeof(cdb));
            memset(buffer, 0, sizeof(buffer));

            /* Many OS4 drivers accept S.M.A.R.T. via raw ATA commands if you know the mapping,
               but most use SAT (SCSI-ATA-Translation) where:
               CDB[0] = 0x85 (ATA PASS-THROUGH 16) or 0xA1 (ATA PASS-THROUGH 12)
            */

            /* ATA PASS-THROUGH (16) - Primary attempt */
            cdb[0] = 0x85;
            cdb[1] = (4 << 1) | 0;        /* PIO Data-In */
            cdb[2] = 0x2E;                /* CK_COND=1, T_DIR=1, BYTE_BLOCK=1, T_LENGTH=2 (Sector Count) */
            cdb[4] = ATA_SMART_READ_DATA; /* Feature (0xD0) for SMART */
            cdb[6] = 1;                   /* Sector Count */
            cdb[8] = 0x4F;                /* LBA Low (SMART Magic) */
            cdb[10] = 0xC2;               /* LBA Mid (SMART Magic) */
            cdb[12] = 0;                  /* LBA High */
            cdb[14] = ATA_SMART_CMD;      /* Command (0xB0) */

            cmd.scsi_Data = (APTR)buffer;
            cmd.scsi_Length = 512;
            cmd.scsi_Command = (APTR)cdb;
            cmd.scsi_CmdLength = 16;
            cmd.scsi_Flags = SCSIF_READ | SCSIF_AUTOSENSE;
            cmd.scsi_SenseData = NULL;
            cmd.scsi_SenseLength = 0;

            io->io_Command = HD_SCSICMD;
            io->io_Data = (APTR)&cmd;
            io->io_Length = sizeof(struct SCSICmd);

            BOOL cmd_success = FALSE;
            if (IExec->DoIO((struct IORequest *)io) == 0 && cmd.scsi_Actual == 512) {
                cmd_success = TRUE;
            } else {
                LOG_DEBUG("GetSmartData: 16-byte PT failed (IOErr=%d, Status=%d). Trying 12-byte fallback...",
                          io->io_Error, cmd.scsi_Status);

                /* FALLBACK: ATA PASS-THROUGH (12) - Secondary attempt for older drivers like a1ide.device */
                memset(cdb, 0, sizeof(cdb));
                cdb[0] = 0xA1;
                cdb[1] = (4 << 1) | 0;        /* PIO Data-In */
                cdb[2] = 0x2E;                /* Same mapping flags */
                cdb[3] = ATA_SMART_READ_DATA; /* Feature */
                cdb[4] = 1;                   /* Count */
                cdb[5] = 0x4F;                /* LBA Low */
                cdb[6] = 0xC2;                /* LBA Mid */
                cdb[7] = 0;                   /* LBA High */
                cdb[8] = 0;                   /* Device */
                cdb[9] = ATA_SMART_CMD;       /* Command */

                cmd.scsi_CmdLength = 12;
                if (IExec->DoIO((struct IORequest *)io) == 0 && cmd.scsi_Actual == 512) {
                    cmd_success = TRUE;
                    LOG_DEBUG("GetSmartData: 12-byte PT succeeded.");
                } else {
                    LOG_DEBUG("GetSmartData: 12-byte PT failed (IOErr=%d, Status=%d).", io->io_Error, cmd.scsi_Status);
                }
            }

            if (cmd_success) {
                /* Successfully read 512 bytes of S.M.A.R.T. data */
                out_data->supported = TRUE;
                out_data->driver_supported = TRUE;
                out_data->overall_status = SMART_STATUS_OK;

                /* Parse Attributes (starting at offset 2) */
                int attr_index = 0;
                for (int i = 2; i < 362 && attr_index < MAX_SMART_ATTRIBUTES; i += 12) {
                    uint8 id = buffer[i];
                    if (id == 0)
                        continue;

                    SmartAttribute *attr = &out_data->attributes[attr_index++];
                    attr->id = id;
                    snprintf(attr->name, sizeof(attr->name), "%s", GetAttributeName(id));
                    attr->value = buffer[i + 3];
                    attr->worst = buffer[i + 4];
                    /* Thresholds are in a different block (0xD1), but usually
                       we can infer basic health from Current/Worst vs constant thresholds.
                       For now, let's keep it simple. */
                    attr->threshold = 0;

                    /* Raw value (6 bytes, little endian) */
                    attr->raw_value = (uint64)buffer[i + 5] | ((uint64)buffer[i + 6] << 8) |
                                      ((uint64)buffer[i + 7] << 16) | ((uint64)buffer[i + 8] << 24) |
                                      ((uint64)buffer[i + 9] << 32) | ((uint64)buffer[i + 10] << 40);

                    attr->status = SMART_STATUS_OK;

                    /* Basic Health Heuristics */
                    if (id == 194)
                        out_data->temperature = (uint32)attr->raw_value & 0xFF;
                    if (id == 9)
                        out_data->power_on_hours = (uint32)attr->raw_value;
                    if (id == 5) {
                        out_data->reallocated_sectors = (uint32)attr->raw_value;
                        if (out_data->reallocated_sectors > 0) {
                            attr->status = SMART_STATUS_WARNING;
                            if (out_data->overall_status < SMART_STATUS_WARNING)
                                out_data->overall_status = SMART_STATUS_WARNING;
                        }
                    }
                    if (id == 197 || id == 198) {
                        if (attr->raw_value > 0) {
                            attr->status = SMART_STATUS_CRITICAL;
                            out_data->overall_status = SMART_STATUS_CRITICAL;
                        }
                    }
                }
                out_data->attribute_count = attr_index;
                snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                         out_data->overall_status == SMART_STATUS_OK ? "Drive is healthy." : "Drive issues detected!");
            } else {
                snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                         "S.M.A.R.T. command failed (IOErr=%d). Driver may not support ATA Passthrough.", io->io_Error);
                out_data->supported = FALSE;
                out_data->driver_supported = FALSE;
                LOG_DEBUG("GetSmartData: Command Sequence Failed completely.");
            }
            IExec->CloseDevice((struct IORequest *)io);
        } else {
            LOG_DEBUG("GetSmartData: OpenDevice failed for '%s' unit %d.", device_name, unit);
            snprintf(out_data->health_summary, sizeof(out_data->health_summary), "Failed to open device '%s' unit %d.",
                     device_name, (int)unit);
        }
    }

    if (io)
        IExec->FreeSysObject(ASOT_IOREQUEST, io);
    if (port)
        IExec->FreeSysObject(ASOT_PORT, port);

    return out_data->supported;
}
