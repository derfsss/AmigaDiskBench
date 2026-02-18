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
#include <graphics/gfxmacros.h>
#include <interfaces/arexx.h>
#include <interfaces/locale.h>
#include <proto/locale.h>
#include <stdlib.h>
#include <utility/hooks.h>

/* Traffic Light Render Hook */
static struct Hook traffic_light_hook;

static LONG ObtainTrafficPen(uint32 rgb)
{
    struct ColorMap *cm = NULL;
    if (ui.window && ui.window->WScreen) {
        cm = ui.window->WScreen->ViewPort.ColorMap;
    }
    if (!cm)
        return 1;

    uint32 r = (rgb >> 16) & 0xFF;
    uint32 g = (rgb >> 8) & 0xFF;
    uint32 b = rgb & 0xFF;

    return IGraphics->ObtainBestPen(cm, r << 24, g << 24, b << 24, OBP_Precision, PRECISION_IMAGE, TAG_DONE);
}

static void ReleaseTrafficPen(LONG pen)
{
    struct ColorMap *cm = NULL;
    if (ui.window && ui.window->WScreen) {
        cm = ui.window->WScreen->ViewPort.ColorMap;
    }
    if (cm && pen >= 0) {
        IGraphics->ReleasePen(cm, pen);
    }
}

void UpdateTrafficLabel(BOOL busy)
{
    LOG_DEBUG("UpdateTrafficLabel: Busy=%d, Window=%p, LabelObj=%p", busy, ui.window, ui.traffic_label);

    if (ui.window && ui.traffic_label) {
        /* Update the ButtonObject text - Using RefreshSetGadgetAttrs to ensure redraw */
        IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.traffic_label, ui.window, NULL, GA_Text,
                                          (uint32)(busy ? "Benchmarking..." : "Ready!"), TAG_DONE);

        /* Also trigger traffic light redraw */
        IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.traffic_light, ui.window, NULL, TAG_DONE);
    } else {
        LOG_DEBUG("UpdateTrafficLabel: SKIPPED (Window or Label missing)");
    }
}

static uint32 TrafficLightDraw(struct Hook *hook, Object *obj, struct gpRender *gpr)
{
    struct RastPort *rp = gpr->gpr_RPort;
    if (!rp)
        return 0;

    struct IBox *box = NULL;
    IIntuition->GetAttr(SPACE_AreaBox, obj, (uint32 *)&box);

    if (box) {
        /* Red (Busy) or Green (Idle) */
        uint32 color = ui.worker_busy ? 0x00FF0000 : 0x0000FF00;

        /* Update Label Text based on busy state */
        /* Note: Doing this in RenderHook is risky but might work if SetGadgetAttrs doesn't trigger redraw of THIS
         * gadget */
        // Better to do it where busy is toggled.
        // I will NOT do it here. I will find the toggle point.

        LONG pen = ObtainTrafficPen(color);

        if (pen >= 0) {
            IGraphics->SetAPen(rp, pen);
            IGraphics->RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
            ReleaseTrafficPen(pen);
        } else {
            /* Fallback to simple pens if Obtain fails */
            IGraphics->SetAPen(rp, ui.worker_busy ? 2 : 1);
            IGraphics->RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
        }
    }
    return 0;
}

