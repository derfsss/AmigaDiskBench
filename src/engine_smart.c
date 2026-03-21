/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * S.M.A.R.T. Data Retrieval Engine
 *
 * Implements a three-tier fallback strategy for querying S.M.A.R.T. health data:
 *
 *   Tier 1: CMD_IDE / CMDIDE_DIRECTATA
 *           Sends raw ATA task file registers directly to the device driver.
 *           This is the method used by the AmigaOS smartctl port (by Stephane
 *           Guillard) and works on all his drivers: a1ide.device, sb600sata.device,
 *           sii3112ide.device, sii3114ide.device, and others that implement
 *           the CMD_IDE interface. Constants and struct layouts were
 *           reverse-engineered from the smartmontools 5.33 PPC binary.
 *
 *   Tier 2: HD_SCSICMD with ATA PASS-THROUGH (SAT)
 *           Wraps ATA commands inside SCSI CDBs per the SAT-2 specification.
 *           Uses 16-byte CDB (opcode 0x85) with 12-byte (opcode 0xA1) fallback.
 *           Works on drivers with SCSI-ATA Translation support.
 *
 *   Tier 3: External smartctl command
 *           Falls back to shelling out to the system smartctl binary
 *           (bundled with AmigaOS 4.1 Final Edition in C:) and parsing
 *           its text output. Availability checked via "version C:smartctl".
 */

#include "engine_internal.h"
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ========================================================================
 * ATA SMART Constants (ATA/ATAPI-5 Specification)
 * ======================================================================== */

#define ATA_SMART_CMD             0xB0
#define ATA_SMART_READ_DATA       0xD0
#define ATA_SMART_READ_THRESHOLDS 0xD1
#define ATA_SMART_ENABLE          0xD8
#define ATA_SMART_STATUS          0xDA
#define ATA_IDENTIFY_DEVICE       0xEC

/* SMART LBA magic signature: packed as (High<<24 | Mid<<16 | Low<<8 | 0) */
#define SMART_LBA_MAGIC           0x00C24F00


/* ========================================================================
 * CMD_IDE Interface
 *
 * Private ATA passthrough API used by Stephane Guillard's AmigaOS 4 device
 * drivers and his smartctl port. These constants and struct layouts were
 * reverse-engineered from the smartmontools 5.33 PPC ELF binary on os4depot.
 *
 * The header "ataio.h" that originally defined these is not part of the
 * public AmigaOS SDK and was never distributed separately.
 * ======================================================================== */

#define ADB_CMD_IDE              0x4100  /* io_Command value for IDE-specific access */
#define ADB_CMDIDE_DIRECTATA     4       /* CustomCmd subcommand: raw ATA passthrough */
#define ADB_CM_NON_DATA_LBA28   4       /* Transfer mode: no data (status/enable) */
#define ADB_CM_PIO_DATA_IN_LBA28 6      /* Transfer mode: PIO read, 512 bytes */

/*
 * ATA task file register block (52 bytes).
 * Matches the driver's internal REG_ATACMD_INFO structure.
 * Field offsets verified against disassembly of smartctl binary.
 */
struct ADB_ATARegs
{
    uint8  cmd;             /* Offset  0: ATA Command register (e.g. 0xB0) */
    uint8  _pad1[3];        /* Offset  1: Alignment to 4-byte boundary */
    uint32 feature;         /* Offset  4: Feature register (SMART subcommand) */
    uint32 sector_count;    /* Offset  8: Sector Count register */
    uint8  sector_num;      /* Offset 12: Sector Number register */
    uint8  _pad2[3];        /* Offset 13: Alignment padding */
    uint32 _reserved1[5];   /* Offset 16: Reserved (zeroed, 20 bytes) */
    uint32 num_sectors;     /* Offset 36: Number of sectors to transfer */
    uint32 _reserved2;      /* Offset 40: Reserved */
    uint32 lba_packed;      /* Offset 44: Packed LBA bytes */
    uint32 _reserved3;      /* Offset 48: Reserved */
};  /* Total: 52 bytes */

/*
 * Custom command wrapper for CMD_IDE (16 bytes).
 * Passed as io_Data to the device driver when io_Command = ADB_CMD_IDE.
 */
