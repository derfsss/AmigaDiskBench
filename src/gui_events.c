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

void UpdateBulkTabInfo(void)
{
    if (!ui.bulk_info_label || !ui.window)
        return;

    char buf[128];
    char test_name[32];

    uint32 run_all_tests = 0;
    uint32 run_all_blocks = 0;

    if (ui.bulk_all_tests_check)
        IIntuition->GetAttr(CHECKBOX_Checked, ui.bulk_all_tests_check, &run_all_tests);
    if (ui.bulk_all_blocks_check)
        IIntuition->GetAttr(CHECKBOX_Checked, ui.bulk_all_blocks_check, &run_all_blocks);

    if (run_all_tests) {
        snprintf(test_name, sizeof(test_name), "All Test Types");
    } else {
        snprintf(test_name, sizeof(test_name), "%s", TestTypeToDisplayName(ui.current_test_type));
    }

    const char *block_str;
    if (run_all_blocks) {
        block_str = "All Block Sizes";
    } else {
        block_str = FormatPresetBlockSize(ui.current_block_size);
    }

    snprintf(buf, sizeof(buf), "Settings: %s / %u Passes / %s", test_name, (unsigned int)ui.current_passes, block_str);

    IIntuition->SetGadgetAttrs((struct Gadget *)ui.bulk_info_label, ui.window, NULL, GA_Text, (uint32)buf, TAG_DONE);
}

