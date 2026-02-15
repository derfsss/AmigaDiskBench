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
#include <stdlib.h>

/* Forward declaration */
static void SaveHistoryToCSV(const char *filename);

void RefreshHistory(void)
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
    /* Open CSV and parse */
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
                bytes_str[32], vendor[32], product[64], firmware[32], serial[32];

            /* Clear strings */
            id[0] = timestamp[0] = type[0] = disk[0] = fs[0] = mbs_str[0] = iops_str[0] = device[0] = unit_str[0] = 0;
            ver[0] = passes[0] = bs_str[0] = trimmed[0] = min_str[0] = max_str[0] = dur_str[0] = bytes_str[0] = 0;
            vendor[0] = product[0] = firmware[0] = serial[0] = 0;

            /* Parse CSV based on column count */
            int fields = sscanf(line,
                                "%31[^,],%31[^,],%63[^,],%63[^,],%127[^,],%31[^,],%31[^,],%63[^,],%31[^,],%31[^,],%15[^"
                                ",],%31[^,],%15[^,],%31[^,],%31[^,],%31[^,],%31[^,],%31[^,],%63[^,],%31[^,],%31s",
                                id, timestamp, type, disk, fs, mbs_str, iops_str, device, unit_str, ver, passes, bs_str,
                                trimmed, min_str, max_str, dur_str, bytes_str, vendor, product, firmware, serial);

            /* Shift data if first field is not a unique ID (legacy format) */
            /* Note: Legacy shifting logic remains but might need adjustment if we see old files.
               For now, assuming new writes will have ID. */
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
                snprintf(type, sizeof(timestamp), "%s", id);
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
                snprintf(res->firmware_rev, sizeof(res->firmware_rev), "%s", (fields >= 20) ? firmware : "N/A");
                snprintf(res->serial_number, sizeof(res->serial_number), "%s", (fields >= 21) ? serial : "N/A");

                res->type = StringToTestType(type);

                /* Calculate Comparison with previous results in the history list */
                BenchResult prev;
                if (FindMatchInList(&ui.history_labels, res, &prev, FALSE)) {
                    res->prev_mbps = prev.mb_per_sec;
                    res->prev_iops = prev.iops;
                    snprintf(res->prev_timestamp, sizeof(res->prev_timestamp), "%s", prev.timestamp);
                    if (prev.mb_per_sec > 0) {
                        res->diff_per = ((res->mb_per_sec - prev.mb_per_sec) / prev.mb_per_sec) * 100.0f;
                    }
                }
            }

            char diff_str[32];
            if (res && res->prev_mbps > 0) {
                snprintf(diff_str, sizeof(diff_str), "%+.1f%%", res->diff_per);
            } else {
                snprintf(diff_str, sizeof(diff_str), "N/A");
            }

            struct Node *hnode = IListBrowser->AllocListBrowserNode(
                13, /* Increased column count for checkbox */
                LBNA_Column, COL_CHECK, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32) "", LBNA_CheckBox, TRUE, LBNA_Column,
                COL_DATE, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)timestamp, LBNA_Column, COL_VOL, LBNCA_CopyText,
                TRUE, LBNCA_Text, (uint32)disk, LBNA_Column, COL_TEST, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)type,
                LBNA_Column, COL_BS, LBNCA_CopyText, TRUE, LBNCA_Text,
                (uint32)FormatPresetBlockSize(strtoul(bs_str, NULL, 10)), LBNA_Column, COL_PASSES, LBNCA_CopyText, TRUE,
                LBNCA_Text, (uint32)passes, LBNA_Column, COL_MBPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)mbs_str,
                LBNA_Column, COL_IOPS, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)iops_str, LBNA_Column, COL_DEVICE,
                LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)device, LBNA_Column, COL_UNIT, LBNCA_CopyText, TRUE,
                LBNCA_Text, (uint32)unit_str, LBNA_Column, COL_VER, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)ver,
                LBNA_Column, COL_DIFF, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)diff_str, LBNA_Column, COL_DUMMY,
                LBNCA_CopyText, TRUE, LBNCA_Text, (uint32) "", LBNA_UserData, (uint32)res, TAG_DONE);
            if (hnode) {
                /* Add HEAD to make newest appear at the top */
                IExec->AddHead(&ui.history_labels, hnode);
                count++;
            } else if (res) {
                IExec->FreeVec(res);
            }
        }
        IDOS->FClose(file);
        LOG_DEBUG("RefreshHistory: Loaded %d records", count);
    } else {
        /* Create empty history file with full header if it doesn't exist */
        file = IDOS->FOpen(ui.csv_path, MODE_NEWFILE, 0);
        if (file) {
            IDOS->FPuts(file, "ID,DateTime,Type,Volume,FS,MB/"
                              "s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize,Trimmed,Min,Max,Duration,TotalBytes,"
                              "Vendor,Product,Firmware,Serial\n");
            IDOS->FClose(file);
            LOG_DEBUG("RefreshHistory: Created new CSV at '%s'", ui.csv_path);
        } else {
            LOG_DEBUG("RefreshHistory: Could not open/create CSV at '%s'", ui.csv_path);
        }
    }
    /* Reattach list and refresh UI */
    if (ui.window && ui.history_list) {
        LOG_DEBUG("RefreshHistory: Reattaching list to %p", ui.history_list);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels,
                                   &ui.history_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
        IIntuition->RefreshGList((struct Gadget *)ui.history_list, ui.window, NULL, 1);
        RefreshVizVolumeFilter();
        UpdateVisualization();
    } else {
        LOG_DEBUG("RefreshHistory: Skiping UI update, win=%p, list=%p", ui.window, ui.history_list);
    }
}

