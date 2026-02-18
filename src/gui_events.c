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

#include "engine_internal.h" /* For GetFileSystemName, GetDeviceFromVolume */
#include "gui_internal.h"
#include <time.h>

/**
 * @brief Updates the info label on the Bulk tab based on current settings.
 *
 * Refreshes the text to show selected test type, pass count, and block size,
 * or "All" if the respective checkboxes are checked.
 */
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

/**
 * @brief Updates the volume information display for a selected drive.
 *
 * Queries the filesystem for size, free space, filesystem type, and device name,
 * then updates the read-only gadgets in the "Volume Information" group.
 *
 * @param volume The volume name (e.g., "DH0:").
 */
void UpdateVolumeInfo(const char *volume)
{
    if (!ui.window || !ui.vol_size_label)
        return;

    char size_buf[64] = "N/A";
    char free_buf[64] = "N/A";
    char fs_buf[64] = "N/A";
    char dev_buf[64] = "N/A";

    struct InfoData info;
    if (IDOS->GetDiskInfoTags(GDI_StringNameInput, volume, GDI_InfoData, &info, TAG_DONE)) {
        uint64 total_bytes = (uint64)info.id_NumBlocks * info.id_BytesPerBlock;
        uint64 free_bytes = (uint64)(info.id_NumBlocks - info.id_NumBlocksUsed) * info.id_BytesPerBlock;

        FormatByteSize(total_bytes); /* Returns static buffer, careful if called twice in printf */
        snprintf(size_buf, sizeof(size_buf), "%s", FormatByteSize(total_bytes));
        snprintf(free_buf, sizeof(free_buf), "%s", FormatByteSize(free_bytes));

        /* Get Filesystem Name */
        char fs_name[32];
        GetFileSystemInfo(volume, fs_name, sizeof(fs_name));
        snprintf(fs_buf, sizeof(fs_buf), "%s", fs_name);

        /* Get Device Name */
        char device_name[32];
        uint32 unit = 0;
        if (GetDeviceFromVolume(volume, device_name, sizeof(device_name), &unit)) {
            snprintf(dev_buf, sizeof(dev_buf), "%s:%u", device_name, (unsigned int)unit);
        }
    }

    IIntuition->SetGadgetAttrs((struct Gadget *)ui.vol_size_label, ui.window, NULL, GA_Text, (uint32)size_buf,
                               TAG_DONE);
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.vol_free_label, ui.window, NULL, GA_Text, (uint32)free_buf,
                               TAG_DONE);
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.vol_fs_label, ui.window, NULL, GA_Text, (uint32)fs_buf, TAG_DONE);
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.vol_device_label, ui.window, NULL, GA_Text, (uint32)dev_buf,
                               TAG_DONE);
}

/**
 * @brief Handles asynchronous reply messages from the benchmark worker process.
 *
 * Processes status updates (progress text) and completion messages (final results).
 * Updates the GUI status light, progress bar (if any), and populates the result list.
 *
 * @param m Pointer to the message received from the worker's reply port.
 */
