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
#include <interfaces/intuition.h>
#include <interfaces/listbrowser.h>
#include <stdint.h>
#include <string.h>

/**
 * RefreshBulkList
 *
 * Synchronizes the bulk volume selection list with the main drive list.
 * Clears existing nodes and re-allocates new ones with checkboxes.
 */
void RefreshBulkList(void)
{
    struct Node *node, *next;

    LOG_DEBUG("Refreshing Bulk List...");

    /* 1. Clear existing list from gadget to avoid refresh issues */
    if (ui.bulk_list && ui.window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bulk_list, ui.window, NULL, LISTBROWSER_Labels, (ULONG)-1,
                                   TAG_DONE);
    }

    /* Free all existing nodes in bulk_labels */
    node = IExec->GetHead(&ui.bulk_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IExec->Remove(node);
        IListBrowser->FreeListBrowserNode(node);
        node = next;
    }

    /* 2. Populate from drive_list */
    if (IsListEmpty(&ui.drive_list)) {
        LOG_DEBUG("Bulk: Main drive list is empty, nothing to add.");
    } else {
        node = IExec->GetHead(&ui.drive_list);
        while (node) {
            DriveNodeData *ddata = NULL;
            IChooser->GetChooserNodeAttrs(node, CNA_UserData, &ddata, TAG_DONE);

            if (ddata) {
                /* Create a new ListBrowser node with a checkbox for bulk selection.
                 * We show Volume name and Auto FS for now.
                 */
                struct Node *bn = IListBrowser->AllocListBrowserNode(
                    2, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text,
                    (uint32)(ddata->bare_name ? ddata->bare_name : "Unknown"), LBNA_Column, 1, LBNCA_CopyText, TRUE,
                    LBNCA_Text, (uint32) "(Auto)", LBNA_CheckBox, TRUE, LBNA_Checked, FALSE, LBNA_UserData,
                    (uint32)ddata, TAG_DONE);

                if (bn) {
                    IExec->AddTail(&ui.bulk_labels, bn);
                }
            }
            node = IExec->GetSucc(node);
        }
    }

    /* 3. Re-attach the list to the gadget if the window is open */
    if (ui.bulk_list && ui.window) {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.bulk_list, ui.window, NULL, LISTBROWSER_Labels,
                                   (uint32)&ui.bulk_labels, LISTBROWSER_AutoFit, TRUE, TAG_DONE);
    }
}

/**
 * LaunchBulkJobs
 *
 * Iterates through the bulk list and dispatches a benchmark job to the worker
 * process for every volume that has been checked by the user.
 */
void LaunchBulkJobs(void)
{
    uint32 test_type_idx = TEST_SPRINTER;
    uint32 passes = 3;
    uint32 block_val = 4096;
    struct Node *node;
    int job_count = 0;

    /* Safety check: ensure worker port is valid before queueing */
    if (!ui.worker_port) {
        LOG_DEBUG("Bulk: Worker port not available, cannot run jobs.");
        return;
    }

    /* Get common settings from the main tab gadgets */
    if (ui.test_chooser) {
        IIntuition->GetAttr(CHOOSER_Selected, ui.test_chooser, &test_type_idx);
    }
    if (ui.pass_gad) {
        IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &passes);
    }
    if (ui.block_chooser) {
        struct Node *bnode = NULL;
        IIntuition->GetAttr(CHOOSER_SelectedNode, ui.block_chooser, (uint32 *)&bnode);
        if (bnode) {
            void *ud = NULL;
            IChooser->GetChooserNodeAttrs(bnode, CNA_UserData, &ud, TAG_DONE);
            if (ud) {
                /* Block size index is stored as a pointer-to-int in UserData */
                block_val = (uint32)((uintptr_t)ud);
            }
        }
    }

    /* Iterate bulk list and launch jobs for checked items */
    node = IExec->GetHead(&ui.bulk_labels);
    while (node) {
        uint32 checked = FALSE;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_Checked, &checked, TAG_DONE);

        if (checked) {
            DriveNodeData *ddata = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &ddata, TAG_DONE);

            if (ddata && ddata->bare_name) {
                /* Allocate a BenchJob message for the worker */
                BenchJob *job =
                    IExec->AllocVecTags(sizeof(BenchJob), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
                if (job) {
                    job->msg_type = MSG_TYPE_JOB;
                    job->type = (BenchTestType)test_type_idx;
                    strncpy(job->target_path, ddata->bare_name, sizeof(job->target_path) - 1);
                    job->num_passes = passes;
                    job->block_size = block_val;
                    job->use_trimmed_mean = ui.use_trimmed_mean;
                    job->flush_cache = ui.flush_cache;
                    job->msg.mn_ReplyPort = ui.worker_reply_port;

                    LOG_DEBUG("Bulk: Queueing job for %s", ddata->bare_name);
                    ui.jobs_pending++;
                    IExec->PutMsg(ui.worker_port, &job->msg);
                    job_count++;
                } else {
                    LOG_DEBUG("Bulk: Memory allocation failed for job %s", ddata->bare_name);
                }
            }
        }
        node = IExec->GetSucc(node);
    }

    /* If we launched jobs, disable the run button and set status to [ BUSY ] */
    if (job_count > 0) {
        if (ui.window) {
            SetGadgetState(GID_RUN_ALL, TRUE);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                       (uint32) "[ BUSY ]", TAG_DONE);
        }
    } else {
        LOG_DEBUG("Bulk: No volumes selected for benchmarking.");
        ShowMessage("AmigaDiskBench", "Please select at least one volume\nin the bulk list.", "OK");
    }
}