struct ADB_CustomCmd
{
    uint8  command;         /* Offset  0: Subcommand type (ADB_CMDIDE_DIRECTATA) */
    uint8  _pad[3];         /* Offset  1: Alignment padding */
    uint32 arg1;            /* Offset  4: Pointer to ADB_ATARegs */
    uint32 arg2;            /* Offset  8: Transfer mode constant */
    uint32 arg3;            /* Offset 12: Data buffer pointer (data-in only) */
};  /* Total: 16 bytes */


/* ========================================================================
 * SMART Attribute Name Lookup
 * ======================================================================== */

static struct
{
    uint8 id;
    const char *name;
} attr_names[] = {
    {1,   "Raw Read Error Rate"},
    {2,   "Throughput Performance"},
    {3,   "Spin-Up Time"},
    {4,   "Start/Stop Count"},
    {5,   "Reallocated Sector Count"},
    {7,   "Seek Error Rate"},
    {8,   "Seek Time Performance"},
    {9,   "Power-On Hours"},
    {10,  "Spin Retry Count"},
    {12,  "Power Cycle Count"},
    {170, "Available Reserved Space"},
    {171, "Program Fail Count"},
    {172, "Erase Fail Count"},
    {173, "Wear Leveling Count"},
    {174, "Unexpected Power Loss"},
    {175, "Power Loss Protection Failure"},
    {177, "Wear Range Delta"},
    {183, "SATA Downshift Error Count"},
    {184, "End-to-End Error Count"},
    {187, "Reported Uncorrectable Errors"},
    {188, "Command Timeout"},
    {190, "Airflow Temperature"},
    {191, "G-Sense Error Rate"},
    {192, "Power-Off Retract Count"},
    {193, "Load/Unload Cycle Count"},
    {194, "Temperature Celsius"},
    {195, "Hardware ECC Recovered"},
    {196, "Reallocation Event Count"},
    {197, "Current Pending Sector Count"},
    {198, "Offline Uncorrectable"},
    {199, "UltraDMA CRC Error Count"},
    {200, "Multi-Zone Error Rate"},
    {240, "Head Flying Hours"},
    {241, "Total LBAs Written"},
    {242, "Total LBAs Read"},
    {0,   NULL}
};

static const char *GetAttributeName(uint8 id)
{
    for (int i = 0; attr_names[i].id != 0; i++) {
        if (attr_names[i].id == id)
            return attr_names[i].name;
    }
    return "Unknown Attribute";
}


/* ========================================================================
 * Shared SMART Data Parsing
 * ======================================================================== */

/**
 * Parse the 512-byte SMART attribute data buffer into SmartData.
 * If thresh_buf is non-NULL, merge threshold values from SMART READ THRESHOLDS.
 *
 * Both buffers follow the ATA specification format:
 *   - Bytes 0-1: Revision number
 *   - Bytes 2-361: 30 attribute entries, 12 bytes each
 *     Data entry: [ID, Flags(2), Value, Worst, Raw(6), Reserved]
 *     Threshold entry: [ID, Threshold, Reserved(10)]
 */
