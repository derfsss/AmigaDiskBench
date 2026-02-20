/*
 * AmigaDiskBench - Disk Information Tab (Master-Detail View)
 *
 * Implements a hierarchical view of Physical Drives and their Logical Partitions.
 * Master Pane: ListBrowser with hierarchical capabilities.
 * Detail Pane: Context-sensitive information pages.
 */

#include "engine_diskinfo.h"
#include "gui_internal.h"
#include <classes/window.h>

// Indices for Page Group
#define PAGE_INIT 0
#define PAGE_DRIVE 1
#define PAGE_PARTITION 2

struct InfoNodeData
{
    PhysicalDrive *drive;   // Pointer to physical drive data (cached)
    LogicalPartition *part; // Pointer to logical partition data (NULL if drive node)
    char *name_buffer;      // Allocated copy of display name
};

static struct List drive_tree_list; // Local list for the tree browser

static void FreeInfoNode(struct Node *node)
{
    struct InfoNodeData *data = NULL;
    IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &data, TAG_DONE);
    if (data) {
        if (data->name_buffer)
            IExec->FreeVec(data->name_buffer);
        // Note: PhysicalDrive/LogicalPartition data is owned by the engine list, not the node data
        IExec->FreeVec(data);
    }
    IListBrowser->FreeListBrowserNode(node);
}

static void ClearInfoTree(void)
{
    struct Node *node;
    if (ui.diskinfo_tree) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_tree, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }

    while ((node = IExec->RemHead(&drive_tree_list))) {
        FreeInfoNode(node);
    }
}

static void UpdateDetailsPage(struct InfoNodeData *data)
{
    LOG_DEBUG("UpdateDetailsPage: Entry with data=%p", data);

    if (!data) {
        LOG_DEBUG("UpdateDetailsPage: Data is NULL, showing Init Page");
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, PAGE_Current, PAGE_INIT,
                                   TAG_DONE);
        return;
    }

    // Partition Details
    if (data->part) {
        LogicalPartition *part = data->part;

        // Hide details if partition is not mounted
        if (part->volume_name[0] == '\0' || strcmp(part->volume_name, "Not Mounted") == 0) {
            LOG_DEBUG("UpdateDetailsPage: Partition not mounted, showing Init Page");
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, PAGE_Current, PAGE_INIT,
                                       TAG_DONE);
            IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
            return;
        }

        LOG_DEBUG("UpdateDetailsPage: Showing Partition Details for '%s' (addr: %p)", part->volume_name, part);

        static char s_part_vol[128];
        static char s_part_size[64];
        static char s_part_used[64];
        static char s_part_free[64];
        static char s_part_fs[128];
        static char s_part_block[64];

        LOG_DEBUG("UpdateDetailsPage: Formatting volume name");
        snprintf(s_part_vol, sizeof(s_part_vol), "%s", part->volume_name[0] ? part->volume_name : "Unmounted");
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_vol_label, ui.window, NULL, GA_Text, s_part_vol,
                                   TAG_DONE);

        LOG_DEBUG("UpdateDetailsPage: Formatting size (%llu)", part->size_bytes);
        FormatSize(part->size_bytes, s_part_size, sizeof(s_part_size));
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_size_label, ui.window, NULL, GA_Text, s_part_size,
                                   TAG_DONE);

        LOG_DEBUG("UpdateDetailsPage: Formatting used (%llu)", part->used_bytes);
        FormatSize(part->used_bytes, s_part_used, sizeof(s_part_used));
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_used_label, ui.window, NULL, GA_Text, s_part_used,
                                   TAG_DONE);

        LOG_DEBUG("UpdateDetailsPage: Formatting free (%llu)", part->free_bytes);
        FormatSize(part->free_bytes, s_part_free, sizeof(s_part_free));
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_free_label, ui.window, NULL, GA_Text, s_part_free,
                                   TAG_DONE);

        // Filesystem Name (Hex ID)
        LOG_DEBUG("UpdateDetailsPage: Formatting filesystem (0x%08X)", part->disk_environment_type);
        uint32 fs = part->disk_environment_type;
        const char *fs_str = GetDosTypeString(fs);
        LOG_DEBUG("UpdateDetailsPage: GetDosTypeString returned '%s'", fs_str ? fs_str : "NULL");

        if (fs_str) {
            snprintf(s_part_fs, sizeof(s_part_fs), "%s (0x%08lX)", fs_str, fs);
        } else {
            snprintf(s_part_fs, sizeof(s_part_fs), "Unknown (0x%08lX)", fs);
        }

        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_fs_label, ui.window, NULL, GA_Text, s_part_fs,
                                   TAG_DONE);

        LOG_DEBUG("UpdateDetailsPage: Formatting blocks (%lu)", part->blocks_per_drive);
        snprintf(s_part_block, sizeof(s_part_block), "%lu", part->blocks_per_drive);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_part_block_label, ui.window, NULL, GA_Text,
                                   s_part_block, TAG_DONE);

        LOG_DEBUG("UpdateDetailsPage: Changing page and rethinking layout");
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, PAGE_Current, PAGE_PARTITION,
                                   TAG_DONE);
        IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
        LOG_DEBUG("UpdateDetailsPage: Exit Partition Path");
        return;
    }

    // Drive Details
    PhysicalDrive *drive = data->drive;
    if (drive) {
        LOG_DEBUG("UpdateDetailsPage: Showing Drive Details for '%s'", drive->device_name);

        static char s_drive_brand[128];
        static char s_drive_bus[128];
        static char s_drive_cap[64];
        static char s_drive_geom[128];
        static char s_drive_flags[64];

        if (drive->revision[0] != '\0') {
            snprintf(s_drive_brand, sizeof(s_drive_brand), "%s %s (Rev: %s)", drive->vendor, drive->product,
                     drive->revision);
        } else {
            snprintf(s_drive_brand, sizeof(s_drive_brand), "%s %s", drive->vendor, drive->product);
        }
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_brand_label, ui.window, NULL, GA_Text, s_drive_brand,
                                   TAG_DONE);

        snprintf(s_drive_bus, sizeof(s_drive_bus), "%s %s", MediaTypeToString(drive->media_type),
                 BusTypeToString(drive->bus_type));
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_bus_label, ui.window, NULL, GA_Text, s_drive_bus,
                                   TAG_DONE);

        FormatSize(drive->capacity_bytes, s_drive_cap, sizeof(s_drive_cap));
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_capacity_label, ui.window, NULL, GA_Text, s_drive_cap,
                                   TAG_DONE);

        snprintf(s_drive_geom, sizeof(s_drive_geom), "C:%lu H:%lu S:%lu B:%lu", drive->cylinders, drive->heads,
                 drive->sectors, drive->block_bytes);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_geometry_label, ui.window, NULL, GA_Text, s_drive_geom,
                                   TAG_DONE);

        snprintf(s_drive_flags, sizeof(s_drive_flags), "RDB: %s", drive->rdb_found ? "Yes" : "No");
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_flags_label, ui.window, NULL, GA_Text, s_drive_flags,
                                   TAG_DONE);

        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, PAGE_Current, PAGE_DRIVE,
                                   TAG_DONE);
        IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
    } else {
        // Fallback for root node or category nodes
        LOG_DEBUG("UpdateDetailsPage: Data present but no Drive/Part (Root Node?)");
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, PAGE_Current, PAGE_INIT,
                                   TAG_DONE);
        IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
    }
}

