#include "gui.h"
#include "version.h"
#include <classes/arexx.h>
#include <interfaces/application.h>
#include <interfaces/arexx.h>
#include <interfaces/icon.h>
#include <interfaces/locale.h>
#include <libraries/application.h>
#include <proto/application.h>
#include <proto/asl.h>
#include <proto/icon.h>
#include <proto/locale.h>
#include <reaction/reaction_macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* [v1.9.9] Redundant connectivity macros removed; using reaction_macros.h */
#include "debug.h"
#include "engine.h"
#include "gui.h"
#include "gui_details_window.h"
#include "version.h"
#include <exec/types.h>

/* Prototypes for local utilities */
const char *FormatByteSize(uint64 bytes);
/* Global UI state */
#define DEFAULT_CSV_PATH "PROGDIR:bench_history.csv"
#define DEFAULT_LAST_TEST 3      /* 3 = All Tests (default) */
#define DEFAULT_BLOCK_SIZE_IDX 0 /* 0 = 4K (default) */
#define DEFAULT_PASSES 3
#define DEFAULT_TRIMMED_MEAN TRUE
GUIState ui;
/* Data structure for drive selection nodes */
typedef struct
{
    STRPTR bare_name;
    STRPTR display_name;
} DriveNodeData;
const char *FormatPresetBlockSize(uint32 bytes)
{
    if (bytes == 4096)
        return "4K";
    if (bytes == 16384)
        return "16K";
    if (bytes == 32768)
        return "32K";
    if (bytes == 65536)
        return "64K";
    if (bytes == 262144)
        return "256K";
    if (bytes == 1048576)
        return "1M";
    static char custom[32];
    if (bytes < 1024)
        snprintf(custom, sizeof(custom), "%uB", (unsigned int)bytes);
    else if (bytes < 1048576)
        snprintf(custom, sizeof(custom), "%uK", (unsigned int)(bytes / 1024));
    else
        snprintf(custom, sizeof(custom), "%uM", (unsigned int)(bytes / 1048576));
    return custom;
}
const char *FormatByteSize(uint64 bytes)
{
    static char buffer[64];
    if (bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1048576) {
        snprintf(buffer, sizeof(buffer), "%.2f KB", (double)bytes / 1024.0);
    } else if (bytes < 1073741824) {
        snprintf(buffer, sizeof(buffer), "%.2f MB", (double)bytes / 1048576.0);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f GB", (double)bytes / 1073741824.0);
    }
    return buffer;
}
static CONST_STRPTR GetString(uint32 id, CONST_STRPTR default_str)
{
    return (ui.catalog && ui.ILoc) ? ui.ILoc->GetCatalogStr(ui.catalog, id, default_str) : default_str;
}
static void RefreshHistory(void)
{
    /* Detach list before modification */
    if (ui.window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }
    /* Free existing nodes and their associated UserData */
    struct Node *node, *next;
    node = IExec->GetHead(&ui.history_labels);
    while (node) {
        next = IExec->GetSucc(node);
        void *udata = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &udata, TAG_DONE);
        if (udata)
            IExec->FreeVec(udata);
        IListBrowser->FreeListBrowserNode(node);
        node = next;
    }
    IExec->NewList(&ui.history_labels);
    /* [v1.8.1 Polish] Open CSV and parse */
    LOG_DEBUG("RefreshHistory: Attempting to open '%s'", ui.csv_path);
    BPTR file = IDOS->FOpen(ui.csv_path, MODE_OLDFILE, 0);
    int count = 0;
    if (file) {
        LOG_DEBUG("RefreshHistory: Opened CSV file");
        char line[1024];
        BOOL first = TRUE;
        while (IDOS->FGets(file, line, sizeof(line))) {
            if (first) {
                first = FALSE;
                continue;
            } /* Skip header */
            /* Remove trailing newline for sscanf robustness */
            char *nl = strpbrk(line, "\r\n");
            if (nl)
                *nl = '\0';
            if (line[0] == '\0')
                continue;

            char id[32], timestamp[32], type[64], disk[64], fs[128], mbs_str[32], iops_str[32], device[64],
                unit_str[32], ver[32], passes[16], bs_str[32], trimmed[16], min_str[32], max_str[32], dur_str[32],
                bytes_str[32], vendor[32], product[64];

            /* Clear strings */
            id[0] = timestamp[0] = type[0] = disk[0] = fs[0] = mbs_str[0] = iops_str[0] = device[0] = unit_str[0] = 0;
            ver[0] = passes[0] = bs_str[0] = trimmed[0] = min_str[0] = max_str[0] = dur_str[0] = bytes_str[0] = 0;
            vendor[0] = product[0] = 0;

            /* Parse CSV based on column count */
            int fields = sscanf(line,
                                "%31[^,],%31[^,],%63[^,],%63[^,],%127[^,],%31[^,],%31[^,],%63[^,],%31[^,],%31[^,],%15[^"
                                ",],%31[^,],%15[^,],%31[^,],%31[^,],%31[^,],%31[^,],%31[^,],%63s",
                                id, timestamp, type, disk, fs, mbs_str, iops_str, device, unit_str, ver, passes, bs_str,
                                trimmed, min_str, max_str, dur_str, bytes_str, vendor, product);

            /* Shift data if first field is not a unique ID (legacy format) */
            if (fields == 11 && strchr(id, '-') && !strchr(id, '_')) {
                snprintf(product, sizeof(product), "%s", vendor);
                snprintf(vendor, sizeof(vendor), "%s", bytes_str);
                snprintf(bytes_str, sizeof(bytes_str), "%s", dur_str);
                snprintf(dur_str, sizeof(dur_str), "%s", max_str);
                snprintf(max_str, sizeof(max_str), "%s", min_str);
                snprintf(min_str, sizeof(min_str), "%s", trimmed);
                snprintf(trimmed, sizeof(trimmed), "%s", bs_str);
                snprintf(bs_str, sizeof(bs_str), "%s", passes);
                snprintf(passes, sizeof(passes), "%s", ver);
                snprintf(ver, sizeof(ver), "%s", unit_str);
                snprintf(unit_str, sizeof(unit_str), "%s", device);
                snprintf(device, sizeof(device), "%s", iops_str);
                snprintf(iops_str, sizeof(iops_str), "%s", mbs_str);
                snprintf(mbs_str, sizeof(mbs_str), "%s", fs);
                snprintf(fs, sizeof(fs), "%s", disk);
                snprintf(disk, sizeof(disk), "%s", type);
                snprintf(type, sizeof(type), "%s", timestamp);
                snprintf(timestamp, sizeof(timestamp), "%s", id);
                snprintf(id, sizeof(id), "N/A");
            }

            /* Prepare BenchResult for UserData */
            BenchResult *res =
                IExec->AllocVecTags(sizeof(BenchResult), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
            if (res) {
                snprintf(res->result_id, sizeof(res->result_id), "%s", (fields >= 12) ? id : "N/A");
                snprintf(res->timestamp, sizeof(res->timestamp), "%s", timestamp);
                snprintf(res->volume_name, sizeof(res->volume_name), "%s", disk);
                snprintf(res->fs_type, sizeof(res->fs_type), "%s", fs);
                res->mb_per_sec = atof(mbs_str);
                res->iops = strtoul(iops_str, NULL, 10);
                snprintf(res->device_name, sizeof(res->device_name), "%s", device);
                res->device_unit = strtoul(unit_str, NULL, 10);
                snprintf(res->app_version, sizeof(res->app_version), "%s", ver);
                res->passes = strtoul(passes, NULL, 10);
                res->block_size = strtoul(bs_str, NULL, 10);
                res->use_trimmed_mean = (fields >= 13) ? strtoul(trimmed, NULL, 10) : 0;
                res->min_mbps = (fields >= 14) ? atof(min_str) : res->mb_per_sec;
                res->max_mbps = (fields >= 15) ? atof(max_str) : res->mb_per_sec;
                res->total_duration = (fields >= 16) ? atof(dur_str) : 0;
                res->cumulative_bytes = (fields >= 17) ? strtoull(bytes_str, NULL, 10) : 0;
                snprintf(res->vendor, sizeof(res->vendor), "%s", (fields >= 18) ? vendor : "N/A");
                snprintf(res->product, sizeof(res->product), "%s", (fields >= 19) ? product : "N/A");

                if (strstr(type, "Sprinter"))
                    res->type = TEST_SPRINTER;
                else if (strstr(type, "Heavy"))
                    res->type = TEST_HEAVY_LIFTER;
                else if (strstr(type, "Legacy"))
                    res->type = TEST_LEGACY;
                else if (strstr(type, "Daily"))
                    res->type = TEST_DAILY_GRIND;
            }

            struct Node *hnode = IListBrowser->AllocListBrowserNode(
                11, LBNA_Column, COL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)timestamp, LBNA_Column, COL_VOL,
                LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)disk, LBNA_Column, COL_TEST, LBNCA_CopyText, TRUE, LBNCA_Text,
                (uint32)type, LBNA_Column, COL_BS, LBNCA_CopyText, TRUE, LBNCA_Text,
                (uint32)FormatPresetBlockSize(strtoul(bs_str, NULL, 10)), LBNA_Column, COL_PASSES, LBNCA_CopyText, TRUE,
                LBNCA_Text, (uint32)passes, LBNA_Column, COL_MBPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)mbs_str,
                LBNA_Column, COL_IOPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)iops_str, LBNA_Column, COL_DEVICE,
                LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)device, LBNA_Column, COL_UNIT, LBNCA_CopyText, TRUE,
                LBNCA_Text, (uint32)unit_str, LBNA_Column, COL_VER, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ver,
                LBNA_Column, COL_DUMMY, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32) "", LBNA_UserData, (uint32)res,
                TAG_DONE);
            if (hnode) {
                /* Add HEAD to make newest appear at the top */
                IExec->AddHead(&ui.history_labels, hnode);
                count++;
            } else if (res) {
                IExec->FreeVec(res);
            }
        }
        IDOS->Close(file);
        LOG_DEBUG("RefreshHistory: Loaded %d records", count);
    } else {
        /* [v1.8.3] Create empty history file with full header if it doesn't exist */
        file = IDOS->FOpen(ui.csv_path, MODE_NEWFILE, 0);
        if (file) {
            IDOS->FPuts(file, "ID,DateTime,Type,Volume,FS,MB/"
                              "s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize,Trimmed,Min,Max,Duration,TotalBytes,"
                              "Vendor,Product\n");
            IDOS->Close(file);
            LOG_DEBUG("RefreshHistory: Created new CSV at '%s'", ui.csv_path);
        } else {
            LOG_DEBUG("RefreshHistory: Could not open/create CSV at '%s'", ui.csv_path);
        }
    }
    if (count == 0) {
        LOG_DEBUG("RefreshHistory: No records found, creating/checking '%s'", ui.csv_path);
        BPTR file = IDOS->FOpen(ui.csv_path, MODE_OLDFILE, 0);
        if (!file) {
            LOG_DEBUG("RefreshHistory: Creating new CSV file at '%s'", ui.csv_path);
            file = IDOS->FOpen(ui.csv_path, MODE_NEWFILE, 0);
            if (file) {
                IDOS->FPuts(file, "DateTime,Type,Volume,FS,MB/s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize\n");
                IDOS->FClose(file);
            }
        } else {
            IDOS->FClose(file);
        }
    }
    /* Reattach list and refresh UI */
    if (ui.window && ui.history_list) {
        LOG_DEBUG("RefreshHistory: Reattaching list to %p", ui.history_list);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels,
                                   &ui.history_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
        IIntuition->RefreshGList((struct Gadget *)ui.history_list, ui.window, NULL, 1);
    } else {
        LOG_DEBUG("RefreshHistory: Skiping UI update, win=%p, list=%p", ui.window, ui.history_list);
    }
}
static void LoadPrefs(void)
{
    LOG_DEBUG("LoadPrefs: Started");
    if (!ui.app_id || !ui.ApplicationBase) {
        LOG_DEBUG("LoadPrefs: Aborting (missing app_id or library)");
        return;
    }
    struct PrefsObjectsIFace *IPrefs = ui.IPrefsObjects;
    if (!IPrefs) {
        LOG_DEBUG("LoadPrefs: IPrefsObjects interface not available");
        return;
    }
    PrefsObject *dict = NULL;
    /*
     * Get the main preferences dictionary from application.library.
     * This is the live object managed by the library.
     * We do not own this object, so we do not release it.
     */
    ui.IApp->GetApplicationAttrs(ui.app_id, APPATTR_MainPrefsDict, (uint32 *)&dict, TAG_DONE);
    if (dict) {
        LOG_DEBUG("LoadPrefs: Found preferences dictionary at %p", dict);
        int32 count = 0;
        IPrefs->PrefsBaseObject(dict, NULL, ALPODICT_GetCount, &count, TAG_DONE);
        /* LOG_DEBUG("LoadPrefs: Dictionary contains %d items", (int)count); */
        if (count == 0) {
            LOG_DEBUG("LoadPrefs: Dictionary is empty, populating with defaults and flushing to disk");
            /*
             * Populate default values using PrefsObjects.
             * Note: DictSetObjectForKey transfers ownership of the object to the dictionary.
             * We only ALPO_Release if the Set operation FAILS.
             */
            BOOL res;
            PrefsObject *obj;
            obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, DEFAULT_LAST_TEST, TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "DefaultTestType");
            LOG_DEBUG("LoadPrefs: Set DefaultTestType result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            /* DefaultDrive (String) */
            obj = IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString, "", TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "DefaultDrive");
            LOG_DEBUG("LoadPrefs: Set DefaultDrive result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, DEFAULT_BLOCK_SIZE_IDX, TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "DefaultBS");
            LOG_DEBUG("LoadPrefs: Set DefaultBS result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, DEFAULT_PASSES, TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "DefaultPasses");
            LOG_DEBUG("LoadPrefs: Set DefaultPasses result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetBool, DEFAULT_TRIMMED_MEAN, TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "TrimmedMean");
            LOG_DEBUG("LoadPrefs: Set TrimmedMean result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            obj = IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString, DEFAULT_CSV_PATH, TAG_DONE);
            res = IPrefs->DictSetObjectForKey(dict, obj, "CSVPath");
            LOG_DEBUG("LoadPrefs: Set CSVPath result=%d (obj=%p)", res, obj);
            if (!res && obj)
                IPrefs->PrefsBaseObject(obj, NULL, ALPO_Release, 0, TAG_DONE);
            ui.IApp->SetApplicationAttrs(ui.app_id, APPATTR_SavePrefs, TRUE, APPATTR_FlushPrefs, TRUE, TAG_DONE);
        }
        uint32 test_sel = IPrefs->DictGetIntegerForKey(dict, "DefaultTestType", DEFAULT_LAST_TEST);
        /* Fallback: Check legacy "LastTest" for migration if DefaultTestType missing?
           DictGetIntegerForKey returns default if missing, so we just use that. */
        ui.default_test_type = test_sel;
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.test_chooser, ui.window, NULL, CHOOSER_Selected, test_sel,
                                   TAG_DONE);
        uint32 bs_sel = IPrefs->DictGetIntegerForKey(dict, "DefaultBS", DEFAULT_BLOCK_SIZE_IDX);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.block_chooser, ui.window, NULL, CHOOSER_Selected, bs_sel,
                                   TAG_DONE);
        ui.default_block_size_idx = bs_sel; /* Store for prefs window */
        CONST_STRPTR def_drive = IPrefs->DictGetStringForKey(dict, "DefaultDrive", "");
        if (def_drive) {
            strncpy(ui.default_drive, def_drive, sizeof(ui.default_drive) - 1);
            ui.default_drive[sizeof(ui.default_drive) - 1] = '\0';
        } else {
            ui.default_drive[0] = '\0';
        }
        uint32 p_num = IPrefs->DictGetIntegerForKey(dict, "DefaultPasses", DEFAULT_PASSES);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.pass_gad, ui.window, NULL, INTEGER_Number, p_num, TAG_DONE);
        ui.use_trimmed_mean = IPrefs->DictGetBoolForKey(dict, "TrimmedMean", DEFAULT_TRIMMED_MEAN);
        CONST_STRPTR p = IPrefs->DictGetStringForKey(dict, "CSVPath", DEFAULT_CSV_PATH);
        LOG_DEBUG("LoadPrefs: DictGetStringForKey(CSVPath) returned '%s'", p ? p : "NULL");
        if (p) {
            strncpy(ui.csv_path, p, sizeof(ui.csv_path) - 1);
            ui.csv_path[sizeof(ui.csv_path) - 1] = '\0';
        } else {
            strcpy(ui.csv_path, DEFAULT_CSV_PATH);
        }
    } else {
        LOG_DEBUG("LoadPrefs: No preferences dictionary found (using defaults)");
        /* Defaults if no dict */
        ui.use_trimmed_mean = DEFAULT_TRIMMED_MEAN;
        strcpy(ui.csv_path, DEFAULT_CSV_PATH);
    }
    LOG_DEBUG("LoadPrefs: Finished");
}
static void BrowseCSV(void)
{
    if (!ui.IAsl)
        return;
    struct FileRequester *req =
        ui.IAsl->AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (uint32) "Select CSV History File",
                                     ASLFR_DoSaveMode, TRUE, ASLFR_InitialFile, (uint32) "bench_history.csv", TAG_DONE);
    if (req) {
        if (ui.IAsl->AslRequestTags(req, ASLFR_Window, (uint32)ui.prefs_window, TAG_DONE)) {
            char path[512];
            strncpy(path, req->fr_Drawer, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
            IDOS->AddPart(path, req->fr_File, sizeof(path));
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.prefs_csv_path, ui.prefs_window, NULL, STRINGA_TextVal,
                                       (uint32)path, TAG_DONE);
        }
        ui.IAsl->FreeAslRequest(req);
    }
}
static void OpenPrefsWindow(void)
{
    if (ui.prefs_win_obj)
        return;
    /* Get current values from active state */
    uint32 num_passes = 3;
    IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &num_passes);
    BOOL trimmed = ui.use_trimmed_mean;
    char *csv_path = ui.csv_path;
    /* [v1.9.9] Refactored Preferences window using standard ReAction macros */
    ui.prefs_win_obj = WindowObject, WA_Title, "Preferences", WA_SizeGadget, TRUE, WA_CloseGadget, TRUE, WA_DragBar,
    TRUE, WA_Activate, TRUE, WA_InnerWidth, 400, WA_InnerHeight, 180, WINDOW_SharedPort, (uint32)ui.prefs_port,
    WINDOW_ParentGroup, VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_BevelStyle,
    BVS_GROUP, LAYOUT_Label, "Default Settings", LAYOUT_AddChild,
    (ui.prefs_target_chooser = ChooserObject, GA_ID, GID_PREFS_TARGET, GA_RelVerify, TRUE, CHOOSER_Labels,
     (uint32)&ui.drive_list, /* Re-use main drive list */
     CHOOSER_Selected, 0,    /* Will be updated after window open if default_drive matches */
     End),
    CHILD_Label, LabelObject, LABEL_Text, "Default Drive:", End, LAYOUT_AddChild,
    (ui.prefs_test_chooser = ChooserObject, GA_ID, GID_PREFS_TEST_TYPE, GA_RelVerify, TRUE, CHOOSER_Selected,
     ui.default_test_type, CHOOSER_Labels, (uint32)&ui.test_labels, End),
    CHILD_Label, LabelObject, LABEL_Text, "Default Test:", End, LAYOUT_AddChild,
    (ui.prefs_block_chooser = ChooserObject, GA_ID, GID_PREFS_BLOCK, GA_RelVerify, TRUE, CHOOSER_Selected,
     ui.default_block_size_idx, CHOOSER_Labels, (uint32)&ui.block_list, End),
    CHILD_Label, LabelObject, LABEL_Text, "Default Block:", End, LAYOUT_AddChild,
    (ui.prefs_pass_gad = IntegerObject, GA_ID, GID_PREFS_PASSES, GA_RelVerify, TRUE, INTEGER_Minimum, 3,
     INTEGER_Maximum, 20, INTEGER_Number, num_passes, End),
    CHILD_Label, LabelObject, LABEL_Text, "Default Passes:", End, LAYOUT_AddChild,
    (ui.prefs_trimmed_check = CheckBoxObject, GA_ID, GID_PREFS_TRIMMED, GA_RelVerify, TRUE, GA_Selected, trimmed,
     GA_Text, "Use Trimmed Mean", End),
    End, LAYOUT_AddChild, HLayoutObject, LAYOUT_BevelStyle, BVS_GROUP, LAYOUT_Label, "Storage", LAYOUT_AddChild,
    (ui.prefs_csv_path = StringObject, GA_ID, GID_PREFS_CSV, GA_RelVerify, TRUE, STRINGA_TextVal, (uint32)csv_path,
     End),
    CHILD_Label, LabelObject, LABEL_Text, "CSV Path:", End, LAYOUT_AddChild, ButtonObject, GA_ID, GID_PREFS_CSV_BR,
    GA_Text, "...", GA_RelVerify, TRUE, End, CHILD_WeightedWidth, 0, End, LAYOUT_AddChild, HLayoutObject,
    LAYOUT_AddChild, ButtonObject, GA_ID, GID_PREFS_SAVE, GA_Text, "Save", GA_RelVerify, TRUE, End, LAYOUT_AddChild,
    ButtonObject, GA_ID, GID_PREFS_CANCEL, GA_Text, "Cancel", GA_RelVerify, TRUE, End, End, CHILD_WeightedHeight, 0,
    End, End;
    if (ui.prefs_win_obj) {
        ui.prefs_window = (struct Window *)IIntuition->IDoMethod(ui.prefs_win_obj, WM_OPEN);
        /* Set initial selection for Drive Chooser based on default_drive */
        if (ui.prefs_window && ui.prefs_target_chooser && ui.default_drive[0] != '\0') {
            struct Node *vn;
            for (vn = IExec->GetHead(&ui.drive_list); vn; vn = IExec->GetSucc(vn)) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name && strcasecmp(dd->bare_name, ui.default_drive) == 0) {
                    IIntuition->SetGadgetAttrs((struct Gadget *)ui.prefs_target_chooser, ui.prefs_window, NULL,
                                               CHOOSER_SelectedNode, vn, TAG_DONE);
                    break;
                }
            }
        }
    }
}
static void FormatSize(uint64 bytes, char *out)
{
    if (bytes >= (1024ULL * 1024 * 1024 * 1024))
        sprintf(out, "%.1f TB", (double)bytes / (1024.0 * 1024 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024 * 1024))
        sprintf(out, "%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024))
        sprintf(out, "%.1f MB", (double)bytes / (1024.0 * 1024));
    else
        sprintf(out, "%llu KB", bytes / 1024);
}
/* Data structure for drive selection nodes */
static void RefreshDriveList(void)
{
    if (ui.target_chooser && ui.window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL, CHOOSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }
    /* Also detach from prefs chooser if open */
    if (ui.prefs_target_chooser && ui.prefs_window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.prefs_target_chooser, ui.prefs_window, NULL, CHOOSER_Labels,
                                   (ULONG)-1, TAG_DONE);
    }
    /* Free existing nodes and their custom data */
    struct Node *node, *next;
    node = IExec->GetHead(&ui.drive_list);
    while (node) {
        next = IExec->GetSucc(node);
        DriveNodeData *ddata = NULL;
        IChooser->GetChooserNodeAttrs(node, CNA_UserData, &ddata, TAG_DONE);
        if (ddata) {
            if (ddata->bare_name)
                IExec->FreeVec(ddata->bare_name);
            if (ddata->display_name)
                IExec->FreeVec(ddata->display_name);
            IExec->FreeVec(ddata);
        }
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.drive_list);
    /* Use modern DOS_VOLUMELIST to iterate directly without BSTRs */
    struct List *vol_list =
        IDOS->AllocDosObjectTags(DOS_VOLUMELIST, ADO_Type, LDF_VOLUMES, ADO_AddColon, TRUE, TAG_DONE);
    if (vol_list) {
        struct Node *vol_node;
        for (vol_node = IExec->GetHead(vol_list); vol_node != NULL; vol_node = IExec->GetSucc(vol_node)) {
            char bare_name[256];
            char detailed_name[512];
            char fs_info[64];
            /* vol_node->ln_Name is already a C-string (STRPTR) from DOS_VOLUMELIST */
            snprintf(bare_name, sizeof(bare_name), "%s", vol_node->ln_Name);
            /* Get FS type from engine's helper */
            GetFileSystemInfo(bare_name, fs_info, sizeof(fs_info));
            /* Get Space info and check for Writable state */
            struct InfoData *info = IDOS->AllocDosObject(DOS_INFODATA, NULL);
            if (info) {
                if (IDOS->GetDiskInfoTags(GDI_StringNameInput, bare_name, GDI_InfoData, info, TAG_DONE)) {
                    /* Filter: Only include writable (validated) volumes */
                    if (info->id_DiskState == ID_DISKSTATE_VALIDATED) {
                        uint64 total = (uint64)info->id_NumBlocks * info->id_BytesPerBlock;
                        uint64 free = (uint64)(info->id_NumBlocks - info->id_NumBlocksUsed) * info->id_BytesPerBlock;
                        char sz_total[32], sz_free[32];
                        FormatSize(total, sz_total);
                        FormatSize(free, sz_free);
                        snprintf(detailed_name, sizeof(detailed_name), "%s [%s]", bare_name, fs_info);
                        /* Store names in DriveNodeData for the chooser node */
                        DriveNodeData *ddata = IExec->AllocVecTags(sizeof(DriveNodeData), AVT_Type, MEMF_SHARED,
                                                                   AVT_ClearWithValue, 0, TAG_DONE);
                        if (ddata) {
                            ddata->bare_name =
                                IExec->AllocVecTags(strlen(bare_name) + 1, AVT_Type, MEMF_SHARED, TAG_DONE);
                            ddata->display_name =
                                IExec->AllocVecTags(strlen(detailed_name) + 1, AVT_Type, MEMF_SHARED, TAG_DONE);
                            if (ddata->bare_name && ddata->display_name) {
                                strcpy(ddata->bare_name, bare_name);
                                strcpy(ddata->display_name, detailed_name);
                                struct Node *cnode = IChooser->AllocChooserNode(
                                    CNA_Text, ddata->display_name, CNA_CopyText, FALSE, CNA_UserData, ddata, TAG_DONE);
                                if (cnode) {
                                    IExec->AddTail(&ui.drive_list, cnode);
                                } else {
                                    IExec->FreeVec(ddata->bare_name);
                                    IExec->FreeVec(ddata->display_name);
                                    IExec->FreeVec(ddata);
                                }
                            } else {
                                if (ddata->bare_name)
                                    IExec->FreeVec(ddata->bare_name);
                                if (ddata->display_name)
                                    IExec->FreeVec(ddata->display_name);
                                IExec->FreeVec(ddata);
                            }
                        }
                    }
                }
                IDOS->FreeDosObject(DOS_INFODATA, info);
            }
        }
        IDOS->FreeDosObject(DOS_VOLUMELIST, vol_list);
    }
    if (ui.target_chooser && ui.window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL, CHOOSER_Labels, &ui.drive_list,
                                   TAG_DONE);
    }
    if (ui.prefs_target_chooser && ui.prefs_window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.prefs_target_chooser, ui.prefs_window, NULL, CHOOSER_Labels,
                                   &ui.drive_list, TAG_DONE);
    }
    LOG_DEBUG("Drive list refreshed with modern DOS_VOLUMELIST API (Filtered for Writable)");
}
/* ARexx Command Callbacks [STUBS - Commented out to fix warnings until implemented]
static void ARexx_Start(struct ARexxCmd *ac, struct RexxMsg *msg)
{
    ac->ac_RC = (int32)0;
}
static void ARexx_Stop(struct ARexxCmd *ac, struct RexxMsg *msg)
{
    ac->ac_RC = (int32)0;
}
static void ARexx_GetResults(struct ARexxCmd *ac, struct RexxMsg *msg)
{
    ac->ac_RC = (int32)0;
}
static void ARexx_Quit(struct ARexxCmd *ac, struct RexxMsg *msg)
{
    ac->ac_RC = (int32)0;
}
static struct ARexxCmd command_list[] = {{"START", 1, ARexx_Start, "TP=TESTTYPE/A,P=PATH/A", 0, NULL, 0, 0, NULL},
                                         {"STOP", 2, ARexx_Stop, NULL, 0, NULL, 0, 0, NULL},
                                         {"GETRESULTS", 3, ARexx_GetResults, NULL, 0, NULL, 0, 0, NULL},
                                         {"QUIT", 4, ARexx_Quit, NULL, 0, NULL, 0, 0, NULL},
                                         {NULL, 0, NULL, NULL, 0, NULL, 0, 0, NULL}};
*/
static void ShowGlobalReport(void)
{
    GlobalReport report;
    if (GenerateGlobalReport(ui.csv_path, &report)) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "AmigaDiskBench Global Report\n"
                 "Total Benchmarks: %u\n\n"
                 "Sprinter:    Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "HeavyLifter: Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Legacy:      Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "DailyGrind:  Avg %.2f MB/s, Max %.2f MB/s (%u runs)",
                 (unsigned int)report.total_benchmarks, report.stats[TEST_SPRINTER].avg_mbps,
                 report.stats[TEST_SPRINTER].max_mbps, (unsigned int)report.stats[TEST_SPRINTER].total_runs,
                 report.stats[TEST_HEAVY_LIFTER].avg_mbps, report.stats[TEST_HEAVY_LIFTER].max_mbps,
                 (unsigned int)report.stats[TEST_HEAVY_LIFTER].total_runs, report.stats[TEST_LEGACY].avg_mbps,
                 report.stats[TEST_LEGACY].max_mbps, (unsigned int)report.stats[TEST_LEGACY].total_runs,
                 report.stats[TEST_DAILY_GRIND].avg_mbps, report.stats[TEST_DAILY_GRIND].max_mbps,
                 (unsigned int)report.stats[TEST_DAILY_GRIND].total_runs);
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, "AmigaDiskBench Report", msg, "Close"};
        IIntuition->EasyRequest(ui.window, &es, NULL);
    } else {
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, "AmigaDiskBench Error",
                                "No historical data found or CSV error.", "OK"};
        IIntuition->EasyRequest(ui.window, &es, NULL);
    }
}
static void ShowBenchmarkDetails(Object *list_obj)
{
    struct Node *sel = NULL;
    IIntuition->GetAttr(LISTBROWSER_SelectedNode, list_obj, (uint32 *)&sel);
    if (!sel)
        return;

    BenchResult *res = NULL;
    IListBrowser->GetListBrowserNodeAttrs(sel, LBNA_UserData, &res, TAG_DONE);

    if (!res) {
        /* Fallback for nodes that might still have the old string UserData or are NULL */
        char *extra_info = NULL;
        IListBrowser->GetListBrowserNodeAttrs(sel, LBNA_UserData, &extra_info, TAG_DONE);
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, "Benchmark Details",
                                extra_info ? extra_info : "No record data available.", "Close"};
        IIntuition->EasyRequest(ui.window, &es, NULL);
        return;
    }

    OpenDetailsWindow(res);
}
// static Object *CreateHistoryPopupMenu(void)
// {
//     /* [v1.9.9] Standard BOOPSI menu strip using menuclass macros.
//        Note: menuclass is built-in; no explicit class pointer required. */
//     Object *menu = MStrip, MA_AddChild, MTitle(GetString(140, "History Options")), MA_AddChild,
//            MItem(GetString(141, "Show Details...")), MA_ID, MID_SHOW_DETAILS, MEnd, MA_AddChild,
//            MItem(GetString(142, "Delete Preferences...")), MA_ID, MID_DELETE_PREFS, MEnd, MEnd, MEnd;
//     if (!menu) {
//         LOG_DEBUG("/* [v1.9.9] */ CreateHistoryPopupMenu: FAILED to create menu strip");
//     }
//     return menu;
// }
void BenchmarkWorker(void)
{
    struct Process *me = (struct Process *)IExec->FindTask(NULL);
    struct MsgPort *job_port = &me->pr_MsgPort;
    BOOL running = TRUE;
    if (!InitEngine()) {
        LOG_DEBUG("Worker FAILED to initialize engine");
        return;
    }
    LOG_DEBUG("Worker process started successfully");
    while (running) {
        IExec->WaitPort(job_port);
        BenchJob *job = (BenchJob *)IExec->GetMsg(job_port);
        if (job) {
            LOG_DEBUG("Worker: Received Job message...");
            if (job->type == (BenchTestType)-1) {
                running = FALSE;
            } else {
                BenchStatus *status =
                    IExec->AllocVecTags(sizeof(BenchStatus), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
                if (status) {
                    status->msg_type = MSG_TYPE_STATUS;
                    status->finished = FALSE;
                    LOG_DEBUG("Worker: Type=%d, Passes=%u, BS=%u", job->type, (unsigned int)job->num_passes,
                              (unsigned int)job->block_size);
                    status->success = RunBenchmark(job->type, job->target_path, job->num_passes, job->block_size,
                                                   job->use_trimmed_mean, &status->result);
                    status->finished = TRUE;
                    if (status->success) {
                        SaveResultToCSV(ui.csv_path, &status->result);
                    }
                    IExec->PutMsg(job->msg.mn_ReplyPort, &status->msg);
                } else {
                    /* If allocation fails, we must at least reply to the job to prevent deadlock */
                    IExec->ReplyMsg(&job->msg);
                    continue;
                }
            }
            IExec->ReplyMsg(&job->msg);
        }
    }
    LOG_DEBUG("Worker process exiting...");
    CleanupEngine();
}
static void CleanupClasses(void)
{
    LOG_DEBUG("CleanupClasses: Starting cleanup");
    /* [v1.9.9] Close explicitly opened classes */
    if (ui.WindowBase)
        IIntuition->CloseClass(ui.WindowBase);
    if (ui.LayoutBase)
        IIntuition->CloseClass(ui.LayoutBase);
    if (ui.ButtonBase)
        IIntuition->CloseClass(ui.ButtonBase);
    if (ui.ListBrowserBase)
        IIntuition->CloseClass(ui.ListBrowserBase);
    if (ui.ChooserBase)
        IIntuition->CloseClass(ui.ChooserBase);
    if (ui.IntegerBase)
        IIntuition->CloseClass(ui.IntegerBase);
    if (ui.TextEditorBase)
        IIntuition->CloseClass(ui.TextEditorBase);
    IIntuition->CloseClass(ui.ScrollerBase);
    if (ui.CheckBoxBase)
        IIntuition->CloseClass(ui.CheckBoxBase);
    if (ui.ClickTabBase)
        IIntuition->CloseClass(ui.ClickTabBase);
    if (ui.PageBase)
        IIntuition->CloseClass(ui.PageBase);
    if (ui.LabelBase)
        IIntuition->CloseClass(ui.LabelBase);
    if (ui.StringBase)
        IIntuition->CloseClass(ui.StringBase);
    ui.WindowBase = ui.LayoutBase = ui.ButtonBase = ui.ListBrowserBase = ui.ChooserBase = NULL;
    ui.IntegerBase = ui.CheckBoxBase = ui.ClickTabBase = ui.PageBase = ui.LabelBase = ui.StringBase = NULL;
    ui.WindowClass = ui.LayoutClass = ui.ButtonClass = ui.ListBrowserClass = ui.ChooserClass = NULL;
    ui.IntegerClass = ui.CheckBoxClass = ui.ClickTabClass = ui.PageClass = ui.LabelClass = ui.StringClass = NULL;
    LOG_DEBUG("CleanupClasses: Finished");
}
int StartGUI(void)
{
    struct Process *worker_proc = NULL;
    struct DiskObject *icon = NULL;
    memset(&ui, 0, sizeof(ui));
    ui.use_trimmed_mean = DEFAULT_TRIMMED_MEAN;
    ui.default_test_type = DEFAULT_LAST_TEST;
    ui.default_drive[0] = '\0';
    ui.default_block_size_idx = DEFAULT_BLOCK_SIZE_IDX;
    strcpy(ui.csv_path, DEFAULT_CSV_PATH);
    ui.delete_prefs_needed = FALSE;
    /* Open required libraries and interfaces */
    ui.IconBase = IExec->OpenLibrary("icon.library", 0);
    if (ui.IconBase)
        ui.IIcn = (struct IconIFace *)IExec->GetInterface(ui.IconBase, "main", 1, NULL);
    else
        LOG_DEBUG("FAILED to open icon.library");
    ui.LocaleBase = IExec->OpenLibrary("locale.library", 0);
    if (ui.LocaleBase)
        ui.ILoc = (struct LocaleIFace *)IExec->GetInterface(ui.LocaleBase, "main", 1, NULL);
    else
        LOG_DEBUG("FAILED to open locale.library");
    ui.ApplicationBase = IExec->OpenLibrary("application.library", 52);
    if (ui.ApplicationBase) {
        ui.IApp = (struct ApplicationIFace *)IExec->GetInterface(ui.ApplicationBase, "application", 2, NULL);
        ui.IPrefsObjects = (struct PrefsObjectsIFace *)IExec->GetInterface(ui.ApplicationBase, "prefsobjects", 2, NULL);
        if (!ui.IApp)
            LOG_DEBUG("FAILED to get application.library interface v2");
        if (!ui.IPrefsObjects)
            LOG_DEBUG("FAILED to get prefsobjects interface v2");
    } else {
        LOG_DEBUG("FAILED to open application.library v52");
    }
    ui.AslBase = IExec->OpenLibrary("asl.library", 0);
    if (ui.AslBase)
        ui.IAsl = (struct AslIFace *)IExec->GetInterface(ui.AslBase, "main", 1, NULL);
    else
        LOG_DEBUG("FAILED to open asl.library");
    /* [v1.9.9] Open ReAction Classes Explicitly [Modern MINVERSION Pattern] */
    ui.WindowBase = IIntuition->OpenClass("window.class", MINVERSION, &ui.WindowClass);
    if (!ui.WindowClass)
        LOG_DEBUG("FAILED to open window.class");
    ui.LayoutBase = IIntuition->OpenClass("gadgets/layout.gadget", MINVERSION, &ui.LayoutClass);
    if (!ui.LayoutClass)
        LOG_DEBUG("FAILED to open layout.gadget");
    /* [v1.9.4] page.gadget is bundled inside layout.gadget.
       We verify it as a public class if layout.gadget is available. */
    ui.PageAvailable = FALSE;
    if (ui.LayoutClass) {
        Object *dummy = IIntuition->NewObject(NULL, "page.gadget", TAG_DONE);
        if (dummy) {
            IIntuition->DisposeObject(dummy);
            ui.PageAvailable = TRUE;
            LOG_DEBUG("Verified bundled page.gadget is available");
        } else {
            LOG_DEBUG("FAILED to discover bundled page.gadget");
        }
    }
    ui.ButtonBase = IIntuition->OpenClass("gadgets/button.gadget", MINVERSION, &ui.ButtonClass);
    if (!ui.ButtonClass)
        LOG_DEBUG("FAILED to open button.gadget");
    ui.ListBrowserBase = IIntuition->OpenClass("gadgets/listbrowser.gadget", MINVERSION, &ui.ListBrowserClass);
    if (!ui.ListBrowserClass)
        LOG_DEBUG("FAILED to open listbrowser.gadget");
    ui.ChooserBase = IIntuition->OpenClass("gadgets/chooser.gadget", MINVERSION, &ui.ChooserClass);
    if (!ui.ChooserClass)
        LOG_DEBUG("FAILED to open chooser.gadget");
    ui.IntegerBase = IIntuition->OpenClass("gadgets/integer.gadget", MINVERSION, &ui.IntegerClass);
    if (!ui.IntegerClass)
        LOG_DEBUG("FAILED to open integer.gadget");
    ui.TextEditorBase = IIntuition->OpenClass("gadgets/texteditor.gadget", MINVERSION, &ui.TextEditorClass);
    if (!ui.TextEditorClass)
        LOG_DEBUG("FAILED to open texteditor.gadget");

    ui.ScrollerBase = IIntuition->OpenClass("gadgets/scroller.gadget", MINVERSION, &ui.ScrollerClass);
    if (!ui.ScrollerClass)
        LOG_DEBUG("FAILED to open scroller.gadget");
    ui.CheckBoxBase = IIntuition->OpenClass("gadgets/checkbox.gadget", MINVERSION, &ui.CheckBoxClass);
    if (!ui.CheckBoxClass)
        LOG_DEBUG("FAILED to open checkbox.gadget");
    ui.ClickTabBase = IIntuition->OpenClass("gadgets/clicktab.gadget", MINVERSION, &ui.ClickTabClass);
    if (!ui.ClickTabClass)
        LOG_DEBUG("FAILED to open clicktab.gadget");
    ui.LabelBase = IIntuition->OpenClass("images/label.image", MINVERSION, &ui.LabelClass);
    if (!ui.LabelClass)
        LOG_DEBUG("FAILED to open label.image");
    ui.StringBase = IIntuition->OpenClass("gadgets/string.gadget", MINVERSION, &ui.StringClass);
    if (!ui.StringClass)
        LOG_DEBUG("FAILED to open string.gadget");
    /* [v1.9.9] menuclass is built-in; no explicit initializer required. */
    /* Critical Resource Check: Exit if ANY required class or library failed to open
       Note: ClickTab and Page are now optional with a vertical fallback. */
    if (!ui.IIcn || !ui.ILoc || !ui.IApp || !ui.IAsl || !ui.WindowClass || !ui.LayoutClass || !ui.ButtonClass ||
        !ui.ListBrowserClass || !ui.ChooserClass || !ui.IntegerClass || !ui.TextEditorClass || !ui.ScrollerClass ||
        !ui.CheckBoxClass || !ui.LabelClass || !ui.StringClass) {
        LOG_DEBUG("FAILED to open critical GUI resources");
        CleanupClasses();
        if (ui.IAsl)
            IExec->DropInterface((struct Interface *)ui.IAsl);
        if (ui.AslBase)
            IExec->CloseLibrary(ui.AslBase);
        if (ui.IApp)
            IExec->DropInterface((struct Interface *)ui.IApp);
        if (ui.ApplicationBase)
            IExec->CloseLibrary(ui.ApplicationBase);
        if (ui.ILoc)
            IExec->DropInterface((struct Interface *)ui.ILoc);
        if (ui.LocaleBase)
            IExec->CloseLibrary(ui.LocaleBase);
        if (ui.IIcn)
            IExec->DropInterface((struct Interface *)ui.IIcn);
        if (ui.IconBase)
            IExec->CloseLibrary(ui.IconBase);
        return 1;
    }
    if (ui.ILoc)
        ui.catalog = ui.ILoc->OpenCatalog(NULL, "AmigaDiskBench.catalog", OC_BuiltInLanguage, "english", OC_Version, 1,
                                          TAG_DONE);
    /* Register application */
    if (ui.IApp) {
        ui.app_id = ui.IApp->RegisterApplication(APP_TITLE, /* [v1.9.9] Use base title to match existing prefs names */
                                                 REGAPP_URLIdentifier, "diskbench.derfs.co.uk", REGAPP_Description,
                                                 APP_DESCRIPTION, REGAPP_NoIcon, TRUE, REGAPP_HasIconifyFeature, TRUE,
                                                 REGAPP_HasPrefsWindow, TRUE, REGAPP_LoadPrefs,
                                                 TRUE, /* [v1.9.9] Must be TRUE to load on start */
                                                 REGAPP_SavePrefs, TRUE, TAG_DONE);
        if (ui.app_id) {
            LOG_DEBUG("StartGUI: Registered application '%s' with ID %u, LoadPrefs=TRUE, SavePrefs=TRUE", APP_TITLE,
                      ui.app_id);
            ui.IApp->GetApplicationAttrs(ui.app_id, APPATTR_Port, (uint32 *)&ui.app_port, TAG_DONE);
        }
    }
    IExec->NewList(&ui.history_labels);
    IExec->NewList(&ui.bench_labels);
    IExec->NewList(&ui.drive_list);
    IExec->NewList(&ui.test_labels);
    IExec->NewList(&ui.block_list);
    /* v1.8.4: Initialize block size options with consistent mapping */
    const char *blocks[] = {"4K", "16K", "32K", "64K", "128K", "256K", "1M"};
    uint32 block_vals[] = {4096, 16384, 32768, 65536, 131072, 262144, 1048576};
    for (int i = 0; i < 7; i++) {
        struct Node *bn =
            IChooser->AllocChooserNode(CNA_Text, blocks[i], CNA_UserData, (void *)block_vals[i], TAG_DONE);
        if (bn)
            IExec->AddTail(&ui.block_list, bn);
    }
    struct Node *tn1 = IChooser->AllocChooserNode(CNA_Text, GetString(11, "Sprinter (10MB)"), TAG_DONE);
    struct Node *tn2 = IChooser->AllocChooserNode(CNA_Text, GetString(12, "HeavyLifter (100MB)"), TAG_DONE);
    struct Node *tn3 = IChooser->AllocChooserNode(CNA_Text, GetString(13, "Legacy (4MB)"), TAG_DONE);
    struct Node *tn4 = IChooser->AllocChooserNode(CNA_Text, GetString(14, "DailyGrind (Random 4K-64K)"), TAG_DONE);
    if (tn1)
        IExec->AddTail(&ui.test_labels, tn1);
    if (tn2)
        IExec->AddTail(&ui.test_labels, tn2);
    if (tn3)
        IExec->AddTail(&ui.test_labels, tn3);
    if (tn4)
        IExec->AddTail(&ui.test_labels, tn4);
    if (!(ui.gui_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL)))
        return 1;
    if (!(ui.worker_reply_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL))) {
        IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
        return 1;
    }
    if (!(ui.prefs_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL))) {
        IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
        IExec->FreeSysObject(ASOT_PORT, ui.worker_reply_port);
        return 1;
    }
    worker_proc = IDOS->CreateNewProcTags(NP_Entry, (uint32)BenchmarkWorker, NP_Name, (uint32) "AmigaDiskBench_Worker",
                                          NP_Child, TRUE, TAG_DONE);
    if (!worker_proc) {
        IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
        IExec->FreeSysObject(ASOT_PORT, ui.prefs_port);
        return 1;
    }
    ui.worker_port = &worker_proc->pr_MsgPort;
    icon = ui.IIcn->GetDiskObject("PROGDIR:AmigaDiskBench");
    if (!icon)
        icon = ui.IIcn->GetDiskObject("ENV:sys/def_tool");
    struct List tab_list;
    IExec->NewList(&tab_list);
    struct Node *tab_bench = IClickTab->AllocClickTabNode(TNA_Text, GetString(1, "Benchmark"), TNA_Number, 0, TAG_DONE);
    struct Node *tab_history = IClickTab->AllocClickTabNode(TNA_Text, GetString(2, "History"), TNA_Number, 1, TAG_DONE);
    if (tab_bench)
        IExec->AddTail(&tab_list, tab_bench);
    if (tab_history)
        IExec->AddTail(&tab_list, tab_history);
    static struct ColumnInfo bench_cols[] = {{100, "Date", CIF_FIXED | CIF_DRAGGABLE},
                                             {80, "Volume", CIF_FIXED | CIF_DRAGGABLE},
                                             {100, "Test Type", CIF_FIXED | CIF_DRAGGABLE},
                                             {80, "Block Size", CIF_FIXED | CIF_DRAGGABLE},
                                             {100, "No. of Passes", CIF_FIXED | CIF_DRAGGABLE},
                                             {120, "Average (MB/s)", CIF_FIXED | CIF_DRAGGABLE},
                                             {60, "IOPS", CIF_FIXED | CIF_DRAGGABLE},
                                             {80, "Device", CIF_FIXED | CIF_DRAGGABLE},
                                             {40, "Unit", CIF_FIXED | CIF_DRAGGABLE},
                                             {120, "App Version", CIF_FIXED | CIF_DRAGGABLE},
                                             {1, "", CIF_FIXED},
                                             {-1, NULL, 0}};
    (void)bench_cols;
    /* [v1.9.9] Refactored Page 0 (Benchmark) using standard ReAction macros */
    Object *page0 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_Label,
           GetString(3, "Benchmark Control"), LAYOUT_BevelStyle, BVS_GROUP,
           /* Drive Selector */
        LAYOUT_AddChild,
           (ui.target_chooser = ChooserObject, GA_ID, GID_VOL_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Labels,
            (uint32)&ui.drive_list, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(4, "Drive:"), End, CHILD_WeightedHeight, 0,
           /* Test Type Selector */
        LAYOUT_AddChild,
           (ui.test_chooser = ChooserObject, GA_ID, GID_TEST_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Selected, 0,
            CHOOSER_Labels, (uint32)&ui.test_labels, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(5, "Test Type:"), End, CHILD_WeightedHeight, 0,
           /* Passes */
        LAYOUT_AddChild,
           (ui.pass_gad = IntegerObject, GA_ID, GID_NUM_PASSES, GA_RelVerify, TRUE, INTEGER_Minimum, 3, INTEGER_Maximum,
            20, INTEGER_Number, 3, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(6, "Passes:"), End, CHILD_WeightedHeight, 0,
           /* Block Size */
        LAYOUT_AddChild,
           (ui.block_chooser = ChooserObject, GA_ID, GID_BLOCK_SIZE, GA_RelVerify, TRUE, CHOOSER_Selected, 0,
            CHOOSER_Labels, (uint32)&ui.block_list, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(7, "Block Size:"), End, CHILD_WeightedHeight, 0, End,
           CHILD_WeightedHeight, 0, /* End Group "Benchmark Control" */
        /* [v1.9.10] Benchmark Actions Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Benchmark Actions", LAYOUT_BevelStyle, BVS_GROUP,
           LAYOUT_AddChild,
           (ui.run_button = ButtonObject, GA_ID, GID_RUN_ALL, GA_RelVerify, TRUE, GA_Text,
            GetString(8, "Run Benchmark"), End),
           CHILD_WeightedWidth, 0, CHILD_WeightedHeight, 0, CHILD_MinWidth, 160, CHILD_MinHeight, 40,
           /* Status Light - Commented out for now
           LAYOUT_AddChild, ButtonObject, GA_ID, GID_STATUS_LIGHT, GA_ReadOnly, TRUE, BUTTON_BevelStyle, BVS_NONE,
           BUTTON_Transparent, TRUE, GA_LabelImage,
           (ui.status_light_obj = LabelObject, LABEL_Text, "[ IDLE ]", LABEL_Justification, LJ_CENTER, End), End,
           */
        End, CHILD_WeightedHeight, 0, /* End Group "Benchmark Actions" */
        /* Benchmark List */
        LAYOUT_AddChild,
           (ui.bench_list = ListBrowserObject, GA_ID, GID_CURRENT_RESULTS, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
            (uint32)bench_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.bench_labels,
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
           CHILD_WeightedHeight, 100, End;
    /* [v1.9.9] Refactored Page 1 (History) using standard ReAction macros */
    Object *page1 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild,
           (ui.history_list = ListBrowserObject, GA_ID, GID_HISTORY_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
            (uint32)bench_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.history_labels,
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE,
            /* [v1.9.11] Synchronized with Benchmark tab for now.
            GA_ContextMenu, (uint32)(ui.history_popup = CreateHistoryPopupMenu()), */
            End),
           CHILD_WeightedHeight, 100, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_REFRESH_HISTORY, GA_Text, GetString(9, "Refresh History"), End, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_VIEW_REPORT, GA_Text, GetString(10, "Global Report"), End, End, CHILD_WeightedHeight, 0, End;
    static struct NewMenu menu_data[] = {
        {NM_TITLE, (STRPTR) "Project", NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "About...", (STRPTR) "A", 0, 0, (APTR)MID_ABOUT},
        {NM_ITEM, (STRPTR) "Preferences...", (STRPTR) "P", 0, 0, (APTR)MID_PREFS},
        {NM_ITEM, (STRPTR) "Delete Preferences...", NULL, 0, 0, (APTR)MID_DELETE_PREFS},
        {NM_ITEM, (STRPTR)NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "Quit", (STRPTR) "Q", 0, 0, (APTR)MID_QUIT},
        {NM_END, NULL, NULL, 0, 0, NULL}};
    Object *main_content = NULL;
    /* [v1.9.9] Main GUI construction using macros */
    if (ui.PageAvailable && page0 && page1) {
        ui.page_obj =
            IIntuition->NewObject(NULL, "page.gadget", PAGE_Add, (uint32)page0, PAGE_Add, (uint32)page1, TAG_DONE);
        ui.tabs = ClickTabObject, GA_ID, GID_TABS, GA_RelVerify, TRUE, CLICKTAB_Labels, (uint32)&tab_list,
        CLICKTAB_PageGroup, (uint32)ui.page_obj, End;
        main_content = ui.tabs;
    } else {
        LOG_DEBUG("/* [v1.9.9] */ Using vertical fallback layout (components missing)");
        main_content = VLayoutObject, LAYOUT_AddChild, page0, LAYOUT_AddChild, page1, End;
        ui.tabs = NULL;
        ui.page_obj = NULL;
    }
    ui.win_obj = WindowObject, WA_Title, APP_VER_TITLE, WA_SizeGadget, TRUE, WA_CloseGadget, TRUE, WA_DepthGadget, TRUE,
    WA_DragBar, TRUE, WA_Activate, TRUE, WA_InnerWidth, 600, WA_InnerHeight, 500, WINDOW_Application, (uint32)ui.app_id,
    WINDOW_SharedPort, (uint32)ui.gui_port, WINDOW_IconifyGadget, TRUE, WINDOW_Iconifiable, TRUE, WINDOW_Icon,
    (uint32)icon, WINDOW_IconNoDispose, TRUE, WINDOW_NewMenu, (uint32)menu_data, WINDOW_ParentGroup, VLayoutObject,
    LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, (uint32)main_content, CHILD_WeightedHeight, 100, End, End;
    if (!ui.win_obj) {
        IClickTab->FreeClickTabList(&tab_list);
        BenchJob qj;
        memset(&qj, 0, sizeof(qj));
        qj.type = (BenchTestType)-1;
        qj.msg.mn_ReplyPort = ui.worker_reply_port;
        IExec->PutMsg(&worker_proc->pr_MsgPort, &qj.msg);
        IExec->WaitPort(ui.worker_reply_port);
        IExec->GetMsg(ui.worker_reply_port);
        IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
        return 1;
    }
    if ((ui.window = (struct Window *)IIntuition->IDoMethod(ui.win_obj, WM_OPEN))) {
        char pv[256];
        if (IDOS->NameFromLock(IDOS->GetProgramDir(), pv, sizeof(pv))) {
            char *c = strchr(pv, ':');
            if (c)
                *(c + 1) = '\0';
        } else
            strcpy(pv, "SYS:");
        RefreshDriveList();
        LoadPrefs();
        RefreshHistory();
        RefreshHistory();
        /* Select Default Drive */
        struct Node *sel_node = NULL;
        struct Node *vn = NULL;
        CONST_STRPTR target_drive = (ui.default_drive[0] != '\0') ? ui.default_drive : pv;
        /* If default_drive is empty, we fallback to 'pv' (Program Volume).
           If 'pv' (or default_drive) is found, we select it.
           If NOT found, we select index 0 (Top Item). */
        for (vn = IExec->GetHead(&ui.drive_list); vn; vn = IExec->GetSucc(vn)) {
            DriveNodeData *dd = NULL;
            IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
            if (dd && dd->bare_name && strcasecmp(dd->bare_name, target_drive) == 0) {
                sel_node = vn;
                break;
            }
        }
        /* If exact match failed and we were trying default_drive, maybe try PV?
           User request: "if prefs not set... default to top item".
           But for existing behavior preservation, defaulting to Program Dir is nice if no pref.
           Let's stick to: Pref -> ProgramDir -> Top. */
        if (!sel_node && ui.default_drive[0] != '\0') {
            /* Pref failed, try pv */
            for (vn = IExec->GetHead(&ui.drive_list); vn; vn = IExec->GetSucc(vn)) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name && strcasecmp(dd->bare_name, pv) == 0) {
                    sel_node = vn;
                    break;
                }
            }
        }
        if (sel_node) {
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL, CHOOSER_SelectedNode,
                                       sel_node, TAG_DONE);
        } else {
            /* Fallback to top item (Index 0) */
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL, CHOOSER_Selected, 0,
                                       TAG_DONE);
        }
        uint32 win_sig = 0;
        IIntuition->GetAttr(WINDOW_SigMask, ui.win_obj, &win_sig);
        uint32 gui_sig = (1L << ui.gui_port->mp_SigBit);
        uint32 worker_sig = (1L << ui.worker_reply_port->mp_SigBit);
        uint32 app_sig = ui.app_port ? (1L << ui.app_port->mp_SigBit) : 0;
        uint32 pref_sig = (1L << ui.prefs_port->mp_SigBit);
        uint32 wait_mask = win_sig | gui_sig | worker_sig | app_sig | pref_sig | SIGBREAKF_CTRL_C;
        BOOL running = TRUE;
        while (running) {
            uint32 sig =
                IExec->Wait(wait_mask | (ui.details_win_obj ? (1L << ui.details_window->UserPort->mp_SigBit) : 0));
            if (sig & SIGBREAKF_CTRL_C)
                running = FALSE;
            if (sig & worker_sig) {
                struct Message *m;
                while ((m = IExec->GetMsg(ui.worker_reply_port))) {
                    LOG_DEBUG("GUI: Worker Msg received at %p. Type=%d", m, m->mn_Node.ln_Type);
                    if (m->mn_Node.ln_Type == NT_REPLYMSG || m->mn_Node.ln_Type == NT_MESSAGE) {
                        BenchStatus *st = (BenchStatus *)m;
                        if (st->msg_type == MSG_TYPE_STATUS) {
                            if (st->finished) {
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL,
                                                           LABEL_Text, (uint32) "[ IDLE ]", TAG_DONE);
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL,
                                                           GA_Disabled, FALSE, TAG_DONE);
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.test_chooser, ui.window, NULL,
                                                           GA_Disabled, FALSE, TAG_DONE);
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.pass_gad, ui.window, NULL, GA_Disabled,
                                                           FALSE, TAG_DONE);
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.block_chooser, ui.window, NULL,
                                                           GA_Disabled, FALSE, TAG_DONE);
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.run_button, ui.window, NULL, GA_Disabled,
                                                           FALSE, GA_Text, (uint32)GetString(8, "Run Benchmark"),
                                                           TAG_DONE);
                                /* Force visual refresh for ReAction layout to reflect enabled state */
                                IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
                                if (st->success) {
                                    char tn[64], ms[32], is[32], ut[32];
                                    snprintf(ms, sizeof(ms), "%.2f", st->result.mb_per_sec);
                                    snprintf(is, sizeof(is), "%u", (unsigned int)st->result.iops);
                                    snprintf(ut, sizeof(ut), "%u", (unsigned int)st->result.device_unit);
                                    switch (st->result.type) {
                                    case TEST_SPRINTER:
                                        strcpy(tn, "Sprinter");
                                        break;
                                    case TEST_HEAVY_LIFTER:
                                        strcpy(tn, "HeavyLifter");
                                        break;
                                    case TEST_LEGACY:
                                        strcpy(tn, "Legacy");
                                        break;
                                    case TEST_DAILY_GRIND:
                                        strcpy(tn, "DailyGrind");
                                        break;
                                    default:
                                        strcpy(tn, "Unknown");
                                    }
                                    /* Prepare BenchResult for UserData */
                                    BenchResult *res = IExec->AllocVecTags(sizeof(BenchResult), AVT_Type, MEMF_SHARED,
                                                                           AVT_ClearWithValue, 0, TAG_DONE);
                                    if (res) {
                                        memcpy(res, &st->result, sizeof(BenchResult));
                                    }

                                    char aps[16], abs[32];
                                    snprintf(aps, sizeof(aps), "%u", (unsigned int)st->result.passes);
                                    strcpy(abs, FormatPresetBlockSize(st->result.block_size));
                                    struct Node *n = IListBrowser->AllocListBrowserNode(
                                        11, LBNA_Column, COL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text,
                                        (uint32)st->result.timestamp, LBNA_Column, COL_VOL, LBNCA_CopyText, TRUE,
                                        LBNCA_Text, (uint32)st->result.volume_name, LBNA_Column, COL_TEST,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)tn, LBNA_Column, COL_BS,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)abs, LBNA_Column, COL_PASSES,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)aps, LBNA_Column, COL_MBPS,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ms, LBNA_Column, COL_IOPS,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)is, LBNA_Column, COL_DEVICE,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.device_name, LBNA_Column,
                                        COL_UNIT, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ut, LBNA_Column, COL_VER,
                                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.app_version, LBNA_Column,
                                        COL_DUMMY, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32) "", LBNA_UserData,
                                        (uint32)res, TAG_DONE);
                                    if (n) {
                                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL,
                                                                   LISTBROWSER_Labels, (ULONG)-1, TAG_DONE);
                                        IExec->AddTail(&ui.bench_labels, n);
                                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL,
                                                                   LISTBROWSER_Labels, (uint32)&ui.bench_labels,
                                                                   LISTBROWSER_AutoFit, TRUE, TAG_DONE);
                                        IIntuition->RefreshGList((struct Gadget *)ui.bench_list, ui.window, NULL, 1);
                                    }
                                    /* [v1.9.4] Automatically refresh History tab since job data was saved
                                       [v1.9.7 Revised P2] Moved to Status block (above) to avoid redundancy */
                                    /* RefreshHistory(); */
                                }
                            }
                            IExec->FreeVec(st);
                        } else {
                            /* Free replied Job messages */
                            IExec->FreeVec(m);
                        }
                    }
                }
            }
            /* Drain Application Library Messages */
            if (ui.app_port && (sig & app_sig)) {
                struct ApplicationMsg *amsg;
                while ((amsg = (struct ApplicationMsg *)IExec->GetMsg(ui.app_port))) {
                    switch (amsg->type) {
                    case APPLIBMT_Quit:
                        running = FALSE;
                        break;
                    case APPLIBMT_Hide:
                        if (IIntuition->IDoMethod(ui.win_obj, WM_ICONIFY))
                            ui.window = NULL;
                        break;
                    case APPLIBMT_Unhide:
                        ui.window = (struct Window *)IIntuition->IDoMethod(ui.win_obj, WM_OPEN);
                        break;
                    case APPLIBMT_OpenPrefs:
                        OpenPrefsWindow();
                        break;
                    }
                    IExec->ReplyMsg((struct Message *)amsg);
                }
            }
            /* Drain Main Window Input Queue */
            if (sig & win_sig) {
                uint16 code;
                uint32 result;
                while ((result = IIntuition->IDoMethod(ui.win_obj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
                    uint32 gid = result & WMHI_GADGETMASK;
                    switch (result & WMHI_CLASSMASK) {
                    case WMHI_CLOSEWINDOW:
                        running = FALSE;
                        break;
                    case WMHI_GADGETUP:
                        switch (gid) {
                        case GID_TABS: {
                            uint32 t = 0;
                            IIntuition->GetAttr(CLICKTAB_Current, ui.tabs, &t);
                            IIntuition->SetGadgetAttrs((struct Gadget *)ui.page_obj, ui.window, NULL, PAGE_Current, t,
                                                       TAG_DONE);
                            break;
                        }
                        case GID_RUN_ALL: {
                            char path[256];
                            uint32 passes = 3, drive_idx = 0;
                            uint32 test_type_idx = TEST_SPRINTER;
                            struct Node *node = NULL;
                            if (ui.target_chooser) {
                                IIntuition->GetAttr(CHOOSER_Selected, ui.target_chooser, &drive_idx);
                                IIntuition->GetAttr(CHOOSER_SelectedNode, ui.target_chooser, (uint32 *)&node);
                            }
                            if (ui.test_chooser) {
                                IIntuition->GetAttr(CHOOSER_Selected, ui.test_chooser, &test_type_idx);
                            }
                            if (node) {
                                DriveNodeData *ddata = NULL;
                                IChooser->GetChooserNodeAttrs(node, CNA_UserData, &ddata, TAG_DONE);
                                if (ddata && ddata->bare_name) {
                                    strcpy(path, ddata->bare_name);
                                } else {
                                    strcpy(path, "RAM:");
                                }
                            } else {
                                strcpy(path, "RAM:");
                            }
                            if (ui.pass_gad) {
                                IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &passes);
                            }
                            uint32 block_val = 4096;
                            if (ui.block_chooser) {
                                struct Node *bnode = NULL;
                                IIntuition->GetAttr(CHOOSER_SelectedNode, ui.block_chooser, (uint32 *)&bnode);
                                if (bnode) {
                                    void *ud = NULL;
                                    IChooser->GetChooserNodeAttrs(bnode, CNA_UserData, &ud, TAG_DONE);
                                    if (ud)
                                        block_val = (uint32)ud;
                                }
                            }
                            LOG_DEBUG("LaunchJob: path='%s', test=%u, passes=%u, block_val=%u, trimmed=%d", path,
                                      (unsigned int)test_type_idx, passes, block_val, (int)ui.use_trimmed_mean);
                            BenchJob *job = IExec->AllocVecTags(sizeof(BenchJob), AVT_Type, MEMF_SHARED,
                                                                AVT_ClearWithValue, 0, TAG_DONE);
                            if (job) {
                                job->msg_type = MSG_TYPE_JOB;
                                job->type = (BenchTestType)test_type_idx;
                                strcpy(job->target_path, path);
                                job->num_passes = passes;
                                job->block_size = block_val;
                                job->use_trimmed_mean = ui.use_trimmed_mean;
                                job->msg.mn_ReplyPort = ui.worker_reply_port;
                                IIntuition->SetGadgetAttrs((struct Gadget *)ui.run_button, ui.window, NULL, GA_Disabled,
                                                           TRUE, TAG_DONE);
                                IExec->PutMsg(ui.worker_port, &job->msg);
                            }
                            break;
                        }
                        case GID_REFRESH_HISTORY:
                            RefreshHistory();
                            break;
                        case GID_HISTORY_LIST:
                        case GID_CURRENT_RESULTS: {
                            uint32 re = 0;
                            Object *lb = (gid == GID_HISTORY_LIST) ? ui.history_list : ui.bench_list;
                            IIntuition->GetAttr(LISTBROWSER_RelEvent, lb, &re);
                            if (re & LBRE_DOUBLECLICK) {
                                ShowBenchmarkDetails(lb);
                            }
                            break;
                        }
                        case GID_VIEW_REPORT:
                            ShowGlobalReport();
                            break;
                        }
                        break;
                    case WMHI_MENUPICK: {
                        uint32 mid = result & WMHI_MENUMASK;
                        while (mid != MENUNULL) {
                            struct MenuItem *mi = IIntuition->ItemAddress(ui.window->MenuStrip, mid);
                            if (mi) {
                                uint32 mdata = (uint32)GTMENUITEM_USERDATA(mi);
                                switch (mdata) {
                                case MID_QUIT:
                                    running = FALSE;
                                    break;
                                case MID_ABOUT: {
                                    struct EasyStruct es = {sizeof(struct EasyStruct), 0, "About AmigaDiskBench",
                                                            APP_ABOUT_MSG, "OK"};
                                    IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE);
                                    break;
                                }
                                case MID_PREFS:
                                    OpenPrefsWindow();
                                    break;
                                case MID_DELETE_PREFS: {
                                    struct EasyStruct es = {
                                        sizeof(struct EasyStruct), 0, "Confirm Preference Deletion",
                                        "This will delete your preferences file\nand exit the application.\n\n"
                                        "Continue?",
                                        "OK|Cancel"};
                                    if (IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE) == 1) {
                                        ui.delete_prefs_needed = TRUE;
                                        running = FALSE;
                                    }
                                    break;
                                }
                                case MID_SHOW_DETAILS:
                                    ShowBenchmarkDetails(ui.history_list);
                                    break;
                                }
                            }
                            mid = mi->NextSelect;
                        }
                        break;
                    }
                    }
                }
            }
            /* Drain Preferences Window Input Queue */
            if (ui.prefs_win_obj && (sig & pref_sig)) {
                uint16 pcode;
                uint32 presult;
                while (ui.prefs_win_obj &&
                       (presult = IIntuition->IDoMethod(ui.prefs_win_obj, WM_HANDLEINPUT, &pcode)) != WMHI_LASTMSG) {
                    uint32 gid = presult & WMHI_GADGETMASK;
                    switch (presult & WMHI_CLASSMASK) {
                    case WMHI_CLOSEWINDOW:
                        IIntuition->IDoMethod(ui.prefs_win_obj, WM_CLOSE);
                        IIntuition->DisposeObject(ui.prefs_win_obj);
                        ui.prefs_win_obj = NULL;
                        ui.prefs_window = NULL;
                        break;
                    case WMHI_GADGETUP:
                        if (gid == GID_PREFS_SAVE) {
                            uint32 p_num = 3, t_mean = 0;
                            char *c_path = NULL;
                            IIntuition->GetAttr(INTEGER_Number, ui.prefs_pass_gad, &p_num);
                            IIntuition->GetAttr(GA_Selected, ui.prefs_trimmed_check, &t_mean);
                            IIntuition->GetAttr(STRINGA_TextVal, ui.prefs_csv_path, (uint32 *)&c_path);
                            /* Update active state */
                            IIntuition->SetGadgetAttrs((struct Gadget *)ui.pass_gad, ui.window, NULL, INTEGER_Number,
                                                       p_num, TAG_DONE);
                            ui.use_trimmed_mean = (BOOL)t_mean;
                            uint32 t_type = DEFAULT_LAST_TEST;
                            IIntuition->GetAttr(CHOOSER_Selected, ui.prefs_test_chooser, &t_type);
                            ui.default_test_type = t_type;
                            uint32 b_size_idx = DEFAULT_BLOCK_SIZE_IDX;
                            IIntuition->GetAttr(CHOOSER_Selected, ui.prefs_block_chooser, &b_size_idx);
                            ui.default_block_size_idx = b_size_idx;
                            /* Update Default Drive from Prefs Chooser */
                            struct Node *d_node = NULL;
                            IIntuition->GetAttr(CHOOSER_SelectedNode, ui.prefs_target_chooser, (uint32 *)&d_node);
                            if (d_node) {
                                DriveNodeData *dd = NULL;
                                IChooser->GetChooserNodeAttrs(d_node, CNA_UserData, &dd, TAG_DONE);
                                if (dd && dd->bare_name) {
                                    strncpy(ui.default_drive, dd->bare_name, sizeof(ui.default_drive) - 1);
                                    ui.default_drive[sizeof(ui.default_drive) - 1] = '\0';
                                }
                            }
                            /* Optional: Update main window test chooser immediately?
                               User said "default on the benchmark tab". Changing the default typically
                               doesn't change the *current* selection unless we want it to.
                               Let's leave the current selection alone to be less intrusive,
                               unless we are in a "fresh" state?
                               Actually, let's NOT update the current chooser, as "Default" applies to initialization.
                             */
                            BOOL path_changed = FALSE;
                            if (c_path && strcmp(ui.csv_path, c_path) != 0) {
                                strncpy(ui.csv_path, c_path, sizeof(ui.csv_path) - 1);
                                ui.csv_path[sizeof(ui.csv_path) - 1] = '\0';
                                path_changed = TRUE;
                            }
                            struct PrefsObjectsIFace *IPrefs = ui.IPrefsObjects;
                            if (IPrefs) {
                                PrefsObject *dict = NULL;
                                ui.IApp->GetApplicationAttrs(ui.app_id, APPATTR_MainPrefsDict, (uint32 *)&dict,
                                                             TAG_DONE);
                                if (dict) {
                                    LOG_DEBUG("SavePrefs: MainPrefsDict ptr=%p", dict);
                                    /* Debug: Check if dict is actually a dictionary (Type 6 usually) */
                                    /* uint32 dict_type = 0;
                                    IPrefs->PrefsBaseObject(dict, NULL, ALPO_Identify, &dict_type, TAG_DONE);
                                    LOG_DEBUG("SavePrefs: MainPrefsDict Type = %d (Expected 6=Dict)",
                                    (int)dict_type); */
                                    /*
                                     * Create new Preference Objects.
                                     * These will be transferred to the dictionary.
                                     */
                                    PrefsObject *p_num_obj =
                                        IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, p_num, TAG_DONE);
                                    PrefsObject *t_mean_obj =
                                        IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetBool, t_mean, TAG_DONE);
                                    PrefsObject *t_type_obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong,
                                                                                  ui.default_test_type, TAG_DONE);
                                    PrefsObject *b_size_obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong,
                                                                                  ui.default_block_size_idx, TAG_DONE);
                                    PrefsObject *d_drive_obj = IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString,
                                                                                   ui.default_drive, TAG_DONE);
                                    PrefsObject *c_path_obj =
                                        IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString, ui.csv_path, TAG_DONE);
                                    /* LOG_DEBUG("SavePrefs: Objs created: num=%p, mean=%p, path=%p", p_num_obj,
                                              t_mean_obj, c_path_obj); */
                                    if (p_num_obj)
                                        IPrefs->DictSetObjectForKey(dict, p_num_obj, "DefaultPasses");
                                    if (t_mean_obj)
                                        IPrefs->DictSetObjectForKey(dict, t_mean_obj, "TrimmedMean");
                                    if (t_type_obj)
                                        IPrefs->DictSetObjectForKey(dict, t_type_obj, "DefaultTestType");
                                    if (b_size_obj)
                                        IPrefs->DictSetObjectForKey(dict, b_size_obj, "DefaultBS");
                                    if (d_drive_obj)
                                        IPrefs->DictSetObjectForKey(dict, d_drive_obj, "DefaultDrive");
                                    struct ALPOObjKey key_struct;
                                    key_struct.obj = c_path_obj;
                                    key_struct.key = "CSVPath";
                                    uint32 err = 0;
                                    /*
                                     * Update "CSVPath":
                                     * 1. Remove existing object (if any) to ensure clean state.
                                     * 2. Set new object.
                                     * Note: We use the low-level PrefsDictionary method here for better error
                                     * control.
                                     */
                                    /* Use robust Remove/Set pattern (implicit Self per compiler/SDK convention) */
                                    IPrefs->PrefsDictionary(dict, NULL, ALPODICT_RemoveObjForKey, "CSVPath", TAG_DONE);
                                    /* Insert new value */
                                    err = 0;
                                    IPrefs->PrefsDictionary(dict, &err, ALPODICT_SetObjForKey, &key_struct, TAG_DONE);
                                    if (err != 0) {
                                        LOG_DEBUG("SavePrefs: DictSetObjectForKey(CSVPath) FAILED (0x%08X)", err);
                                    }
                                    /*
                                     * Commit changes to disk (ENVARC).
                                     * APPATTR_SavePrefs=TRUE ensures it's saved to XML upon unregistration or
                                     * flush. APPATTR_FlushPrefs=TRUE triggers immediate write.
                                     */
                                    /* Flush to ENV (SetApplicationAttrs handles this via FlushPrefs) */
                                    uint32 res = ui.IApp->SetApplicationAttrs(ui.app_id, APPATTR_SavePrefs, TRUE,
                                                                              APPATTR_FlushPrefs, TRUE, TAG_DONE);
                                    LOG_DEBUG("SavePrefs: SetApplicationAttrs(APPATTR_SavePrefs/Flush) returned %u",
                                              res);
                                    /* Immediate verification removed for release */
                                    /*
                                     * [Ownership Note]
                                     * DictSetObjectForKey (and ALPODICT_SetObjForKey) TRANSFER ownership to the
                                     * dictionary. We must NOT release p_num_obj, t_mean_obj, or c_path_obj here if
                                     * they were successfully added. The dictionary (and application.library) now
                                     * owns them.
                                     */
                                } else {
                                    LOG_DEBUG("SavePrefs: FAILED - Dict is NULL");
                                }
                            }
                            LOG_DEBUG("SavePrefs: Path changed = %d", (int)path_changed);
                            if (path_changed) {
                                RefreshHistory();
                            }
                            IIntuition->IDoMethod(ui.prefs_win_obj, WM_CLOSE);
                            IIntuition->DisposeObject(ui.prefs_win_obj);
                            ui.prefs_win_obj = NULL;
                            ui.prefs_window = NULL;
                        } else if (gid == GID_PREFS_CANCEL) {
                            IIntuition->IDoMethod(ui.prefs_win_obj, WM_CLOSE);
                            IIntuition->DisposeObject(ui.prefs_win_obj);
                            ui.prefs_win_obj = NULL;
                            ui.prefs_window = NULL;
                        } else if (gid == GID_PREFS_CSV_BR) {
                            BrowseCSV();
                        }
                        break;
                    }
                }
            }
            /* Drain Details Window Input Queue */
            if (ui.details_win_obj && (sig & (1L << ui.details_window->UserPort->mp_SigBit))) {
                uint16 dcode;
                uint32 dresult;
                while (ui.details_win_obj &&
                       (dresult = IIntuition->IDoMethod(ui.details_win_obj, WM_HANDLEINPUT, &dcode)) != WMHI_LASTMSG) {
                    HandleDetailsWindowEvent(dcode, dresult);
                }
            }
        }
        IClickTab->FreeClickTabList(&tab_list);
        /* Signal worker to quit and wait for it */
        BenchJob qj;
        memset(&qj, 0, sizeof(qj));
        qj.type = (BenchTestType)-1;
        qj.msg.mn_ReplyPort = ui.worker_reply_port;
        IExec->PutMsg(&worker_proc->pr_MsgPort, &qj.msg);
        IExec->WaitPort(ui.worker_reply_port);
        IExec->GetMsg(ui.worker_reply_port);
        /* [v1.8.3 fixes] Consolidated and Safe Final Cleanup */
        if (ui.win_obj) {
            LOG_DEBUG("StartGUI: Shutting down GUI...");
            if (ui.window) {
                IIntuition->IDoMethod(ui.win_obj, WM_CLOSE);
                ui.window = NULL;
            }
            LOG_DEBUG("StartGUI: Disposing window object %p", ui.win_obj);
            IIntuition->DisposeObject(ui.win_obj);
            ui.win_obj = NULL;
            if (ui.gui_port)
                IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
            if (ui.worker_reply_port)
                IExec->FreeSysObject(ASOT_PORT, ui.worker_reply_port);
            if (ui.prefs_port)
                IExec->FreeSysObject(ASOT_PORT, ui.prefs_port);
            if (ui.popup_port)
                IExec->FreeSysObject(ASOT_PORT, ui.popup_port);
            if (ui.catalog && ui.ILoc)
                ui.ILoc->CloseCatalog(ui.catalog);
            /* Now safely free the node data */
            struct Node *n, *nx;
            n = IExec->GetHead(&ui.history_labels);
            while (n) {
                nx = IExec->GetSucc(n);
                void *udata = NULL;
                IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &udata, TAG_DONE);
                if (udata)
                    IExec->FreeVec(udata);
                IListBrowser->FreeListBrowserNode(n);
                n = nx;
            }
            n = IExec->GetHead(&ui.bench_labels);
            while (n) {
                nx = IExec->GetSucc(n);
                void *udata = NULL;
                IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &udata, TAG_DONE);
                if (udata)
                    IExec->FreeVec(udata);
                IListBrowser->FreeListBrowserNode(n);
                n = nx;
            }
            n = IExec->GetHead(&ui.test_labels);
            while (n) {
                nx = IExec->GetSucc(n);
                IChooser->FreeChooserNode(n);
                n = nx;
            }
            n = IExec->GetHead(&ui.block_list);
            while (n) {
                nx = IExec->GetSucc(n);
                IChooser->FreeChooserNode(n);
                n = nx;
            }
            n = IExec->GetHead(&ui.drive_list);
            while (n) {
                nx = IExec->GetSucc(n);
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(n, CNA_UserData, &dd, TAG_DONE);
                if (dd) {
                    if (dd->bare_name)
                        IExec->FreeVec(dd->bare_name);
                    if (dd->display_name)
                        IExec->FreeVec(dd->display_name);
                    IExec->FreeVec(dd);
                }
                IChooser->FreeChooserNode(n);
                n = nx;
            }
            if (ui.app_id && ui.IApp) {
                if (ui.delete_prefs_needed) {
                    LOG_DEBUG("StartGUI: Preference deletion requested. Disabling SavePrefs and deleting file.");
                    ui.IApp->SetApplicationAttrs(ui.app_id, APPATTR_SavePrefs, FALSE, TAG_DONE);
                    char prefs_path[256];
                    snprintf(prefs_path, sizeof(prefs_path), "ENVARC:%s.diskbench.derfs.co.uk.xml", APP_TITLE);
                    if (IDOS->Delete(prefs_path)) {
                        LOG_DEBUG("StartGUI: Deleted %s", prefs_path);
                    } else {
                        LOG_DEBUG("StartGUI: Failed to delete %s", prefs_path);
                    }
                    /* Delete ENV: copy as well if it exists */
                    snprintf(prefs_path, sizeof(prefs_path), "ENV:%s.diskbench.derfs.co.uk.xml", APP_TITLE);
                    IDOS->Delete(prefs_path);
                }
                LOG_DEBUG("StartGUI: Unregistering application ID %u", ui.app_id);
                ui.IApp->UnregisterApplication(ui.app_id, TAG_DONE);
            }
            if (ui.IApp) {
                LOG_DEBUG("StartGUI: Dropping IApp interface");
                IExec->DropInterface((struct Interface *)ui.IApp);
            }
            if (ui.IPrefsObjects) {
                LOG_DEBUG("StartGUI: Dropping IPrefsObjects interface");
                IExec->DropInterface((struct Interface *)ui.IPrefsObjects);
            }
            if (ui.ApplicationBase)
                IExec->CloseLibrary(ui.ApplicationBase);
            if (ui.ILoc)
                IExec->DropInterface((struct Interface *)ui.ILoc);
            if (ui.LocaleBase)
                IExec->CloseLibrary(ui.LocaleBase);
            if (ui.IIcn) {
                if (icon)
                    ui.IIcn->FreeDiskObject(icon);
                IExec->DropInterface((struct Interface *)ui.IIcn);
            }
            if (ui.IconBase)
                IExec->CloseLibrary(ui.IconBase);
            if (ui.IAsl)
                IExec->DropInterface((struct Interface *)ui.IAsl);
            if (ui.AslBase)
                IExec->CloseLibrary(ui.AslBase);
            CleanupClasses();
            return 0;
        }
        LOG_DEBUG("StartGUI: Window failed to open");
        CleanupClasses();
        return 1;
    }
    LOG_DEBUG("StartGUI: win_obj was NULL");
    CleanupClasses();
    return 1;
}
