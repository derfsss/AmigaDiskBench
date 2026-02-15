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
                /* Create a new ListBrowser node with 4 columns to match ColumnInfo
                 * Col 0: Checkbox
                 * Col 1: Volume Name
                 * Col 2: FileSystem (Retrieve fresh info)
                 * Col 3: Spacer
                 */
                char fs_info[64] = "Unknown";
                GetFileSystemName(ddata->bare_name, fs_info, sizeof(fs_info));

                struct Node *bn = IListBrowser->AllocListBrowserNode(
                    4, LBNA_CheckBox, TRUE, LBNA_Column, 0, LBNCA_Text, (uint32) "", LBNA_Column, 1, LBNCA_CopyText,
                    TRUE, LBNCA_Text, (uint32)(ddata->bare_name ? ddata->bare_name : "Unknown"), LBNA_Column, 2,
                    LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)fs_info, LBNA_UserData, (uint32)ddata, TAG_DONE);

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
    uint32 job_count = 0;

    /* 1. Gather Settings */
    /* Check "Run All" flags */
    uint32 run_all_tests = 0;
    uint32 run_all_blocks = 0;
    if (ui.bulk_all_tests_check)
        IIntuition->GetAttr(CHECKBOX_Checked, ui.bulk_all_tests_check, &run_all_tests);
    if (ui.bulk_all_blocks_check)
        IIntuition->GetAttr(CHECKBOX_Checked, ui.bulk_all_blocks_check, &run_all_blocks);

    /* Define Test Types to run */
    uint32 tests[8];
    int num_tests = 0;
    if (run_all_tests) {
        tests[num_tests++] = TEST_SPRINTER;
        tests[num_tests++] = TEST_HEAVY_LIFTER;
        tests[num_tests++] = TEST_LEGACY;
        tests[num_tests++] = TEST_DAILY_GRIND;
        tests[num_tests++] = TEST_SEQUENTIAL;
        tests[num_tests++] = TEST_RANDOM_4K;
        tests[num_tests++] = TEST_PROFILER;
    } else {
        tests[num_tests++] = ui.current_test_type;
    }

    /* Define Block Sizes to run */
    uint32 blocks[8];
    int num_blocks = 0;
    if (run_all_blocks) {
        blocks[num_blocks++] = 4096;
        blocks[num_blocks++] = 16384;
        blocks[num_blocks++] = 32768;
        blocks[num_blocks++] = 65536;
        blocks[num_blocks++] = 131072;
        blocks[num_blocks++] = 262144;
        blocks[num_blocks++] = 1048576;
    } else {
        blocks[num_blocks++] = ui.current_block_size;
    }

    /* 2. Queue Jobs */
    struct Node *node = IExec->GetHead(&ui.bulk_labels);
    while (node) {
        uint32 checked = 0;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_Checked, &checked, TAG_DONE);
        if (checked) {
            DriveNodeData *ddata = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &ddata, TAG_DONE);
            if (ddata && ddata->bare_name) {

                /* Nested Loops for Permutations: Tests -> Blocks */
                for (int t = 0; t < num_tests; t++) {
                    for (int b = 0; b < num_blocks; b++) {

                        BenchJob *job = IExec->AllocVecTags(sizeof(BenchJob), AVT_Type, MEMF_SHARED, AVT_ClearWithValue,
                                                            0, TAG_DONE);
                        if (job) {
                            job->msg_type = MSG_TYPE_JOB;
                            job->type = (BenchTestType)tests[t];
                            snprintf(job->target_path, sizeof(job->target_path), "%s", ddata->bare_name);
                            job->target_path[sizeof(job->target_path) - 1] = '\0';
                            job->num_passes = ui.current_passes;
                            job->block_size = blocks[b];
                            job->use_trimmed_mean = ui.use_trimmed_mean;
                            job->flush_cache = ui.flush_cache;
                            job->msg.mn_ReplyPort = ui.worker_reply_port;

                            LOG_DEBUG("Bulk: Queueing job for %s (Test=%d, BS=%u)", ddata->bare_name, tests[t],
                                      blocks[b]);
                            EnqueueBenchmarkJob(job);
                            job_count++;
                        } else {
                            LOG_DEBUG("Bulk: Memory allocation failed for job %s", ddata->bare_name);
                        }
                    }
                }
            }
        }
        node = IExec->GetSucc(node);
    }

    if (job_count == 0) {
        LOG_DEBUG("Bulk: No volumes selected for benchmarking.");
        ShowMessage("AmigaDiskBench", "Please select at least one volume\nin the bulk list.", "OK");
    }
}
