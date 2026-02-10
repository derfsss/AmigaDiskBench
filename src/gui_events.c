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
#include <interfaces/intuition.h>
#include <interfaces/listbrowser.h>
#include <stdio.h>
#include <string.h>

void HandleWorkerReply(struct Message *m)
{
    LOG_DEBUG("GUI: Worker Msg received at %p. Type=%d", m, m->mn_Node.ln_Type);
    if (m->mn_Node.ln_Type == NT_REPLYMSG || m->mn_Node.ln_Type == NT_MESSAGE) {
        BenchStatus *st = (BenchStatus *)m;
        if (st->msg_type == MSG_TYPE_STATUS) {
            if (st->finished) {
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                           (uint32) "[ IDLE ]", TAG_DONE);
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.target_chooser, ui.window, NULL, GA_Disabled, FALSE,
                                           TAG_DONE);
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.test_chooser, ui.window, NULL, GA_Disabled, FALSE,
                                           TAG_DONE);
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.pass_gad, ui.window, NULL, GA_Disabled, FALSE, TAG_DONE);
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.block_chooser, ui.window, NULL, GA_Disabled, FALSE,
                                           TAG_DONE);
                IIntuition->SetGadgetAttrs((struct Gadget *)ui.run_button, ui.window, NULL, GA_Disabled, FALSE, GA_Text,
                                           (uint32)GetString(8, "Run Benchmark"), TAG_DONE);
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
                        11, LBNA_Column, COL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.timestamp,
                        LBNA_Column, COL_VOL, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.volume_name,
                        LBNA_Column, COL_TEST, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)tn, LBNA_Column, COL_BS,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)abs, LBNA_Column, COL_PASSES, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)aps, LBNA_Column, COL_MBPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ms,
                        LBNA_Column, COL_IOPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)is, LBNA_Column, COL_DEVICE,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.device_name, LBNA_Column, COL_UNIT,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ut, LBNA_Column, COL_VER, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)st->result.app_version, LBNA_Column, COL_DUMMY, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32) "", LBNA_UserData, (uint32)res, TAG_DONE);
                    if (n) {
                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels,
                                                   (ULONG)-1, TAG_DONE);
                        IExec->AddTail(&ui.bench_labels, n);
                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels,
                                                   (uint32)&ui.bench_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
                        IIntuition->RefreshGList((struct Gadget *)ui.bench_list, ui.window, NULL, 1);
                    }
                }
            }
            IExec->FreeVec(st);
        } else {
            /* Free replied Job messages */
            IExec->FreeVec(m);
        }
    }
}

void HandleWorkbenchMessage(struct ApplicationMsg *amsg, BOOL *running)
{
    switch (amsg->type) {
    case APPLIBMT_Quit:
        *running = FALSE;
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
}

void HandleGUIEvent(uint32 result, uint16 code, BOOL *running)
{
    uint32 gid = result & WMHI_GADGETMASK;
    switch (result & WMHI_CLASSMASK) {
    case WMHI_CLOSEWINDOW:
        *running = FALSE;
        break;
    case WMHI_GADGETUP:
        switch (gid) {
        case GID_TABS: {
            uint32 t = 0;
            IIntuition->GetAttr(CLICKTAB_Current, ui.tabs, &t);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.page_obj, ui.window, NULL, PAGE_Current, t, TAG_DONE);
            break;
        }
        case GID_RUN_ALL:
            LaunchBenchmarkJob();
            break;
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
                    *running = FALSE;
                    break;
                case MID_ABOUT: {
                    struct EasyStruct es = {sizeof(struct EasyStruct), 0, "About AmigaDiskBench", APP_ABOUT_MSG, "OK"};
                    IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE);
                    break;
                }
                case MID_PREFS:
                    OpenPrefsWindow();
                    break;
                case MID_DELETE_PREFS: {
                    struct EasyStruct es = {
                        sizeof(struct EasyStruct), 0, "Confirm Preference Deletion",
                        "This will delete your preferences file\nand exit the application.\n\nContinue?", "OK|Cancel"};
                    if (IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE) == 1) {
                        ui.delete_prefs_needed = TRUE;
                        *running = FALSE;
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

void HandlePrefsEvent(uint32 result, uint16 code)
{
    uint32 gid = result & WMHI_GADGETMASK;
    switch (result & WMHI_CLASSMASK) {
    case WMHI_CLOSEWINDOW:
        IIntuition->IDoMethod(ui.prefs_win_obj, WM_CLOSE);
        IIntuition->DisposeObject(ui.prefs_win_obj);
        ui.prefs_win_obj = NULL;
        ui.prefs_window = NULL;
        break;
    case WMHI_GADGETUP:
        if (gid == GID_PREFS_SAVE) {
            UpdatePreferences();
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