static void ParseSmartBuffers(const uint8 *data_buf, const uint8 *thresh_buf,
                              SmartData *out_data)
{
    out_data->supported = TRUE;
    out_data->driver_supported = TRUE;
    out_data->overall_status = SMART_STATUS_OK;

    /* Parse SMART attribute entries (30 slots x 12 bytes, starting at offset 2) */
    int attr_index = 0;
    for (int i = 2; i < 362 && attr_index < MAX_SMART_ATTRIBUTES; i += 12) {
        uint8 id = data_buf[i];
        if (id == 0)
            continue;

        SmartAttribute *attr = &out_data->attributes[attr_index++];
        attr->id = id;
        snprintf(attr->name, sizeof(attr->name), "%s", GetAttributeName(id));
        attr->value = data_buf[i + 3];
        attr->worst = data_buf[i + 4];
        attr->threshold = 0;

        /* Raw value: 6 bytes at offset +5, stored little-endian by the drive */
        attr->raw_value = (uint64)data_buf[i + 5]
                        | ((uint64)data_buf[i + 6] << 8)
                        | ((uint64)data_buf[i + 7] << 16)
                        | ((uint64)data_buf[i + 8] << 24)
                        | ((uint64)data_buf[i + 9] << 32)
                        | ((uint64)data_buf[i + 10] << 40);

        attr->status = SMART_STATUS_OK;

        /* Extract key health indicators */
        if (id == 194)
            out_data->temperature = (uint32)(attr->raw_value & 0xFF);
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

    /* Merge threshold data if available */
    if (thresh_buf) {
        for (int i = 2; i < 362; i += 12) {
            uint8 tid = thresh_buf[i];
            uint8 tval = thresh_buf[i + 1];
            if (tid == 0)
                continue;

            for (uint32 j = 0; j < out_data->attribute_count; j++) {
                if (out_data->attributes[j].id == tid) {
                    out_data->attributes[j].threshold = tval;
                    /* Flag attributes at or below their failure threshold */
                    if (tval > 0 && out_data->attributes[j].value <= tval) {
                        out_data->attributes[j].status = SMART_STATUS_CRITICAL;
                        out_data->overall_status = SMART_STATUS_CRITICAL;
                    }
                    break;
                }
            }
        }
    }
}


/* ========================================================================
 * Tier 1: CMD_IDE / CMDIDE_DIRECTATA (Direct ATA Registers)
 *
 * Sends raw ATA task file registers to the device driver using the private
 * CMD_IDE command. This bypasses SCSI translation entirely and is the same
 * method used by the bundled smartctl on AmigaOS 4.
 * ======================================================================== */

static BOOL TrySmartCmdIDE(struct IOStdReq *ior, SmartData *out_data)
{
    uint8 data_buf[512];
    uint8 thresh_buf[512];
    struct ADB_ATARegs  ata_regs;
    struct ADB_CustomCmd ccmd;
    BOOL has_thresholds = FALSE;

    LOG_DEBUG("TrySmartCmdIDE: Attempting CMD_IDE (0x%04X)", ADB_CMD_IDE);

    /* --- Read SMART Data (ATA Feature 0xD0) --- */
    memset(data_buf, 0, sizeof(data_buf));
    memset(&ata_regs, 0, sizeof(ata_regs));
    memset(&ccmd, 0, sizeof(ccmd));

    ata_regs.cmd          = ATA_SMART_CMD;
    ata_regs.feature      = ATA_SMART_READ_DATA;
    ata_regs.sector_count = 1;
    ata_regs.num_sectors  = 1;
    ata_regs.lba_packed   = SMART_LBA_MAGIC;

    ccmd.command = ADB_CMDIDE_DIRECTATA;
    ccmd.arg1    = (uint32)&ata_regs;
    ccmd.arg2    = ADB_CM_PIO_DATA_IN_LBA28;
    ccmd.arg3    = (uint32)data_buf;

    ior->io_Command = ADB_CMD_IDE;
    ior->io_Data    = (APTR)&ccmd;
    ior->io_Length  = 512;
    ior->io_Error   = -3;  /* IOERR_NOCMD sentinel */

    if (IExec->DoIO((struct IORequest *)ior) != 0) {
        LOG_DEBUG("TrySmartCmdIDE: SMART READ DATA failed (io_Error=%d)", ior->io_Error);
        return FALSE;
    }

    LOG_DEBUG("TrySmartCmdIDE: SMART READ DATA succeeded");

    /* --- Read SMART Thresholds (ATA Feature 0xD1) --- */
    memset(thresh_buf, 0, sizeof(thresh_buf));
    memset(&ata_regs, 0, sizeof(ata_regs));
    memset(&ccmd, 0, sizeof(ccmd));

    ata_regs.cmd          = ATA_SMART_CMD;
    ata_regs.feature      = ATA_SMART_READ_THRESHOLDS;
    ata_regs.sector_count = 1;
    ata_regs.sector_num   = 1;
    ata_regs.num_sectors  = 1;
    ata_regs.lba_packed   = SMART_LBA_MAGIC;

    ccmd.command = ADB_CMDIDE_DIRECTATA;
    ccmd.arg1    = (uint32)&ata_regs;
    ccmd.arg2    = ADB_CM_PIO_DATA_IN_LBA28;
    ccmd.arg3    = (uint32)thresh_buf;

    ior->io_Command = ADB_CMD_IDE;
    ior->io_Data    = (APTR)&ccmd;
    ior->io_Length  = 512;
    ior->io_Error   = -3;

    if (IExec->DoIO((struct IORequest *)ior) == 0) {
        has_thresholds = TRUE;
        LOG_DEBUG("TrySmartCmdIDE: SMART READ THRESHOLDS succeeded");
    } else {
        LOG_DEBUG("TrySmartCmdIDE: SMART READ THRESHOLDS failed (non-fatal, io_Error=%d)",
                  ior->io_Error);
    }

    ParseSmartBuffers(data_buf, has_thresholds ? thresh_buf : NULL, out_data);
    out_data->method = SMART_METHOD_CMD_IDE;

    return (out_data->attribute_count > 0);
}


/* ========================================================================
 * Tier 2: HD_SCSICMD with ATA PASS-THROUGH (SAT)
 *
 * Sends ATA commands wrapped in SCSI CDBs per the SAT-2 specification.
 * Tries 16-byte CDB (opcode 0x85) first, then 12-byte (opcode 0xA1).
 * Each attempt fully reinitializes the SCSICmd struct to avoid stale state.
 * ======================================================================== */

/**
 * Issue a single ATA SMART read command via SAT.
 * Tries 16-byte ATA PASS-THROUGH first, then 12-byte fallback.
 *
 * @param ior      Open IOStdReq for the target device
 * @param feature  ATA Feature register value (e.g. 0xD0 for READ DATA)
 * @param buffer   512-byte output buffer (cleared before each attempt)
 * @return TRUE if 512 bytes were successfully returned
 */
static BOOL IssueSATCommand(struct IOStdReq *ior, uint8 feature, uint8 *buffer)
{
    struct SCSICmd cmd;
    uint8 cdb[16];
    uint8 sense[32];

    /* --- Attempt 1: ATA PASS-THROUGH (16), CDB opcode 0x85 --- */
    memset(&cmd, 0, sizeof(cmd));
    memset(cdb, 0, sizeof(cdb));
    memset(sense, 0, sizeof(sense));
    memset(buffer, 0, 512);

    cdb[0]  = 0x85;              /* ATA PASS-THROUGH (16) */
    cdb[1]  = (4 << 1);          /* Protocol: PIO Data-In */
    cdb[2]  = 0x2E;              /* CK_COND=1, T_DIR=1, BYTE_BLOCK=1, T_LENGTH=SectorCount */
    cdb[4]  = feature;           /* ATA Feature register */
    cdb[6]  = 1;                 /* Sector Count */
    cdb[8]  = 0x4F;              /* LBA Low  (SMART magic) */
    cdb[10] = 0xC2;              /* LBA Mid  (SMART magic) */
    cdb[14] = ATA_SMART_CMD;     /* ATA Command (0xB0) */

    cmd.scsi_Data       = (APTR)buffer;
    cmd.scsi_Length      = 512;
    cmd.scsi_Command     = (APTR)cdb;
    cmd.scsi_CmdLength   = 16;
    cmd.scsi_Flags       = SCSIF_READ | SCSIF_AUTOSENSE;
    cmd.scsi_SenseData   = sense;
    cmd.scsi_SenseLength = sizeof(sense);

    ior->io_Command = HD_SCSICMD;
    ior->io_Data    = (APTR)&cmd;
    ior->io_Length  = sizeof(struct SCSICmd);

    if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Actual == 512) {
        LOG_DEBUG("IssueSATCommand: 16-byte CDB succeeded for feature 0x%02X", feature);
        return TRUE;
    }

    LOG_DEBUG("IssueSATCommand: 16-byte CDB failed (IOErr=%d, Status=%d, Actual=%d)",
              ior->io_Error, cmd.scsi_Status, cmd.scsi_Actual);

    /* --- Attempt 2: ATA PASS-THROUGH (12), CDB opcode 0xA1 --- */
    memset(&cmd, 0, sizeof(cmd));
    memset(cdb, 0, sizeof(cdb));
    memset(sense, 0, sizeof(sense));
    memset(buffer, 0, 512);

    cdb[0] = 0xA1;              /* ATA PASS-THROUGH (12) */
    cdb[1] = (4 << 1);          /* Protocol: PIO Data-In */
    cdb[2] = 0x2E;              /* CK_COND=1, T_DIR=1, BYTE_BLOCK=1, T_LENGTH=SectorCount */
    cdb[3] = feature;           /* ATA Feature register */
    cdb[4] = 1;                 /* Sector Count */
    cdb[5] = 0x4F;              /* LBA Low  (SMART magic) */
    cdb[6] = 0xC2;              /* LBA Mid  (SMART magic) */
    cdb[9] = ATA_SMART_CMD;     /* ATA Command (0xB0) */

    cmd.scsi_Data       = (APTR)buffer;
    cmd.scsi_Length      = 512;
    cmd.scsi_Command     = (APTR)cdb;
    cmd.scsi_CmdLength   = 12;
    cmd.scsi_Flags       = SCSIF_READ | SCSIF_AUTOSENSE;
    cmd.scsi_SenseData   = sense;
    cmd.scsi_SenseLength = sizeof(sense);

    ior->io_Command = HD_SCSICMD;
    ior->io_Data    = (APTR)&cmd;
    ior->io_Length  = sizeof(struct SCSICmd);

    if (IExec->DoIO((struct IORequest *)ior) == 0 && cmd.scsi_Actual == 512) {
        LOG_DEBUG("IssueSATCommand: 12-byte CDB succeeded for feature 0x%02X", feature);
        return TRUE;
    }

    LOG_DEBUG("IssueSATCommand: 12-byte CDB also failed (IOErr=%d, Status=%d, Actual=%d)",
              ior->io_Error, cmd.scsi_Status, cmd.scsi_Actual);
    return FALSE;
}

