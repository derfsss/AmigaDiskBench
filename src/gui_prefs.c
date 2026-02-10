#include "gui_internal.h"
#include <string.h>

void LoadPrefs(void)
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

void BrowseCSV(void)
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

void OpenPrefsWindow(void)
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

void UpdatePreferences(void)
{
    if (!ui.prefs_win_obj)
        return;

    uint32 p_num = 3, t_mean = 0;
    char *c_path = NULL;

    IIntuition->GetAttr(INTEGER_Number, ui.prefs_pass_gad, &p_num);
    IIntuition->GetAttr(GA_Selected, ui.prefs_trimmed_check, &t_mean);
    IIntuition->GetAttr(STRINGA_TextVal, ui.prefs_csv_path, (uint32 *)&c_path);

    /* Update active state in main window */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.pass_gad, ui.window, NULL, INTEGER_Number, p_num, TAG_DONE);
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

    BOOL path_changed = FALSE;
    if (c_path && strcmp(ui.csv_path, c_path) != 0) {
        strncpy(ui.csv_path, c_path, sizeof(ui.csv_path) - 1);
        ui.csv_path[sizeof(ui.csv_path) - 1] = '\0';
        path_changed = TRUE;
    }

    struct PrefsObjectsIFace *IPrefs = ui.IPrefsObjects;
    if (IPrefs) {
        PrefsObject *dict = NULL;
        ui.IApp->GetApplicationAttrs(ui.app_id, APPATTR_MainPrefsDict, (uint32 *)&dict, TAG_DONE);
        if (dict) {
            LOG_DEBUG("UpdatePreferences: Found MainPrefsDict at %p", dict);
            PrefsObject *p_num_obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, p_num, TAG_DONE);
            PrefsObject *t_mean_obj = IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetBool, t_mean, TAG_DONE);
            PrefsObject *t_type_obj =
                IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, ui.default_test_type, TAG_DONE);
            PrefsObject *b_size_obj =
                IPrefs->PrefsNumber(NULL, NULL, ALPONUM_AllocSetLong, ui.default_block_size_idx, TAG_DONE);
            PrefsObject *d_drive_obj =
                IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString, ui.default_drive, TAG_DONE);
            PrefsObject *c_path_obj = IPrefs->PrefsString(NULL, NULL, ALPOSTR_AllocSetString, ui.csv_path, TAG_DONE);

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

            if (c_path_obj) {
                struct ALPOObjKey key_struct;
                key_struct.obj = c_path_obj;
                key_struct.key = "CSVPath";
                uint32 err = 0;
                IPrefs->PrefsDictionary(dict, NULL, ALPODICT_RemoveObjForKey, "CSVPath", TAG_DONE);
                IPrefs->PrefsDictionary(dict, &err, ALPODICT_SetObjForKey, &key_struct, TAG_DONE);
                if (err != 0) {
                    LOG_DEBUG("UpdatePreferences: Set CSVPath FAILED (0x%08X)", err);
                }
            }

            ui.IApp->SetApplicationAttrs(ui.app_id, APPATTR_SavePrefs, TRUE, APPATTR_FlushPrefs, TRUE, TAG_DONE);
        }
    }

    if (path_changed) {
        RefreshHistory();
    }

    IIntuition->IDoMethod(ui.prefs_win_obj, WM_CLOSE);
    IIntuition->DisposeObject(ui.prefs_win_obj);
    ui.prefs_win_obj = NULL;
    ui.prefs_window = NULL;
}
