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
#include <stdlib.h>
#include <string.h>

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
        IDOS->FClose(file);
        LOG_DEBUG("RefreshHistory: Loaded %d records", count);
    } else {
        /* Create empty history file with full header if it doesn't exist */
        file = IDOS->FOpen(ui.csv_path, MODE_NEWFILE, 0);
        if (file) {
            IDOS->FPuts(file, "ID,DateTime,Type,Volume,FS,MB/"
                              "s,IOPS,Hardware,Unit,AppVersion,Passes,BlockSize,Trimmed,Min,Max,Duration,TotalBytes,"
                              "Vendor,Product\n");
            IDOS->FClose(file);
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