static BOOL TrySmartSAT(struct IOStdReq *ior, SmartData *out_data)
{
    uint8 data_buf[512];
    uint8 thresh_buf[512];

    LOG_DEBUG("TrySmartSAT: Attempting HD_SCSICMD with ATA PASS-THROUGH");

    /* Read SMART Data (Feature 0xD0) */
    if (!IssueSATCommand(ior, ATA_SMART_READ_DATA, data_buf)) {
        LOG_DEBUG("TrySmartSAT: SMART READ DATA failed via SAT");
        return FALSE;
    }

    /* Read SMART Thresholds (Feature 0xD1) - non-fatal if unsupported */
    BOOL has_thresholds = IssueSATCommand(ior, ATA_SMART_READ_THRESHOLDS, thresh_buf);

    ParseSmartBuffers(data_buf, has_thresholds ? thresh_buf : NULL, out_data);
    out_data->method = SMART_METHOD_SAT;

    return (out_data->attribute_count > 0);
}


/* ========================================================================
 * Tier 3: External smartctl Command
 *
 * Falls back to the system smartctl binary bundled with AmigaOS 4.1 FE.
 * Executes "C:smartctl -HA device:unit" and parses the text output.
 * ======================================================================== */

/**
 * Check if smartctl is installed. Result is cached for the session.
 * Uses "version C:smartctl" which returns 0 if the file exists and
 * contains a valid AmigaOS version string.
 */