/* Static render hook for visualization SpaceObject */
static struct Hook viz_hook;

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
    snprintf(ui.csv_path, sizeof(ui.csv_path), "%s", DEFAULT_CSV_PATH);
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
    IExec->NewList(&ui.health_labels);

    InitBenchmarkQueue();

    /* Initialize nodes for choosers */
    const char *blocks[] = {"4K", "16K", "32K", "64K", "128K", "256K", "1M"};
    uint32 block_vals[] = {4096, 16384, 32768, 65536, 131072, 262144, 1048576};
    for (int i = 0; i < 7; i++) {
        struct Node *bn =
            IChooser->AllocChooserNode(CNA_Text, blocks[i], CNA_UserData, (void *)block_vals[i], TAG_DONE);
        if (bn)
            IExec->AddTail(&ui.block_list, bn);
    }
    /* Dynamically populate test types from the engine definition */
    for (int i = 0; i < TEST_COUNT; i++) {
        struct Node *tn = IChooser->AllocChooserNode(CNA_Text, TestTypeToDisplayName((BenchTestType)i), TAG_DONE);
        if (tn)
            IExec->AddTail(&ui.test_labels, tn);
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

    /* Get Utility Interface (for Date functions) */
    struct Library *UtilityBase = IExec->OpenLibrary("utility.library", 53);
    if (UtilityBase) {
        ui.IUtility = (struct UtilityIFace *)IExec->GetInterface(UtilityBase, "main", 1, NULL);
        IExec->CloseLibrary(UtilityBase); /* As per OS4 spec, interface holds lib? No, usually keep lib open or depends.
                                             ReAction classes handled differently. */
        /* Actually, let's keep it simple. If IExec->OpenLibrary and then GetInterface, usually you keep lib open if not
         * using interface counting */
        /* But looking at existing code, it uses IApp etc. Let's refer to InitSystemResources in gui_system.c if
         * existing. */
        /* gui.c line 155 is StartGUI. InitSystemResources is called later! */
    }

    icon = ui.IIcn->GetDiskObject("PROGDIR:AmigaDiskBench");
    if (!icon)
        icon = ui.IIcn->GetDiskObject("ENV:sys/def_tool");

    struct List tab_list;
    IExec->NewList(&tab_list);
    struct Node *tab_bench = IClickTab->AllocClickTabNode(TNA_Text, GetString(1, "Benchmark"), TNA_Number, 0, TAG_DONE);
    struct Node *tab_history = IClickTab->AllocClickTabNode(TNA_Text, GetString(2, "History"), TNA_Number, 1, TAG_DONE);
    struct Node *tab_viz =
        IClickTab->AllocClickTabNode(TNA_Text, GetString(15, "Visualization"), TNA_Number, 2, TAG_DONE);
    struct Node *tab_health = IClickTab->AllocClickTabNode(TNA_Text, "Drive Health", TNA_Number, 3, TAG_DONE);
    struct Node *tab_bulk = IClickTab->AllocClickTabNode(TNA_Text, GetString(16, "Bulk"), TNA_Number, 4, TAG_DONE);
    if (tab_bench)
        IExec->AddTail(&tab_list, tab_bench);
    if (tab_history)
        IExec->AddTail(&tab_list, tab_history);
    if (tab_viz)
        IExec->AddTail(&tab_list, tab_viz);
    if (tab_health)
        IExec->AddTail(&tab_list, tab_health);
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

    /* Initialize visualization filter labels (must be before CreateMainLayout) */
    InitVizFilterLabels();

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
        char pv[MAX_PATH_LEN];
        if (IDOS->NameFromLock(IDOS->GetProgramDir(), pv, sizeof(pv))) {
            char *c = strchr(pv, ':');
            if (c)
                *(c + 1) = '\0';
        } else
            snprintf(pv, sizeof(pv), "SYS:");

        /* Install render hook on visualization SpaceObject */
        memset(&viz_hook, 0, sizeof(viz_hook));
        viz_hook.h_Entry = (HOOKFUNC)VizRenderHook;
        viz_hook.h_Data = NULL;
        if (ui.viz_canvas) {
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_canvas, ui.window, NULL, SPACE_RenderHook,
                                       (uint32)&viz_hook, TAG_DONE);
        }

        /* Install Traffic Light Hook */
        memset(&traffic_light_hook, 0, sizeof(traffic_light_hook));
        traffic_light_hook.h_Entry = (HOOKFUNC)TrafficLightDraw;
        traffic_light_hook.h_Data = NULL;
        if (ui.traffic_light) {
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.traffic_light, ui.window, NULL, SPACE_RenderHook,
                                       (uint32)&traffic_light_hook, TAG_DONE);
            /* Trigger initial refresh */
            IIntuition->RefreshGList((struct Gadget *)ui.traffic_light, ui.window, NULL, 1);
        }

        RefreshDriveList();
        LoadPrefs();
        UpdateBulkTabInfo();
        RefreshHistory();
        RefreshVizVolumeFilter();

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

        /* Update volume info for initially selected drive */
        if (sel_node) {
            DriveNodeData *dd = NULL;
            IChooser->GetChooserNodeAttrs(sel_node, CNA_UserData, &dd, TAG_DONE);
            if (dd && dd->bare_name) {
                UpdateVolumeInfo(dd->bare_name);
            }
        } else {
            /* Fallback to program dir volume if selection failed */
            char prog_vol[32];
            char *c = strchr(pv, ':');
            if (c) {
                int len = c - pv + 1;
                if (len < sizeof(prog_vol)) {
                    strncpy(prog_vol, pv, len);
                    prog_vol[len] = '\0';
                    UpdateVolumeInfo(prog_vol);
                }
            }
        }

        IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL,
                                   sel_node ? CHOOSER_SelectedNode : CHOOSER_Selected, sel_node ? (uint32)sel_node : 0,
                                   TAG_DONE);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.health_target_chooser, ui.window, NULL,
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
                IExec->Wait(wait_mask | (ui.details_win_obj ? (1L << ui.details_window->UserPort->mp_SigBit) : 0) |
                            (ui.compare_win_obj ? (1L << ui.compare_window->UserPort->mp_SigBit) : 0));
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
                while ((result = IIntuition->IDoMethod(ui.win_obj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
                    /* Handle Mouse Move for Visualization Hover */
                    if ((result & WMHI_CLASSMASK) == WMHI_MOUSEMOVE) {
                        /* Check only if Visualization tab is active (Page 2) */
                        uint32 t = 0;
                        IIntuition->GetAttr(CLICKTAB_Current, ui.tabs, &t);
                        if (t == 2 && ui.window) {
                            VizCheckHover(ui.window->MouseX, ui.window->MouseY);
                        }
                    }
                    HandleGUIEvent(result, code, &running);
                }
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
            if (ui.compare_win_obj && (sig & (1L << ui.compare_window->UserPort->mp_SigBit))) {
                uint16 ccode;
                uint32 cresult;
                while (ui.compare_win_obj &&
                       (cresult = IIntuition->IDoMethod(ui.compare_win_obj, WM_HANDLEINPUT, &ccode)) != WMHI_LASTMSG)
                    HandleCompareWindowEvent(ccode, cresult);
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
        for (n = IExec->GetHead(&ui.health_labels); n; n = nx) {
            nx = IExec->GetSucc(n);
            IListBrowser->FreeListBrowserNode(n);
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
                char prefs_path[MAX_PATH_LEN];
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

        CleanupVizFilterLabels();

        if (icon)
            ui.IIcn->FreeDiskObject(icon);

        CleanupSystemResources();
        CleanupBenchmarkQueue();
        return 0;
    }
    CleanupVizFilterLabels();
    CleanupSystemResources();
    CleanupBenchmarkQueue();
    return 1;
}