BOOL FindMatchInList(struct List *list, BenchResult *current, BenchResult *out_prev, BOOL reverse)
{
    struct Node *node;
    if (reverse) {
        node = IExec->GetTail(list);
    } else {
        node = IExec->GetHead(list);
    }

    while (node) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);

        if (res && res != current) {
            /* Match criteria: volume name, test type, block size, device name, and unit */
            if (res->type == current->type && res->block_size == current->block_size &&
                res->device_unit == current->device_unit && strcmp(res->volume_name, current->volume_name) == 0 &&
                strcmp(res->device_name, current->device_name) == 0) {
                if (out_prev) {
                    memcpy(out_prev, res, sizeof(BenchResult));
                }
                return TRUE;
            }
        }

        if (reverse) {
            node = IExec->GetPred(node);
        } else {
            node = IExec->GetSucc(node);
        }
    }
    return FALSE;
}

BOOL FindMatchingResult(BenchResult *current, BenchResult *out_prev)
{
    /* Search bench_labels first (reverse: newest at tail via AddTail) */
    if (FindMatchInList(&ui.bench_labels, current, out_prev, TRUE)) {
        return TRUE;
    }
    /* Fall back to history_labels (forward: newest at head via AddHead) */
    return FindMatchInList(&ui.history_labels, current, out_prev, FALSE);
}

static void SaveHistoryToCSV(const char *filename)
{
    LOG_DEBUG("SaveHistoryToCSV: Opening '%s'...", filename);
    BPTR file = IDOS->FOpen(filename, MODE_NEWFILE, 0);
    if (!file) {
        LOG_DEBUG("SaveHistoryToCSV: Failed to open file!");
        return;
    }

    IDOS->FPuts(file, "ID,DateTime,Type,Volume,FS,MB/s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize,Trimmed,Min,Max,"
                      "Duration,TotalBytes,Vendor,Product,Firmware,Serial\n");

    /* History list is Newest-First (Head->Tail).
       CSV should be Oldest-First (Append).
       So iterate from Tail to Head. */
    if (IExec->GetHead(&ui.history_labels)) {
        struct Node *node = IExec->GetTail(&ui.history_labels);

        while (node->ln_Pred) /* Stop before header */
        {
            BenchResult *result = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &result, TAG_DONE);

            if (result) {
                /* Map test type enum to CSV string via centralised lookup */
                const char *typeName = TestTypeToString(result->type);

                char line[1024];
                snprintf(line, sizeof(line),
                         "%s,%s,%s,%s,%s,%.2f,%u,%s,%u,%s,%u,%u,%d,%.2f,%.2f,%.2f,%llu,%s,%s,%s,%s\n",
                         result->result_id, result->timestamp, typeName, result->volume_name, result->fs_type,
                         result->mb_per_sec, (unsigned int)result->iops, result->device_name,
                         (unsigned int)result->device_unit, result->app_version, (unsigned int)result->passes,
                         (unsigned int)result->block_size, (int)result->use_trimmed_mean, result->min_mbps,
                         result->max_mbps, result->total_duration, (unsigned long long)result->cumulative_bytes,
                         result->vendor, result->product, result->firmware_rev, result->serial_number);
                IDOS->FPuts(file, line);
            }
            node = node->ln_Pred;
        }
    }

    IDOS->FClose(file);
}