static BOOL IsSmartCtlAvailable(void)
{
    static int32 cached = -1;  /* -1=unchecked, 0=absent, 1=present */

    if (cached >= 0)
        return (BOOL)cached;

    BPTR nil_in  = IDOS->Open("NIL:", MODE_OLDFILE);
    BPTR nil_out = IDOS->Open("NIL:", MODE_NEWFILE);

    if (nil_in && nil_out) {
        /* SystemTags takes ownership of the file handles */
        LONG rc = IDOS->SystemTags("version C:smartctl >NIL:",
                                   SYS_Input, nil_in,
                                   SYS_Output, nil_out,
                                   TAG_DONE);
        cached = (rc == 0) ? 1 : 0;
    } else {
        if (nil_in)  IDOS->Close(nil_in);
        if (nil_out) IDOS->Close(nil_out);
        cached = 0;
    }

    LOG_DEBUG("IsSmartCtlAvailable: %s", cached ? "Yes" : "No");
    return (BOOL)cached;
}

/**
 * Parse a single line from smartctl -A output into a SmartAttribute.
 *
 * Expected format (smartmontools 5.33):
 *   ID# ATTRIBUTE_NAME  FLAG  VALUE WORST THRESH TYPE  UPDATED WHEN_FAILED RAW
 *     1 Raw_Read_Error  0x000f  200   200   051  Pre-fail Always  -         0
 */
static BOOL ParseSmartCtlLine(const char *line, SmartAttribute *attr)
{
    unsigned int id, flags, value, worst, threshold;
    char name[64];

    int n = sscanf(line, " %u %63s 0x%x %u %u %u",
                   &id, name, &flags, &value, &worst, &threshold);
    if (n < 6 || id == 0 || id > 255)
        return FALSE;

    attr->id = (uint8)id;
    snprintf(attr->name, sizeof(attr->name), "%s", GetAttributeName((uint8)id));
    attr->value = (uint8)value;
    attr->worst = (uint8)worst;
    attr->threshold = (uint8)threshold;
    attr->status = SMART_STATUS_OK;

    /* Extract raw value from the last whitespace-delimited token on the line */
    attr->raw_value = 0;
    const char *p = line + strlen(line) - 1;
    while (p > line && (*p == ' ' || *p == '\n' || *p == '\r'))
        p--;
    while (p > line && *(p - 1) != ' ')
        p--;
    if (p > line) {
        char *endp;
        attr->raw_value = strtoull(p, &endp, 10);
        if (endp == p)
            attr->raw_value = 0;  /* Non-numeric (compound value), default to 0 */
    }

    return TRUE;
}