void HandleWorkerReply(struct Message *m)
{
    LOG_DEBUG("GUI: Worker Msg received at %p. Type=%d", m, m->mn_Node.ln_Type);
    if (m->mn_Node.ln_Type == NT_REPLYMSG || m->mn_Node.ln_Type == NT_MESSAGE) {
        BenchStatus *st = (BenchStatus *)m;
        if (st->msg_type == MSG_TYPE_STATUS) {
            /* Handle intermediate progress updates */
            if (!st->finished) {
                /* Update status text with progress information */
                if (ui.status_light_obj && st->status_text[0] != '\0') {
                    IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                               (uint32)st->status_text, TAG_DONE);
                    LOG_DEBUG("GUI: Progress update - %s", st->status_text);
                }
                /* Free intermediate status message */
                IExec->FreeVec(st);
                return;
            }

            /* Handle final completion */
            if (st->finished) {
                /* Job finished */
                ui.completed_jobs++;

                /* Update Fuel Gauge */
                if (ui.fuel_gauge && ui.total_jobs > 0) {
                    uint32 percent = (ui.completed_jobs * 100) / ui.total_jobs;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)ui.completed_jobs,
                             (unsigned long)ui.total_jobs);
                    IIntuition->SetGadgetAttrs((struct Gadget *)ui.fuel_gauge, ui.window, NULL, FUELGAUGE_Level,
                                               percent, FUELGAUGE_VarArgs, (uint32)buf, TAG_DONE);
                }

                /* Try to dispatch next one */
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
                    // IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
                    IIntuition->RefreshGList((struct Gadget *)ui.run_button, ui.window, NULL, 1);

                    /* Reset Traffic Light to Green */
                    if (ui.traffic_light) {
                        IIntuition->RefreshGList((struct Gadget *)ui.traffic_light, ui.window, NULL, 1);
                    }

                    /* Reset Progress Counters */
                    ui.total_jobs = 0;
                    ui.completed_jobs = 0;
                }
                /* If queue is NOT empty, we are still busy processing the NextJob dispatched above.
                   DispatchNextJob sets ui.worker_busy = TRUE if it successfully dispatches a job.
                   We re-assert the busy state here to ensure the UI remains locked until the queue is fully drained. */
                if (!IsQueueEmpty()) {
                    ui.worker_busy = TRUE;
                    /* Traffic light remains Red */
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
                        6, LBNA_Column, BCOL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.timestamp,
                        LBNA_Column, BCOL_VOL, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)st->result.volume_name,
                        LBNA_Column, BCOL_TEST, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)tn, LBNA_Column, BCOL_MBPS,
                        LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ms, LBNA_Column, BCOL_DIFF, LBNCA_CopyText, TRUE,
                        LBNCA_Text, (uint32)ds, LBNA_Column, BCOL_VER, LBNCA_CopyText, TRUE, LBNCA_Text,
                        (uint32)st->result.app_version, LBNA_UserData, (uint32)res, TAG_DONE);
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

/**
 * @brief Handles Workbench-specific messages (AppLib).
 *
 * Responds to Quit, Hide, Unhide, and OpenPrefs messages sent by the system
 * or application library.
 *
 * @param amsg Pointer to the ApplicationMsg.
 * @param running Pointer to the main loop's running flag.
 */
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

/**
 * @brief Main event handler for the application window.
 *
 * Processes gadget up/down events, menu selections, and window close events.
 * Dispatches actions to appropriate helper functions.
 *
 * @param result The result from RA_HandleInput or WM_HANDLEINPUT.
 * @param code The code field from the IntuiMessage.
 * @param running Pointer to the main loop's running flag.
 */