void DeleteSelectedHistoryItems(void)
{
    BOOL removed_any = FALSE;
    struct Node *node = IExec->GetHead(&ui.history_labels);
    struct Node *next;
    uint32 count = 0;

    /* Detach list */
    if (ui.window && ui.history_list) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }

    while (node) {
        next = IExec->GetSucc(node);
        uint32 is_checked = 0;
        uint32 is_selected = 0;

        /* Check both Checkbox and Selection to support both interaction models */
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_Checked, &is_checked, LBNA_Selected, &is_selected, TAG_DONE);

        if (is_checked || is_selected) {
            void *udata = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &udata, TAG_DONE);
            IExec->Remove(node);
            /* Node is now detached, can free */
            if (udata)
                IExec->FreeVec(udata);
            IListBrowser->FreeListBrowserNode(node);
            removed_any = TRUE;
            count++;
        }
        node = next;
    }

    if (removed_any) {
        /* Rewrite CSV */
        SaveHistoryToCSV(ui.csv_path);
        RefreshVizVolumeFilter();
        UpdateVisualization();
    }

    /* Reattach list */
    if (ui.window && ui.history_list) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels,
                                   &ui.history_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
        IIntuition->RefreshGList((struct Gadget *)ui.history_list, ui.window, NULL, 1);
    }
}

void ClearBenchmarkList(void)
{
    if (ui.bench_list) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }

    struct Node *node = IExec->GetHead(&ui.bench_labels);
    struct Node *next;
    while (node) {
        next = IExec->GetSucc(node);
        void *udata = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &udata, TAG_DONE);
        IExec->Remove(node);
        if (udata)
            IExec->FreeVec(udata);
        IListBrowser->FreeListBrowserNode(node);
        node = next;
    }

    if (ui.bench_list) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bench_list, ui.window, NULL, LISTBROWSER_Labels,
                                   &ui.bench_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
        IIntuition->RefreshGList((struct Gadget *)ui.bench_list, ui.window, NULL, 1);
    }
}

void ClearHistory(void)
{
    /* 1. Clear History List */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                               TAG_DONE);

    struct Node *node = IExec->GetHead(&ui.history_labels);
    struct Node *next;
    while (node) {
        next = IExec->GetSucc(node);
        void *udata = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &udata, TAG_DONE);
        IExec->Remove(node);
        if (udata)
            IExec->FreeVec(udata);
        IListBrowser->FreeListBrowserNode(node);
        node = next;
    }

    /* 2. Clear Benchmark List (Current Session) */
    ClearBenchmarkList();

    /* 3. Overwrite CSV with empty header */
    SaveHistoryToCSV(ui.csv_path);

    /* 4. Update Visualization (now truly empty) */
    RefreshVizVolumeFilter();
    UpdateVisualization();

    /* 5. Reattach lists (empty) */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels,
                               &ui.history_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
    IIntuition->RefreshGList((struct Gadget *)ui.history_list, ui.window, NULL, 1);
}

void ExportHistoryToCSV(const char *filename)
{
    SaveHistoryToCSV(filename);
}

void DeselectAllHistoryItems(void)
{
    if (!ui.history_list || !ui.window)
        return;

    /* Detach list for speed */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                               TAG_DONE);

    struct Node *node;
    for (node = IExec->GetHead(&ui.history_labels); node; node = IExec->GetSucc(node)) {
        IListBrowser->SetListBrowserNodeAttrs(node, LBNA_Checked, FALSE, TAG_DONE);
    }

    /* Reattach and refresh */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.history_list, ui.window, NULL, LISTBROWSER_Labels,
                               (uint32)&ui.history_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
    IIntuition->RefreshGList((struct Gadget *)ui.history_list, ui.window, NULL, 1);

    /* Update Compare Button State - Explicitly disable it as count is now 0 */
    SetGadgetState(GID_HISTORY_COMPARE, TRUE);
}
