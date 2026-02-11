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

#include "gui_internal.h"
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

/* Global UI state */
GUIState ui;

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

    if (!InitSystemResources()) {
        CleanupSystemResources();
        return 1;
    }

    IExec->NewList(&ui.history_labels);
    IExec->NewList(&ui.bench_labels);
    IExec->NewList(&ui.drive_list);
    IExec->NewList(&ui.test_labels);
    IExec->NewList(&ui.block_list);
    IExec->NewList(&ui.bulk_labels);

    /* Initialize nodes for choosers */
    const char *blocks[] = {"4K", "16K", "32K", "64K", "128K", "256K", "1M"};
    uint32 block_vals[] = {4096, 16384, 32768, 65536, 131072, 262144, 1048576};
    for (int i = 0; i < 7; i++) {
        struct Node *bn =
            IChooser->AllocChooserNode(CNA_Text, blocks[i], CNA_UserData, (void *)block_vals[i], TAG_DONE);
        if (bn)
            IExec->AddTail(&ui.block_list, bn);
    }
    struct Node *tnNodes[] = {
        IChooser->AllocChooserNode(CNA_Text, GetString(11, "Sprinter (10MB)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(12, "HeavyLifter (100MB)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(13, "Legacy (4MB)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(14, "DailyGrind (Random 4K-64K)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(17, "Sequential (Professional)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(18, "Random 4K (Professional)"), TAG_DONE),
        IChooser->AllocChooserNode(CNA_Text, GetString(19, "Profiler (Professional)"), TAG_DONE)};
    for (int i = 0; i < sizeof(tnNodes) / sizeof(tnNodes[0]); i++) {
        if (tnNodes[i])
            IExec->AddTail(&ui.test_labels, tnNodes[i]);
    }

    if (!(ui.gui_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL)))
        return 1;
    if (!(ui.worker_reply_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL)))
        return 1;
    if (!(ui.prefs_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL)))
        return 1;

    worker_proc = IDOS->CreateNewProcTags(NP_Entry, (uint32)BenchmarkWorker, NP_Name, (uint32) "AmigaDiskBench_Worker",
                                          NP_Child, TRUE, TAG_DONE);
    if (!worker_proc)
        return 1;
    ui.worker_port = &worker_proc->pr_MsgPort;

    icon = ui.IIcn->GetDiskObject("PROGDIR:AmigaDiskBench");
    if (!icon)
        icon = ui.IIcn->GetDiskObject("ENV:sys/def_tool");

    struct List tab_list;
    IExec->NewList(&tab_list);
    struct Node *tab_bench = IClickTab->AllocClickTabNode(TNA_Text, GetString(1, "Benchmark"), TNA_Number, 0, TAG_DONE);
    struct Node *tab_history = IClickTab->AllocClickTabNode(TNA_Text, GetString(2, "History"), TNA_Number, 1, TAG_DONE);
    struct Node *tab_viz =
        IClickTab->AllocClickTabNode(TNA_Text, GetString(15, "Visualization"), TNA_Number, 2, TAG_DONE);
    struct Node *tab_bulk = IClickTab->AllocClickTabNode(TNA_Text, GetString(16, "Bulk"), TNA_Number, 3, TAG_DONE);
    if (tab_bench)
        IExec->AddTail(&tab_list, tab_bench);
    if (tab_history)
        IExec->AddTail(&tab_list, tab_history);
    if (tab_viz)
        IExec->AddTail(&tab_list, tab_viz);
    if (tab_bulk)
        IExec->AddTail(&tab_list, tab_bulk);

    /* Register application */
    ui.app_id = ui.IApp->RegisterApplication(APP_TITLE, REGAPP_URLIdentifier, "diskbench.derfs.co.uk",
                                             REGAPP_Description, APP_DESCRIPTION, REGAPP_NoIcon, TRUE,
                                             REGAPP_HasIconifyFeature, TRUE, REGAPP_HasPrefsWindow, TRUE,
                                             REGAPP_LoadPrefs, TRUE, REGAPP_SavePrefs, TRUE, TAG_DONE);
    if (ui.app_id) {
        ui.IApp->GetApplicationAttrs(ui.app_id, APPATTR_Port, (uint32 *)&ui.app_port, TAG_DONE);
    }

    ui.win_obj = CreateMainLayout(icon, &tab_list);

    if (!ui.win_obj) {
        IClickTab->FreeClickTabList(&tab_list);
        /* Signal worker to quit */
        BenchJob qj = {.type = (BenchTestType)-1, .msg.mn_ReplyPort = ui.worker_reply_port};
        IExec->PutMsg(ui.worker_port, &qj.msg);
        IExec->WaitPort(ui.worker_reply_port);
        IExec->GetMsg(ui.worker_reply_port);
        CleanupSystemResources();
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

        /* Select Default Drive */
        struct Node *sel_node = NULL, *vn;
        CONST_STRPTR target_drive = (ui.default_drive[0] != '\0') ? ui.default_drive : pv;
        for (vn = IExec->GetHead(&ui.drive_list); vn; vn = IExec->GetSucc(vn)) {
            DriveNodeData *dd = NULL;
            IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
            if (dd && dd->bare_name && strcasecmp(dd->bare_name, target_drive) == 0) {
                sel_node = vn;
                break;
            }
        }
        if (!sel_node && ui.default_drive[0] != '\0') {
            for (vn = IExec->GetHead(&ui.drive_list); vn; vn = IExec->GetSucc(vn)) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name && strcasecmp(dd->bare_name, pv) == 0) {
                    sel_node = vn;
                    break;
                }
            }
        }
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL,
                                   sel_node ? CHOOSER_SelectedNode : CHOOSER_Selected, sel_node ? (uint32)sel_node : 0,
                                   TAG_DONE);

        uint32 win_sig = 0;
        IIntuition->GetAttr(WINDOW_SigMask, ui.win_obj, &win_sig);
        uint32 wait_mask = win_sig | (1L << ui.gui_port->mp_SigBit) | (1L << ui.worker_reply_port->mp_SigBit) |
                           (ui.app_port ? (1L << ui.app_port->mp_SigBit) : 0) | (1L << ui.prefs_port->mp_SigBit) |
                           SIGBREAKF_CTRL_C;
        BOOL running = TRUE;
        while (running) {
            uint32 sig =
                IExec->Wait(wait_mask | (ui.details_win_obj ? (1L << ui.details_window->UserPort->mp_SigBit) : 0));
            if (sig & SIGBREAKF_CTRL_C)
                running = FALSE;

            if (sig & (1L << ui.worker_reply_port->mp_SigBit)) {
                struct Message *m;
                while ((m = IExec->GetMsg(ui.worker_reply_port)))
                    HandleWorkerReply(m);
            }
            if (ui.app_port && (sig & (1L << ui.app_port->mp_SigBit))) {
                struct ApplicationMsg *amsg;
                while ((amsg = (struct ApplicationMsg *)IExec->GetMsg(ui.app_port))) {
                    HandleWorkbenchMessage(amsg, &running);
                    IExec->ReplyMsg((struct Message *)amsg);
                }
            }
            if (sig & win_sig) {
                uint16 code;
                uint32 result;
                while ((result = IIntuition->IDoMethod(ui.win_obj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG)
                    HandleGUIEvent(result, code, &running);
            }
            if (ui.prefs_win_obj && (sig & (1L << ui.prefs_port->mp_SigBit))) {
                uint16 pcode;
                uint32 presult;
                while (ui.prefs_win_obj &&
                       (presult = IIntuition->IDoMethod(ui.prefs_win_obj, WM_HANDLEINPUT, &pcode)) != WMHI_LASTMSG)
                    HandlePrefsEvent(presult, pcode);
            }
            if (ui.details_win_obj && (sig & (1L << ui.details_window->UserPort->mp_SigBit))) {
                uint16 dcode;
                uint32 dresult;
                while (ui.details_win_obj &&
                       (dresult = IIntuition->IDoMethod(ui.details_win_obj, WM_HANDLEINPUT, &dcode)) != WMHI_LASTMSG)
                    HandleDetailsWindowEvent(dcode, dresult);
            }
        }
        IClickTab->FreeClickTabList(&tab_list);
        /* Signal worker to quit and wait for it */
        BenchJob qj = {.type = (BenchTestType)-1, .msg.mn_ReplyPort = ui.worker_reply_port};
        IExec->PutMsg(ui.worker_port, &qj.msg);
        IExec->WaitPort(ui.worker_reply_port);
        IExec->GetMsg(ui.worker_reply_port);

        /* Node data cleanup */
        struct Node *n, *nx;
        for (n = IExec->GetHead(&ui.history_labels); n; n = nx) {
            nx = IExec->GetSucc(n);
            void *udata = NULL;
            IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &udata, TAG_DONE);
            if (udata)
                IExec->FreeVec(udata);
            IListBrowser->FreeListBrowserNode(n);
        }
        for (n = IExec->GetHead(&ui.bench_labels); n; n = nx) {
            nx = IExec->GetSucc(n);
            void *udata = NULL;
            IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &udata, TAG_DONE);
            if (udata)
                IExec->FreeVec(udata);
            IListBrowser->FreeListBrowserNode(n);
        }
        for (n = IExec->GetHead(&ui.test_labels); n; n = nx) {
            nx = IExec->GetSucc(n);
            IChooser->FreeChooserNode(n);
        }
        for (n = IExec->GetHead(&ui.block_list); n; n = nx) {
            nx = IExec->GetSucc(n);
            IChooser->FreeChooserNode(n);
        }
        for (n = IExec->GetHead(&ui.drive_list); n; n = nx) {
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
        }

        if (ui.app_id && ui.IApp) {
            if (ui.delete_prefs_needed) {
                ui.IApp->SetApplicationAttrs(ui.app_id, APPATTR_SavePrefs, FALSE, TAG_DONE);
                char prefs_path[256];
                snprintf(prefs_path, sizeof(prefs_path), "ENVARC:%s.diskbench.derfs.co.uk.xml", APP_TITLE);
                IDOS->Delete(prefs_path);
                snprintf(prefs_path, sizeof(prefs_path), "ENV:%s.diskbench.derfs.co.uk.xml", APP_TITLE);
                IDOS->Delete(prefs_path);
            }
            ui.IApp->UnregisterApplication(ui.app_id, TAG_DONE);
        }

        if (ui.win_obj)
            IIntuition->DisposeObject(ui.win_obj);
        if (ui.gui_port)
            IExec->FreeSysObject(ASOT_PORT, ui.gui_port);
        if (ui.worker_reply_port)
            IExec->FreeSysObject(ASOT_PORT, ui.worker_reply_port);
        if (ui.prefs_port)
            IExec->FreeSysObject(ASOT_PORT, ui.prefs_port);

        if (icon)
            ui.IIcn->FreeDiskObject(icon);

        CleanupSystemResources();
        return 0;
    }
    CleanupSystemResources();
    return 1;
}