void HandleGUIEvent(uint32 result, uint16 code, BOOL *running)
{
    uint32 gid = result & WMHI_GADGETMASK;
    switch (result & WMHI_CLASSMASK) {
    case WMHI_CLOSEWINDOW:
        *running = FALSE;
        break;
    case WMHI_GADGETUP:
        switch (gid) {
        case GID_VOL_CHOOSER: {
            struct Node *vn = NULL;
            IIntuition->GetAttr(CHOOSER_SelectedNode, ui.target_chooser, (uint32 *)&vn);
            if (vn) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name) {
                    UpdateVolumeInfo(dd->bare_name);
                }
            }
            break;
        }
        case GID_TABS: {
            uint32 t = 0;
            IIntuition->GetAttr(CLICKTAB_Current, ui.tabs, &t);

            /* Auto-Refresh History when switching to Visualization Tab (Index 2) */
            if (t == 2UL) {
                RefreshHistory();
                /* NOTE: RefreshHistory() usually triggers a UI update, but since we are in
                   HandleEvents, the Visualization update is triggered separately by
                   HandleVizEvents() or explicitly in some paths.
                   RefreshHistory() updates the ListBrowser nodes which CollectFilteredResults() reads. */
            }

            /* Update Page visibility */
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.page_obj, ui.window, NULL, PAGE_Current, t, TAG_DONE);

            /* Handle Health Tab Switch (Index 3) */
            if (t == 3UL) {
                struct Node *vn = NULL;
                IIntuition->GetAttr(CHOOSER_SelectedNode, ui.health_target_chooser, (uint32 *)&vn);
                if (vn) {
                    DriveNodeData *dd = NULL;
                    IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                    if (dd && dd->bare_name) {
                        UpdateHealthUI(dd->bare_name);
                    }
                }
            }

            /* Refresh Bulk Tab Info when switching tabs (Tab 4 is Bulk) */
            if (t == 4UL) {
                UpdateBulkTabInfo();
            }
            break;
        }
        case GID_TEST_CHOOSER:
            IIntuition->GetAttr(CHOOSER_Selected, ui.test_chooser, &ui.current_test_type);
            LOG_DEBUG("GUI: Test Type changed to %u", ui.current_test_type);

            /* Disable Block Size chooser for Fixed-Behavior tests */
            BOOL disable_blocks = (ui.current_test_type == TEST_DAILY_GRIND || ui.current_test_type == TEST_PROFILER);

            SetGadgetState(GID_BLOCK_SIZE, disable_blocks);

            UpdateBulkTabInfo();
            break;
        case GID_NUM_PASSES:
            IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &ui.current_passes);
            LOG_DEBUG("GUI: Passes changed to %u", ui.current_passes);
            UpdateBulkTabInfo();
            break;
        /* Visualization Tab Filters */
        case GID_VIZ_FILTER_VOLUME:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_volume, &ui.viz_filter_volume_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_FILTER_TEST:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_test, &ui.viz_filter_test_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_FILTER_METRIC: /* Reused as Date Range Filter */
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_metric, &ui.viz_date_range_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_FILTER_VERSION:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_filter_version, &ui.viz_filter_version_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_CHART_TYPE:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_chart_type, &ui.viz_chart_type_idx);
            UpdateVisualization();
            break;
        case GID_VIZ_COLOR_BY:
            IIntuition->GetAttr(CHOOSER_Selected, ui.viz_color_by, &ui.viz_color_by_idx);
            UpdateVisualization();
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
        case GID_HEALTH_REFRESH: {
            struct Node *vn = NULL;
            IIntuition->GetAttr(CHOOSER_SelectedNode, ui.health_target_chooser, (uint32 *)&vn);
            if (vn) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name) {
                    UpdateHealthUI(dd->bare_name);
                }
            }
            break;
        }
        case GID_HEALTH_DRIVE: {
            struct Node *vn = NULL;
            IIntuition->GetAttr(CHOOSER_SelectedNode, ui.health_target_chooser, (uint32 *)&vn);
            if (vn) {
                DriveNodeData *dd = NULL;
                IChooser->GetChooserNodeAttrs(vn, CNA_UserData, &dd, TAG_DONE);
                if (dd && dd->bare_name) {
                    UpdateHealthUI(dd->bare_name);
                }
            }
            break;
        }
        case GID_BULK_ALL_TESTS:
        case GID_BULK_ALL_BLOCKS:
            UpdateBulkTabInfo();
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
        case GID_HISTORY_DELETE:
            if (ShowConfirm("Delete Benchmark History", "Are you sure you want to delete\nthe selected history items?",
                            "Delete|Cancel")) {
                DeleteSelectedHistoryItems();
            }
            break;
        case GID_HISTORY_CLEAR_ALL:
            if (ShowConfirm("Clear All History",
                            "Are you sure you want to delete\nALL history items?\nThis cannot be undone.",
                            "Clear All|Cancel")) {
                ClearHistory();
            }
            break;
        case GID_HISTORY_EXPORT: {
            if (ui.IAsl) {
                char default_filename[64];
                time_t rawtime;
                struct tm *timeinfo;

                time(&rawtime);
                timeinfo = localtime(&rawtime);

                /* Format: bench_history_YYYY-MM-DD-HH-MI-SS.csv */
                strftime(default_filename, sizeof(default_filename), "bench_history_%Y-%m-%d-%H-%M-%S.csv", timeinfo);

                struct FileRequester *req = ui.IAsl->AllocAslRequestTags(
                    ASL_FileRequest, ASLFR_TitleText, (uint32) "Export History to CSV", ASLFR_DoSaveMode, TRUE,
                    ASLFR_InitialFile, (uint32)default_filename, TAG_DONE);
                if (req) {
                    if (ui.IAsl->AslRequest(req, NULL)) {
                        char filepath[512];
                        snprintf(filepath, sizeof(filepath), "%s%s", req->fr_Drawer, req->fr_File);
                        ExportHistoryToCSV(filepath);
                        ShowMessage("Export Successful", "History has been exported to CSV.", "OK");
                    }
                    ui.IAsl->FreeAslRequest(req);
                }
            } else {
                ShowMessage("Error", "Could not open asl.library", "OK");
            }
            break;
        }
        case GID_HISTORY_LIST:
        case GID_CURRENT_RESULTS: {
            uint32 re = 0;
            Object *lb = (gid == GID_HISTORY_LIST) ? ui.history_list : ui.bench_list;
            IIntuition->GetAttr(LISTBROWSER_RelEvent, lb, &re);

            /* Update Compare Button state if History list selected */
            if (gid == GID_HISTORY_LIST) {
                uint32 selected_count = 0;
                struct Node *n;
                for (n = IExec->GetHead(&ui.history_labels); n; n = IExec->GetSucc(n)) {
                    uint32 is_checked = 0;
                    IListBrowser->GetListBrowserNodeAttrs(n, LBNA_Checked, &is_checked, TAG_DONE);
                    if (is_checked) {
                        selected_count++;
                    }
                }

                SetGadgetState(GID_HISTORY_COMPARE, (selected_count != 2)); // Disabled if not exactly 2
            }

            if (re & LBRE_DOUBLECLICK) {
                ShowBenchmarkDetails(lb);
            }
            break;
        }
        case GID_HISTORY_COMPARE: {
            BenchResult *res1 = NULL;
            BenchResult *res2 = NULL;

            struct Node *n;
            for (n = IExec->GetHead(&ui.history_labels); n; n = IExec->GetSucc(n)) {
                uint32 is_checked = 0;
                IListBrowser->GetListBrowserNodeAttrs(n, LBNA_Checked, &is_checked, TAG_DONE);
                if (is_checked) {
                    BenchResult *res = NULL;
                    IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &res, TAG_DONE);
                    if (res) {
                        if (!res1)
                            res1 = res;
                        else if (!res2)
                            res2 = res;
                    }
                }
            }

            if (res1 && res2) {
                OpenCompareWindow(res1, res2);
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

/**
 * @brief Event handler for the Preferences window.
 *
 * Handles Save, Cancel, and CSV Browse events for the settings dialog.
 *
 * @param result The result from RA_HandleInput.
 * @param code The code field from the IntuiMessage.
 */
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

/**
 * @brief Event handler for the Comparison window.
 *
 * Handles the "Close" button and window close gadget.
 * Automatically deselects history items when closed.
 *
 * @param code The code field from the IntuiMessage.
 * @param result The result from RA_HandleInput.
 */
void HandleCompareWindowEvent(uint16 code, uint32 result)
{
    uint32 gid = result & WMHI_GADGETMASK;
    switch (result & WMHI_CLASSMASK) {
    case WMHI_CLOSEWINDOW:
        CloseCompareWindow();
        DeselectAllHistoryItems();
        break;
    case WMHI_GADGETUP:
        if (gid == GID_COMPARE_CLOSE) {
            CloseCompareWindow();
            DeselectAllHistoryItems();
        }
        break;
    }
}
