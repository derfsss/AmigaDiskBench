/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (C) 2026 Team Derfs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "gui_internal.h"
#include <stdio.h>
#include <string.h>

void RefreshDriveList(void)
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

BOOL InitSystemResources(void)
{
    /* Open required libraries and interfaces */
    ui.IconBase = IExec->OpenLibrary("icon.library", 0);
    if (ui.IconBase)
        ui.IIcn = (struct IconIFace *)IExec->GetInterface(ui.IconBase, "main", 1, NULL);
    else
        LOG_DEBUG("InitSystemResources: FAILED to open icon.library");

    ui.LocaleBase = IExec->OpenLibrary("locale.library", 0);
    if (ui.LocaleBase)
        ui.ILoc = (struct LocaleIFace *)IExec->GetInterface(ui.LocaleBase, "main", 1, NULL);
    else
        LOG_DEBUG("InitSystemResources: FAILED to open locale.library");

    ui.ApplicationBase = IExec->OpenLibrary("application.library", 52);
    if (ui.ApplicationBase) {
        ui.IApp = (struct ApplicationIFace *)IExec->GetInterface(ui.ApplicationBase, "application", 2, NULL);
        ui.IPrefsObjects = (struct PrefsObjectsIFace *)IExec->GetInterface(ui.ApplicationBase, "prefsobjects", 2, NULL);
    } else {
        LOG_DEBUG("InitSystemResources: FAILED to open application.library v52");
    }

    ui.AslBase = IExec->OpenLibrary("asl.library", 0);
    if (ui.AslBase)
        ui.IAsl = (struct AslIFace *)IExec->GetInterface(ui.AslBase, "main", 1, NULL);
    else
        LOG_DEBUG("InitSystemResources: FAILED to open asl.library");

    /* Open ReAction Classes Explicitly */
    ui.WindowBase = IIntuition->OpenClass("window.class", MINVERSION, &ui.WindowClass);
    ui.LayoutBase = IIntuition->OpenClass("gadgets/layout.gadget", MINVERSION, &ui.LayoutClass);
    ui.ButtonBase = IIntuition->OpenClass("gadgets/button.gadget", MINVERSION, &ui.ButtonClass);
    ui.ListBrowserBase = IIntuition->OpenClass("gadgets/listbrowser.gadget", MINVERSION, &ui.ListBrowserClass);
    ui.ChooserBase = IIntuition->OpenClass("gadgets/chooser.gadget", MINVERSION, &ui.ChooserClass);
    ui.IntegerBase = IIntuition->OpenClass("gadgets/integer.gadget", MINVERSION, &ui.IntegerClass);
    ui.TextEditorBase = IIntuition->OpenClass("gadgets/texteditor.gadget", MINVERSION, &ui.TextEditorClass);
    ui.ScrollerBase = IIntuition->OpenClass("gadgets/scroller.gadget", MINVERSION, &ui.ScrollerClass);
    ui.CheckBoxBase = IIntuition->OpenClass("gadgets/checkbox.gadget", MINVERSION, &ui.CheckBoxClass);
    ui.ClickTabBase = IIntuition->OpenClass("gadgets/clicktab.gadget", MINVERSION, &ui.ClickTabClass);
    ui.LabelBase = IIntuition->OpenClass("images/label.image", MINVERSION, &ui.LabelClass);
    ui.StringBase = IIntuition->OpenClass("gadgets/string.gadget", MINVERSION, &ui.StringClass);

    /* Verify bundled page.gadget */
    ui.PageAvailable = FALSE;
    if (ui.LayoutClass) {
        Object *dummy = IIntuition->NewObject(NULL, "page.gadget", TAG_DONE);
        if (dummy) {
            IIntuition->DisposeObject(dummy);
            ui.PageAvailable = TRUE;
        }
    }

    if (!ui.IIcn || !ui.ILoc || !ui.IApp || !ui.IAsl || !ui.WindowClass || !ui.LayoutClass || !ui.ButtonClass ||
        !ui.ListBrowserClass || !ui.ChooserClass || !ui.IntegerClass || !ui.TextEditorClass || !ui.ScrollerClass ||
        !ui.CheckBoxClass || !ui.LabelClass || !ui.StringClass) {
        LOG_DEBUG("InitSystemResources: FAILED to open critical GUI resources");
        return FALSE;
    }

    if (ui.ILoc)
        ui.catalog = ui.ILoc->OpenCatalog(NULL, "AmigaDiskBench.catalog", OC_BuiltInLanguage, "english", OC_Version, 1,
                                          TAG_DONE);

    return TRUE;
}

void CleanupSystemResources(void)
{
    LOG_DEBUG("CleanupSystemResources: Starting");

    if (ui.catalog && ui.ILoc) {
        ui.ILoc->CloseCatalog(ui.catalog);
        ui.catalog = NULL;
    }

    if (ui.IApp) {
        IExec->DropInterface((struct Interface *)ui.IApp);
        ui.IApp = NULL;
    }
    if (ui.IPrefsObjects) {
        IExec->DropInterface((struct Interface *)ui.IPrefsObjects);
        ui.IPrefsObjects = NULL;
    }
    if (ui.ApplicationBase) {
        IExec->CloseLibrary(ui.ApplicationBase);
        ui.ApplicationBase = NULL;
    }

    if (ui.ILoc) {
        IExec->DropInterface((struct Interface *)ui.ILoc);
        ui.ILoc = NULL;
    }
    if (ui.LocaleBase) {
        IExec->CloseLibrary(ui.LocaleBase);
        ui.LocaleBase = NULL;
    }

    if (ui.IIcn) {
        IExec->DropInterface((struct Interface *)ui.IIcn);
        ui.IIcn = NULL;
    }
    if (ui.IconBase) {
        IExec->CloseLibrary(ui.IconBase);
        ui.IconBase = NULL;
    }

    if (ui.IAsl) {
        IExec->DropInterface((struct Interface *)ui.IAsl);
        ui.IAsl = NULL;
    }
    if (ui.AslBase) {
        IExec->CloseLibrary(ui.AslBase);
        ui.AslBase = NULL;
    }

    /* Close ReAction classes */
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
    if (ui.ScrollerBase)
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

    LOG_DEBUG("CleanupSystemResources: Finished");
}