void RefreshDiskInfoTree(void)
{
    LOG_DEBUG("RefreshDiskInfoTree: Entry");
    ClearInfoTree();
    IExec->NewList(&drive_tree_list);

    struct List *drives = ScanSystemDrives();
    if (!drives)
        return;

    static struct List *current_drives = NULL;
    if (current_drives) {
        FreePhysicalDriveList(current_drives);
    }
    current_drives = drives;

    const char *categories[] = {"Fixed Drives", "USB Drives", "Optical Drives"};

    for (int cat_idx = 0; cat_idx < 3; cat_idx++) {
        // Check if there are any drives in this category before creating the root node
        BOOL has_drives = FALSE;
        struct Node *dNode;
        for (dNode = IExec->GetHead(drives); dNode; dNode = IExec->GetSucc(dNode)) {
            PhysicalDrive *drive = (PhysicalDrive *)dNode;
            int drive_cat = 0; // Fixed
            if (drive->media_type == MEDIA_TYPE_CDROM) {
                drive_cat = 2; // Optical
            } else if (drive->bus_type == BUS_TYPE_USB) {
                drive_cat = 1; // USB
            }
            if (drive_cat == cat_idx) {
                has_drives = TRUE;
                break;
            }
        }

        if (!has_drives)
            continue;

        struct InfoNodeData *rootData =
            IExec->AllocVecTags(sizeof(struct InfoNodeData), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
        if (rootData) {
            struct Node *rootNode = IListBrowser->AllocListBrowserNode(
                1, LBNCA_Text, categories[cat_idx], LBNA_UserData, rootData, LBNA_Generation, 1, LBNA_Flags,
                LBFLG_HASCHILDREN | LBFLG_SHOWCHILDREN, TAG_DONE);
            if (rootNode) {
                IExec->AddTail(&drive_tree_list, rootNode);

                // Loop Drives for this category
                for (dNode = IExec->GetHead(drives); dNode; dNode = IExec->GetSucc(dNode)) {
                    PhysicalDrive *drive = (PhysicalDrive *)dNode;
                    int drive_cat = 0; // Fixed
                    if (drive->media_type == MEDIA_TYPE_CDROM) {
                        drive_cat = 2; // Optical
                    } else if (drive->bus_type == BUS_TYPE_USB) {
                        drive_cat = 1; // USB
                    }

                    if (drive_cat != cat_idx)
                        continue;

                    struct InfoNodeData *dData = IExec->AllocVecTags(sizeof(struct InfoNodeData), AVT_Type, MEMF_SHARED,
                                                                     AVT_ClearWithValue, 0, TAG_DONE);
                    char label[128];
                    snprintf(label, sizeof(label), "%s (Unit %lu)", drive->device_name, drive->unit_number);

                    if (dData) {
                        dData->drive = drive;
                        dData->name_buffer = IExec->AllocVecTags(strlen(label) + 1, AVT_Type, MEMF_SHARED, TAG_DONE);
                        if (dData->name_buffer)
                            strcpy(dData->name_buffer, label);

                        struct Node *lbDriveNode = IListBrowser->AllocListBrowserNode(
                            1, LBNCA_Text, dData->name_buffer, LBNA_UserData, dData, LBNA_Generation, 2, LBNA_Flags,
                            LBFLG_HASCHILDREN | LBFLG_SHOWCHILDREN, TAG_DONE);

                        if (lbDriveNode) {
                            IExec->AddTail(&drive_tree_list, lbDriveNode);

                            // Actual Partitions (direct children of the drive)
                            struct Node *pNode;
                            for (pNode = IExec->GetHead((struct List *)&drive->partitions); pNode;
                                 pNode = IExec->GetSucc(pNode)) {
                                LogicalPartition *part = (LogicalPartition *)pNode;
                                struct InfoNodeData *pData =
                                    IExec->AllocVecTags(sizeof(struct InfoNodeData), AVT_Type, MEMF_SHARED,
                                                        AVT_ClearWithValue, 0, TAG_DONE);

                                char pLabel[128];
                                snprintf(pLabel, sizeof(pLabel), "%s",
                                         part->volume_name[0] ? part->volume_name : part->dos_device_name);

                                if (pData) {
                                    pData->drive = drive; // Assign drive so parent details can be accessed if needed
                                    pData->part = part;
                                    pData->name_buffer =
                                        IExec->AllocVecTags(strlen(pLabel) + 1, AVT_Type, MEMF_SHARED, TAG_DONE);
                                    if (pData->name_buffer)
                                        strcpy(pData->name_buffer, pLabel);

                                    struct Node *lbPartNode = IListBrowser->AllocListBrowserNode(
                                        1, LBNCA_Text, pData->name_buffer, LBNA_UserData, pData, LBNA_Generation, 3,
                                        TAG_DONE);
                                    if (lbPartNode)
                                        IExec->AddTail(&drive_tree_list, lbPartNode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (ui.diskinfo_tree) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_tree, ui.window, NULL, LISTBROWSER_Labels,
                                   &drive_tree_list, LISTBROWSER_AutoFit, TRUE, TAG_DONE);

        // Expand root by default? No, let's select first drive if possible?
        // Or just select root?
        struct Node *head = IExec->GetHead(&drive_tree_list);
        if (head) {
            LOG_DEBUG("RefreshDiskInfoTree: Selecting head node");
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.diskinfo_tree, ui.window, NULL, LISTBROWSER_SelectedNode,
                                       head, TAG_DONE);
            UpdateDetailsPage(NULL); // Root select -> Init Page
        }

        IIntuition->RefreshGList((struct Gadget *)ui.diskinfo_tree, ui.window, NULL, 1);
    }
    LOG_DEBUG("RefreshDiskInfoTree: Exit");
}

void HandleDiskInfoEvent(uint32 result)
{
    LOG_DEBUG("HandleDiskInfoEvent: Tree Event Received (Result=0x%08X)", result);
    struct Node *node = NULL;
    struct InfoNodeData *data = NULL;
    IIntuition->GetAttr(LISTBROWSER_SelectedNode, ui.diskinfo_tree, (uint32 *)&node);
    if (node) {
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &data, TAG_DONE);
        LOG_DEBUG("HandleDiskInfoEvent: Node Selected, Data=%p", data);
        UpdateDetailsPage(data);
    } else {
        LOG_DEBUG("HandleDiskInfoEvent: No node selected");
    }
}

Object *CreateDiskInfoPage(void)
{
    IExec->NewList(&drive_tree_list);

    Object *page_init = VLayoutObject, LAYOUT_AddChild, SpaceObject, End, LAYOUT_AddChild, ButtonObject, GA_ReadOnly,
           TRUE, GA_Text, "Select a drive or partition.", BUTTON_BevelStyle, BVS_NONE, BUTTON_Transparent, TRUE,
           BUTTON_Justification, BCJ_CENTER, End, LAYOUT_AddChild, SpaceObject, End, End;

    /* Physical Drive Page */
    Object *page_drive = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_BevelStyle,
           BVS_GROUP, LAYOUT_Label, "Physical Drive Details",

           LAYOUT_AddChild,
           (ui.diskinfo_brand_label = ButtonObject, GA_ID, GID_DISKINFO_BRAND, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Model:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_bus_label = ButtonObject, GA_ID, GID_DISKINFO_BUS, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Type:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_capacity_label = ButtonObject, GA_ID, GID_DISKINFO_CAPACITY, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Capacity:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_geometry_label = ButtonObject, GA_ID, GID_DISKINFO_GEOMETRY, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Geometry:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_flags_label = ButtonObject, GA_ID, GID_DISKINFO_FLAGS, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "RDB:", End, CHILD_WeightedHeight, 0,

           End, CHILD_WeightedHeight, 0, // Prevent BevelFrame from growing vertically

        // Spacer at the bottom to push content up
        LAYOUT_AddChild, SpaceObject, End, CHILD_WeightedHeight, 100, End;

    /* Partition Page */
    Object *page_part = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_BevelStyle,
           BVS_GROUP, LAYOUT_Label, "Partition Details",

           LAYOUT_AddChild,
           (ui.diskinfo_part_vol_label = ButtonObject, GA_ID, GID_DISKINFO_PART_VOL, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Volume:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_part_size_label = ButtonObject, GA_ID, GID_DISKINFO_PART_SIZE, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Size:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_part_used_label = ButtonObject, GA_ID, GID_DISKINFO_PART_USED, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Used:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_part_free_label = ButtonObject, GA_ID, GID_DISKINFO_PART_FREE, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Free:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_part_fs_label = ButtonObject, GA_ID, GID_DISKINFO_PART_FS, GA_ReadOnly, TRUE, GA_Text, "-",
            BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Filesystem:", End, CHILD_WeightedHeight, 0,

           LAYOUT_AddChild,
           (ui.diskinfo_part_block_label = ButtonObject, GA_ID, GID_DISKINFO_PART_BLOCK, GA_ReadOnly, TRUE, GA_Text,
            "-", BUTTON_Justification, BCJ_LEFT, End),
           CHILD_Label, LabelObject, LABEL_Text, "Blocks:", End, CHILD_WeightedHeight, 0,

           End, CHILD_WeightedHeight, 0, // Prevent BevelFrame from growing vertically

        // Spacer at the bottom
        LAYOUT_AddChild, SpaceObject, End, CHILD_WeightedHeight, 100, End;

    ui.diskinfo_pages = IIntuition->NewObject(NULL, "page.gadget", PAGE_Add, (uint32)page_init, PAGE_Add,
                                              (uint32)page_drive, PAGE_Add, (uint32)page_part, TAG_DONE);

    static struct ColumnInfo diskinfo_cols[] = {{100, "Device / Partition", CIF_WEIGHTED | CIF_DRAGGABLE},
                                                {-1, NULL, 0}};

    Object *tree =
        (ui.diskinfo_tree = ListBrowserObject, GA_ID, GID_DISKINFO_TREE, GA_RelVerify, TRUE, LISTBROWSER_ShowSelected,
         TRUE, LISTBROWSER_Hierarchical, TRUE, // Enable Tree Mode
         LISTBROWSER_ColumnInfo, (uint32)diskinfo_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_AutoFit, TRUE,
         // IPC? No, local list.
         End);

    Object *left_pane = VLayoutObject, LAYOUT_AddChild, tree, CHILD_WeightedHeight, 100, LAYOUT_AddChild, ButtonObject,
           GA_ID, GID_DISKINFO_REFRESH, GA_Text, "Refresh Drives", GA_RelVerify, TRUE, End, CHILD_WeightedHeight, 0,
           End;

    return HLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, left_pane, CHILD_WeightedWidth, 40, LAYOUT_AddChild,
           ui.diskinfo_pages, CHILD_WeightedWidth, 60, End;
}