static BOOL TrySmartCtl(const char *device_name, uint32 unit, SmartData *out_data)
{
    if (!IsSmartCtlAvailable()) {
        LOG_DEBUG("TrySmartCtl: smartctl not available on this system");
        return FALSE;
    }

    LOG_DEBUG("TrySmartCtl: Running C:smartctl for %s:%lu",
              device_name, (unsigned long)unit);

    /* Build the command string with output redirect to T: */
    char cmd_str[256];
    snprintf(cmd_str, sizeof(cmd_str),
             "C:smartctl -HA %s:%lu >T:adb_smart.tmp",
             device_name, (unsigned long)unit);

    BPTR nil_in  = IDOS->Open("NIL:", MODE_OLDFILE);
    BPTR nil_out = IDOS->Open("NIL:", MODE_NEWFILE);
    if (!nil_in || !nil_out) {
        if (nil_in)  IDOS->Close(nil_in);
        if (nil_out) IDOS->Close(nil_out);
        LOG_DEBUG("TrySmartCtl: Failed to open NIL: handles");
        return FALSE;
    }

    /* SystemTags takes ownership of nil_in/nil_out */
    IDOS->SystemTags(cmd_str,
                     SYS_Input, nil_in,
                     SYS_Output, nil_out,
                     TAG_DONE);

    /* Parse the output file */
    BPTR fh = IDOS->Open("T:adb_smart.tmp", MODE_OLDFILE);
    if (!fh) {
        LOG_DEBUG("TrySmartCtl: Could not open T:adb_smart.tmp");
        return FALSE;
    }

    char line[256];
    BOOL in_attributes = FALSE;
    int attr_index = 0;

    while (IDOS->FGets(fh, line, sizeof(line))) {

        /* Detect overall health status line */
        if (strstr(line, "test result:")) {
            if (strstr(line, "PASSED"))
                out_data->overall_status = SMART_STATUS_OK;
            else if (strstr(line, "FAILED"))
                out_data->overall_status = SMART_STATUS_CRITICAL;
            continue;
        }

        /* Detect attribute table header */
        if (strstr(line, "ID#") && strstr(line, "ATTRIBUTE_NAME")) {
            in_attributes = TRUE;
            continue;
        }

        /* Parse attribute lines */
        if (in_attributes && attr_index < MAX_SMART_ATTRIBUTES) {
            const char *p = line;
            while (*p == ' ')
                p++;

            /* Attribute lines start with a digit; anything else ends the table */
            if (*p < '0' || *p > '9') {
                if (*p != '\0' && *p != '\n')
                    in_attributes = FALSE;
                continue;
            }

            SmartAttribute *attr = &out_data->attributes[attr_index];
            if (ParseSmartCtlLine(line, attr)) {
                /* Apply health heuristics (same logic as ParseSmartBuffers) */
                if (attr->id == 194)
                    out_data->temperature = (uint32)(attr->raw_value & 0xFF);
                if (attr->id == 9)
                    out_data->power_on_hours = (uint32)attr->raw_value;
                if (attr->id == 5) {
                    out_data->reallocated_sectors = (uint32)attr->raw_value;
                    if (out_data->reallocated_sectors > 0) {
                        attr->status = SMART_STATUS_WARNING;
                        if (out_data->overall_status < SMART_STATUS_WARNING)
                            out_data->overall_status = SMART_STATUS_WARNING;
                    }
                }
                if (attr->id == 197 || attr->id == 198) {
                    if (attr->raw_value > 0) {
                        attr->status = SMART_STATUS_CRITICAL;
                        out_data->overall_status = SMART_STATUS_CRITICAL;
                    }
                }
                /* Check threshold breach */
                if (attr->threshold > 0 && attr->value <= attr->threshold) {
                    attr->status = SMART_STATUS_CRITICAL;
                    out_data->overall_status = SMART_STATUS_CRITICAL;
                }
                attr_index++;
            }
        }
    }

    IDOS->Close(fh);
    IDOS->Delete("T:adb_smart.tmp");

    out_data->attribute_count = attr_index;
    if (attr_index > 0) {
        out_data->supported = TRUE;
        out_data->driver_supported = TRUE;
        out_data->method = SMART_METHOD_SMARTCTL;
        LOG_DEBUG("TrySmartCtl: Parsed %d attributes", attr_index);
    }

    return (attr_index > 0);
}


