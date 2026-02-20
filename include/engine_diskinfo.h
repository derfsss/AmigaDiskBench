#ifndef ENGINE_DISKINFO_H
#define ENGINE_DISKINFO_H

#include <devices/hardblocks.h>
#include <dos/dos.h>
#include <exec/nodes.h>
#include <exec/types.h>

// Enums for Drive Characterization
typedef enum
{
    BUS_TYPE_UNKNOWN = 0,
    BUS_TYPE_PATA, // a1ide, etc.
    BUS_TYPE_SATA, // sata.device
    BUS_TYPE_SCSI, // scsi.device, cybppc.device
    BUS_TYPE_USB,  // usbmassstorage.device
    BUS_TYPE_NVME  // nvme.device
} BusType;

typedef enum
{
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_TYPE_HDD,
    MEDIA_TYPE_SSD,
    MEDIA_TYPE_CDROM,
    MEDIA_TYPE_FLOPPY
} MediaType;

// Logical Partition Node (Child of PhysicalDrive)
typedef struct LogicalPartition
{
    struct Node node;
    char dos_device_name[32]; // e.g. "DH0"
    char volume_name[64];     // e.g. "System"
    uint32 dos_type;          // e.g. 0x444F5303 (DOS3)
    uint64 size_bytes;
    uint64 used_bytes;
    uint64 free_bytes; // Derived/Queried
    uint32 block_size;
    uint32 blocks_per_drive;      // Total blocks
    uint32 disk_environment_type; // e.g. DOSType
    BOOL bootable;
} LogicalPartition;

// Physical Drive Node (Parent)
typedef struct PhysicalDrive
{
    struct Node node;

    // Identity
    char device_name[32]; // e.g. "a1ide.device"
    uint32 unit_number;   // e.g. 0
    char vendor[32];      // From RDB or Inquiry
    char product[64];     // From RDB or Inquiry
    char revision[16];    // From RDB or Inquiry
    char serial[32];      // From Inquiry (if available)

    // Characterization
    BusType bus_type;
    MediaType media_type;
    BOOL is_removable;

    // Physical Geometry
    uint32 cylinders;
    uint32 heads;
    uint32 sectors;
    uint32 block_bytes;
    uint64 capacity_bytes;

    // RDB Data (if found)
    BOOL rdb_found;
    struct RigidDiskBlock rdb; // Copy of RDB if present

    // Lists
    struct MinList partitions; // List of LogicalPartition
} PhysicalDrive;

// Function Prototypes
struct List *ScanSystemDrives(void);
void FreePhysicalDriveList(struct List *list);
const char *BusTypeToString(BusType type);
const char *MediaTypeToString(MediaType type);
const char *GetDosTypeString(uint32 dostype);

#endif // ENGINE_DISKINFO_H