void HandleWorkerReply(struct Message *m)
{
    LOG_DEBUG("GUI: Worker Msg received at %p. Type=%d", m, m->mn_Node.ln_Type);
    if (m->mn_Node.ln_Type == NT_REPLYMSG || m->mn_Node.ln_Type == NT_MESSAGE) {
        BenchStatus *st = (BenchStatus *)m;
        if (st->msg_type == MSG_TYPE_STATUS) {
            if (st->finished) {
                /* Job finished, try to dispatch next one */
                ui.worker_busy = FALSE; /* Briefly strictly to allow DispatchNextJob to see we are ready */
                DispatchNextJob();

                /* Only reset UI if queue is empty AND worker is not busy */
                if (IsQueueEmpty() && !ui.worker_busy) {
                    IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                               (uint32) "[ IDLE ]", TAG_DONE);

                    SetGadgetState(GID_VOL_CHOOSER, FALSE);
                    SetGadgetState(GID_TEST_CHOOSER, FALSE);
                    SetGadgetState(GID_NUM_PASSES, FALSE);
                    SetGadgetState(GID_BLOCK_SIZE, FALSE);

                    /* Enable run button and restore label */
                    SetGadgetState(GID_RUN_ALL, FALSE);
                    IIntuition->SetGadgetAttrs((struct Gadget *)ui.run_button, ui.window, NULL, GA_Text,
                                               (uint32)GetString(8, "Run Benchmark"), TAG_DONE);

                    /* Force visual refresh for ReAction layout to reflect enabled state */
                    IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
                }
                if (st->success) {
                    char tn[64], ms[32], is[32], ut[32];
                    snprintf(ms, sizeof(ms), "%.2f", st->result.mb_per_sec);
                    snprintf(is, sizeof(is), "%u", (unsigned int)st->result.iops);
                    snprintf(ut, sizeof(ut), "%u", (unsigned int)st->result.device_unit);
                    snprintf(tn, sizeof(tn), "%s", TestTypeToString(st->result.type));
                    /* Prepare BenchResult for UserData */
                    BenchResult *res = IExec->AllocVecTags(sizeof(BenchResult), AVT_Type, MEMF_SHARED,
                                                           AVT_ClearWithValue, 0, TAG_DONE);
                    if (res) {
                        memcpy(res, &st->result, sizeof(BenchResult));

                        /* Calculate Comparison */
                        BenchResult prev;
                        if (FindMatchingResult(res, &prev)) {
                            res->prev_mbps = prev.mb_per_sec;
                            res->prev_iops = prev.iops;
                            snprintf(res->prev_timestamp, sizeof(res->prev_timestamp), "%s", prev.timestamp);
                            if (prev.mb_per_sec > 0) {
                                res->diff_per = ((res->mb_per_sec - prev.mb_per_sec) / prev.mb_per_sec) * 100.0f;
                            }
                        }
                    }

                    char aps[16], abs[32], ds[32];
                    snprintf(aps, sizeof(aps), "%u", (unsigned int)st->result.passes);
                    snprintf(abs, sizeof(abs), "%s", FormatPresetBlockSize(st->result.block_size));
                    if (res && res->prev_mbps > 0) {
                        snprintf(ds, sizeof(ds), "%+.1f%%", res->diff_per);
                    } else {
                        snprintf(ds, sizeof(ds), "N/A");
                    }

                    struct Node *n = IListBrowser->AllocListBrowserNode(
                        12, LBNA_Column, COL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.timestamp,
                        LBNA_Column, COL_VOL, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.volume_name,
                        LBNA_Column, COL_TEST, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)tn, LBNA_Column, COL_BS,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)abs, LBNA_Column, COL_PASSES, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)aps, LBNA_Column, COL_MBPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ms,
                        LBNA_Column, COL_IOPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)is, LBNA_Column, COL_DEVICE,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.device_name, LBNA_Column, COL_UNIT,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ut, LBNA_Column, COL_VER, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)st->result.app_version, LBNA_Column, COL_DIFF, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)ds, LBNA_Column, COL_DUMMY, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32) "",
                        LBNA_UserData, (uint32)res, TAG_DONE);
                    if (n) {
                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels,
                                                   (ULONG)-1, TAG_DONE);
                        IExec->AddTail(&ui.bench_labels, n);
                        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels,
                                                   (uint32)&ui.bench_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
                        IIntuition->RefreshGList((struct Gadget *)ui.bench_list, ui.window, NULL, 1);
                        UpdateVisualization();
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
            /* Refresh Bulk Tab Info when switching tabs (Tab 3 is Bulk) */
            if (t == 3) {
                UpdateBulkTabInfo();
            }
            break;
        }
        case GID_TEST_CHOOSER:
            IIntuition->GetAttr(CHOOSER_Selected, ui.test_chooser, &ui.current_test_type);
            LOG_DEBUG("GUI: Test Type changed to %u", ui.current_test_type);
            UpdateBulkTabInfo();
            break;
        case GID_NUM_PASSES:
            IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &ui.current_passes);
            LOG_DEBUG("GUI: Passes changed to %u", ui.current_passes);
            UpdateBulkTabInfo();
            break;
        case GID_BLOCK_SIZE: {
            struct Node *bn = NULL;
            IIntuition->GetAttr(CHOOSER_SelectedNode, ui.block_chooser, (uint32 *)&bn);
            ui.current_block_size = 4096; /* Safety fallback */
            if (bn) {
                uint32 bs_val = 0;
                IChooser->GetChooserNodeAttrs(bn, CNA_UserData, &bs_val, TAG_DONE);
                ui.current_block_size = bs_val;
            }
            LOG_DEBUG("GUI: Block Size changed to %u", ui.current_block_size);
            UpdateBulkTabInfo();
            break;
        }
        case GID_RUN_ALL:
            ui.jobs_pending = 1;
            LaunchBenchmarkJob();
            break;
        case GID_BULK_RUN:
            LaunchBulkJobs();
            break;
        case GID_BULK_ALL_TESTS:
        case GID_BULK_ALL_BLOCKS:
            UpdateBulkTabInfo();
            break;
        case GID_FLUSH_CACHE:
            ui.flush_cache = code;
            break;
        case GID_VIZ_FILTER_VOLUME:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_volume, &ui.viz_filter_volume_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_FILTER_TEST:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_test, &ui.viz_filter_test_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_FILTER_METRIC:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_metric, &ui.viz_filter_metric_idx);
            UpdateVisualization();
            break;
        case GID_REFRESH_HISTORY:
            RefreshHistory();
            break;
        case GID_REFRESH_DRIVES:
            if (!ui.worker_busy) {
                ClearHardwareInfoCache();
                RefreshDriveList();
                ShowMessage("Drives Refreshed", "Drive list and hardware info cache\nhave been refreshed.", "OK");
            }
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
                case MID_ABOUT:
                    ShowMessage("About AmigaDiskBench", APP_ABOUT_MSG, "OK");
                    break;
                case MID_PREFS:
                    OpenPrefsWindow();
                    break;
                case MID_DELETE_PREFS:
                    if (ShowConfirm("Confirm Preference Deletion",
                                    "This will delete your preferences file\nand exit the application.\n\nContinue?",
                                    "OK|Cancel")) {
                        ui.delete_prefs_needed = TRUE;
                        *running = FALSE;
                    }
                    break;
                case MID_SHOW_DETAILS:
                    ShowBenchmarkDetails(ui.history_list);
                    break;
                case MID_EXPORT_TEXT: {
                    if (ui.IAsl) {
                        struct FileRequester *req = ui.IAsl->AllocAslRequestTags(
                            ASL_FileRequest, ASLFR_TitleText, (uint32) "Export History to ANSI Text", ASLFR_DoSaveMode,
                            TRUE, ASLFR_InitialFile, (uint32) "AmigaDiskBench_Report.txt", TAG_DONE);
                        if (req) {
                            if (ui.IAsl->AslRequestTags(req, ASLFR_Window, (uint32)ui.window, TAG_DONE)) {
                                char path[512];
                                snprintf(path, sizeof(path), "%s", req->fr_Drawer);
                                path[sizeof(path) - 1] = '\0';
                                IDOS->AddPart(path, req->fr_File, sizeof(path));
                                ExportToAnsiText(path);
                            }
                            ui.IAsl->FreeAslRequest(req);
                        }
                    }
                    break;
                }
                }
                mid = mi->NextSelect;
            } else {
                mid = MENUNULL;
            }
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
