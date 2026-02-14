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

#define GID_COMPARE_CLOSE 5000

/* Column definitions for comparison list */
static struct ColumnInfo compare_cols[] = {
    {150, "Metric", 0},
    {150, "Result 1", 0},
    {150, "Result 2", 0},
    {100, "Difference", 0},
    {-1, (STRPTR)~0, -1}
};

void OpenCompareWindow(BenchResult *result1, BenchResult *result2)
{
    if (!result1 || !result2) {
        LOG_DEBUG("OpenCompareWindow: NULL result pointer(s)");
        return;
    }

    if (ui.compare_window) {
        LOG_DEBUG("OpenCompareWindow: Window already open");
        IIntuition->WindowToFront(ui.compare_window);
        IIntuition->ActivateWindow(ui.compare_window);
        return;
    }

    /* Create list for comparison data */
    struct List *compare_list = IExec->AllocVecTags(sizeof(struct List), AVT_Type, MEMF_SHARED,
                                                     AVT_ClearWithValue, 0, TAG_DONE);
    if (!compare_list) {
        LOG_DEBUG("OpenCompareWindow: Failed to allocate list");
        return;
    }
    IExec->NewList(compare_list);

    /* Helper macros for creating comparison rows */
    char val1[64], val2[64], diff[64];

    /* Add comparison rows */

    /* Timestamp */
    snprintf(val1, sizeof(val1), "%s", result1->timestamp);
    snprintf(val2, sizeof(val2), "%s", result2->timestamp);
    snprintf(diff, sizeof(diff), "N/A");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Timestamp",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Test Type */
    snprintf(val1, sizeof(val1), "%s", TestTypeToDisplayName(result1->type));
    snprintf(val2, sizeof(val2), "%s", TestTypeToDisplayName(result2->type));
    snprintf(diff, sizeof(diff), "%s", (result1->type == result2->type) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Test Type",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* MB/s */
    snprintf(val1, sizeof(val1), "%.2f MB/s", result1->mb_per_sec);
    snprintf(val2, sizeof(val2), "%.2f MB/s", result2->mb_per_sec);
    if (result1->mb_per_sec > 0) {
        float percent_diff = ((result2->mb_per_sec - result1->mb_per_sec) / result1->mb_per_sec) * 100.0f;
        snprintf(diff, sizeof(diff), "%+.1f%%", percent_diff);
    } else {
        snprintf(diff, sizeof(diff), "N/A");
    }
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Throughput (MB/s)",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* IOPS */
    snprintf(val1, sizeof(val1), "%u IOPS", result1->iops);
    snprintf(val2, sizeof(val2), "%u IOPS", result2->iops);
    if (result1->iops > 0) {
        float percent_diff = ((float)(result2->iops - result1->iops) / (float)result1->iops) * 100.0f;
        snprintf(diff, sizeof(diff), "%+.1f%%", percent_diff);
    } else {
        snprintf(diff, sizeof(diff), "N/A");
    }
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "IOPS",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Volume */
    snprintf(val1, sizeof(val1), "%s", result1->volume_name);
    snprintf(val2, sizeof(val2), "%s", result2->volume_name);
    snprintf(diff, sizeof(diff), "%s", (strcmp(result1->volume_name, result2->volume_name) == 0) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Volume",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Filesystem */
    snprintf(val1, sizeof(val1), "%s", result1->fs_type);
    snprintf(val2, sizeof(val2), "%s", result2->fs_type);
    snprintf(diff, sizeof(diff), "%s", (strcmp(result1->fs_type, result2->fs_type) == 0) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Filesystem",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Block Size */
    snprintf(val1, sizeof(val1), "%u bytes", result1->block_size);
    snprintf(val2, sizeof(val2), "%u bytes", result2->block_size);
    snprintf(diff, sizeof(diff), "%s", (result1->block_size == result2->block_size) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Block Size",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Passes */
    snprintf(val1, sizeof(val1), "%u", result1->passes);
    snprintf(val2, sizeof(val2), "%u", result2->passes);
    snprintf(diff, sizeof(diff), "%s", (result1->passes == result2->passes) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Passes",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Device */
    snprintf(val1, sizeof(val1), "%s:%u", result1->device_name, result1->device_unit);
    snprintf(val2, sizeof(val2), "%s:%u", result2->device_name, result2->device_unit);
    snprintf(diff, sizeof(diff), "%s", (strcmp(result1->device_name, result2->device_name) == 0 &&
                                         result1->device_unit == result2->device_unit) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Device",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Vendor/Product */
    snprintf(val1, sizeof(val1), "%s %s", result1->vendor, result1->product);
    snprintf(val2, sizeof(val2), "%s %s", result2->vendor, result2->product);
    snprintf(diff, sizeof(diff), "%s", (strcmp(result1->vendor, result2->vendor) == 0 &&
                                         strcmp(result1->product, result2->product) == 0) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Drive Model",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Firmware */
    snprintf(val1, sizeof(val1), "%s", result1->firmware_rev);
    snprintf(val2, sizeof(val2), "%s", result2->firmware_rev);
    snprintf(diff, sizeof(diff), "%s", (strcmp(result1->firmware_rev, result2->firmware_rev) == 0) ? "Same" : "Different");
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Firmware",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Duration */
    snprintf(val1, sizeof(val1), "%.2f sec", result1->duration_secs);
    snprintf(val2, sizeof(val2), "%.2f sec", result2->duration_secs);
    if (result1->duration_secs > 0) {
        float percent_diff = ((result2->duration_secs - result1->duration_secs) / result1->duration_secs) * 100.0f;
        snprintf(diff, sizeof(diff), "%+.1f%%", percent_diff);
    } else {
        snprintf(diff, sizeof(diff), "N/A");
    }
    IExec->AddTail(compare_list, IListBrowser->AllocListBrowserNode(
        4, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, "Duration",
        LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, val1,
        LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, val2,
        LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, diff,
        TAG_DONE));

    /* Create window */
    ui.compare_win_obj = WindowObject,
        WA_Title, "Benchmark Comparison",
        WA_Width, 700,
        WA_Height, 450,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_Layout, VLayoutObject,
            LAYOUT_SpaceOuter, TRUE,
            LAYOUT_AddChild, VLayoutObject,
                LAYOUT_Label, "Side-by-Side Comparison",
                LAYOUT_BevelStyle, BVS_GROUP,
                LAYOUT_AddChild, ListBrowserObject,
                    LISTBROWSER_ColumnInfo, compare_cols,
                    LISTBROWSER_ColumnTitles, TRUE,
                    LISTBROWSER_Labels, compare_list,
                    LISTBROWSER_AutoFit, TRUE,
                    LISTBROWSER_ShowSelected, FALSE,
                End,
                CHILD_WeightedHeight, 100,
            End,
            LAYOUT_AddChild, HLayoutObject,
                LAYOUT_AddChild, ButtonObject,
                    GA_ID, GID_COMPARE_CLOSE,
                    GA_Text, "Close",
                    GA_RelVerify, TRUE,
                End,
            End,
            CHILD_WeightedHeight, 0,
        End,
    End;

    if (ui.compare_win_obj) {
        ui.compare_window = (struct Window *)IIntuition->IDoMethod(ui.compare_win_obj, WM_OPEN, NULL);
        if (ui.compare_window) {
            uint32 win_sigmask = 0;
            IIntuition->GetAttr(WINDOW_SigMask, ui.compare_win_obj, &win_sigmask);
            ui.window_mask |= win_sigmask;
            LOG_DEBUG("OpenCompareWindow: Comparison window opened");
        } else {
            LOG_DEBUG("OpenCompareWindow: Failed to open window");
            IIntuition->DisposeObject(ui.compare_win_obj);
            ui.compare_win_obj = NULL;
        }
    }
}

void CloseCompareWindow(void)
{
    if (ui.compare_win_obj) {
        IIntuition->DisposeObject(ui.compare_win_obj);
        ui.compare_win_obj = NULL;
        ui.compare_window = NULL;
        LOG_DEBUG("CloseCompareWindow: Window closed");
    }
}