/* ========================================================================
 * Public API
 * ======================================================================== */

/**
 * Retrieve S.M.A.R.T. health data for a physical device.
 *
 * Opens the device once for Tier 1 (CMD_IDE) and Tier 2 (SAT), sharing
 * the I/O request between both attempts. Tier 3 (smartctl) manages its
 * own device access via the external process.
 *
 * @param device_name  Device driver name (e.g. "sb600sata.device")
 * @param unit         Device unit number
 * @param out_data     SmartData structure to populate
 * @return TRUE if any tier succeeded, FALSE if all failed
 */
BOOL GetSmartData(const char *device_name, uint32 unit, SmartData *out_data)
{
    if (!device_name || !out_data)
        return FALSE;

    memset(out_data, 0, sizeof(SmartData));
    out_data->overall_status = SMART_STATUS_UNKNOWN;
    out_data->method = SMART_METHOD_NONE;
    snprintf(out_data->health_summary, sizeof(out_data->health_summary),
             "Querying S.M.A.R.T. data...");

    /* Allocate I/O resources shared by Tier 1 and Tier 2 */
    struct MsgPort *port = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    if (!port) {
        snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                 "Failed to allocate message port.");
        return FALSE;
    }

    struct IOStdReq *ior = IExec->AllocSysObjectTags(ASOT_IOREQUEST,
                                                      ASOIOR_Size, sizeof(struct IOExtTD),
                                                      ASOIOR_ReplyPort, port,
                                                      TAG_DONE);
    if (!ior) {
        IExec->FreeSysObject(ASOT_PORT, port);
        snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                 "Failed to allocate I/O request.");
        return FALSE;
    }

    BOOL success = FALSE;

    if (IExec->OpenDevice(device_name, unit, (struct IORequest *)ior, 0) == 0) {

        /* Tier 1: CMD_IDE with direct ATA registers */
        success = TrySmartCmdIDE(ior, out_data);
        if (success) {
            LOG_DEBUG("GetSmartData: Tier 1 (CMD_IDE) succeeded for %s:%lu",
                      device_name, (unsigned long)unit);
        }

        /* Tier 2: HD_SCSICMD with ATA PASS-THROUGH (SAT) */
        if (!success) {
            success = TrySmartSAT(ior, out_data);
            if (success) {
                LOG_DEBUG("GetSmartData: Tier 2 (SAT) succeeded for %s:%lu",
                          device_name, (unsigned long)unit);
            }
        }

        IExec->CloseDevice((struct IORequest *)ior);
    } else {
        LOG_DEBUG("GetSmartData: OpenDevice failed for '%s' unit %lu",
                  device_name, (unsigned long)unit);
    }

    IExec->FreeSysObject(ASOT_IOREQUEST, ior);
    IExec->FreeSysObject(ASOT_PORT, port);

    /* Tier 3: External smartctl (uses its own device I/O) */
    if (!success) {
        success = TrySmartCtl(device_name, unit, out_data);
        if (success) {
            LOG_DEBUG("GetSmartData: Tier 3 (smartctl) succeeded for %s:%lu",
                      device_name, (unsigned long)unit);
        }
    }

    /* Build final health summary with method indicator */
    if (success) {
        const char *method_label = "";
        switch (out_data->method) {
        case SMART_METHOD_CMD_IDE:  method_label = "Direct ATA";       break;
        case SMART_METHOD_SAT:      method_label = "ATA Pass-Through"; break;
        case SMART_METHOD_SMARTCTL: method_label = "smartctl";         break;
        default:                    method_label = "Unknown";          break;
        }

        if (out_data->overall_status == SMART_STATUS_OK) {
            snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                     "Drive is healthy. (via %s)", method_label);
        } else {
            snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                     "Drive issues detected! (via %s)", method_label);
        }
    } else {
        snprintf(out_data->health_summary, sizeof(out_data->health_summary),
                 "S.M.A.R.T. not available. Driver does not support ATA passthrough.");
    }

    return success;
}
