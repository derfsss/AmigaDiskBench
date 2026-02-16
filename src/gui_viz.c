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
#include <proto/graphics.h>
#include <stdlib.h>

/* Maximum results to plot on the graph */
#define MAX_PLOT_RESULTS 200

/* Forward declaration of render function from gui_viz_render.c */
extern void RenderTrendGraph(struct RastPort *rp, struct IBox *box, BenchResult **results, uint32 count);

/* Comparison for sorting results by timestamp (ascending = oldest first) */
static int compare_by_timestamp(const void *a, const void *b)
{
    BenchResult *ra = *(BenchResult **)a;
    BenchResult *rb = *(BenchResult **)b;
    return strcmp(ra->timestamp, rb->timestamp);
}

/**
 * CollectFilteredResults
 *
 * Scans history_labels list and collects results matching current filter settings.
 * Results are sorted chronologically (oldest first) for trend display.
 *
 * @param out_results Output array of BenchResult pointers (caller-provided).
 * @param max_count Maximum number of results to collect.
 * @return Number of results collected.
 */
static uint32 CollectFilteredResults(BenchResult **out_results, uint32 max_count)
{
    uint32 count = 0;
    uint32 filter_test = ui.viz_filter_test_idx;
    uint32 filter_vol = ui.viz_filter_volume_idx;

    /* Scan history list */
    struct Node *node = IExec->GetHead(&ui.history_labels);
    while (node && count < max_count) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);

        if (res) {
            BOOL match = TRUE;

            /* Test type filter: index 0 = "All Tests" */
            if (filter_test > 0) {
                if ((uint32)res->type != (filter_test - 1))
                    match = FALSE;
            }

            /* Volume filter: index 0 = "All Volumes" */
            if (filter_vol > 0 && match) {
                /* Get the volume name from the chooser node at this index */
                struct Node *vol_node = IExec->GetHead(&ui.viz_volume_labels);
                uint32 idx = 0;
                const char *filter_name = NULL;
                while (vol_node) {
                    if (idx == filter_vol) {
                        IChooser->GetChooserNodeAttrs(vol_node, CNA_Text, &filter_name, TAG_DONE);
                        break;
                    }
                    vol_node = IExec->GetSucc(vol_node);
                    idx++;
                }
                if (filter_name && strcmp(res->volume_name, filter_name) != 0)
                    match = FALSE;
            }

            if (match) {
                out_results[count++] = res;
            }
        }
        node = IExec->GetSucc(node);
    }

    /* Also scan bench_labels (current session results) */
    node = IExec->GetHead(&ui.bench_labels);
    while (node && count < max_count) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);

        if (res) {
            BOOL match = TRUE;

            if (filter_test > 0) {
                if ((uint32)res->type != (filter_test - 1))
                    match = FALSE;
            }

            if (filter_vol > 0 && match) {
                struct Node *vol_node = IExec->GetHead(&ui.viz_volume_labels);
                uint32 idx = 0;
                const char *filter_name = NULL;
                while (vol_node) {
                    if (idx == filter_vol) {
                        IChooser->GetChooserNodeAttrs(vol_node, CNA_Text, &filter_name, TAG_DONE);
                        break;
                    }
                    vol_node = IExec->GetSucc(vol_node);
                    idx++;
                }
                if (filter_name && strcmp(res->volume_name, filter_name) != 0)
                    match = FALSE;
            }

            if (match) {
                out_results[count++] = res;
            }
        }
        node = IExec->GetSucc(node);
    }

    /* Sort chronologically (oldest first for left-to-right trend) */
    if (count > 1) {
        qsort(out_results, count, sizeof(BenchResult *), compare_by_timestamp);
    }

    return count;
}

/**
 * UpdateVisualization
 *
 * Collects filtered results and triggers a redraw of the SpaceObject graph.
 */
void UpdateVisualization(void)
{
    if (!ui.viz_canvas || !ui.window)
        return;

    LOG_DEBUG("Updating Visualization (Trend Graph)...");

    /* Force SpaceObject to redraw via RefreshGList */
    IIntuition->RefreshGList((struct Gadget *)ui.viz_canvas, ui.window, NULL, 1);

    LOG_DEBUG("Visualization redraw triggered.");
}

/**
 * VizRenderHook
 *
 * SPACE_RenderHook callback. Called by space.gadget during GM_RENDER.
 * Renders the trend graph directly into the SpaceObject's RastPort.
 */
uint32 VizRenderHook(struct Hook *hook, Object *space_obj, struct gpRender *gpr)
{
    if (!ui.viz_canvas || !ui.window)
        return 0;

    struct RastPort *rp = gpr->gpr_RPort;
    if (!rp)
        return 0;

    struct IBox *box = NULL;
    IIntuition->GetAttr(SPACE_AreaBox, space_obj, (uint32 *)&box);

    if (box) {
        BenchResult *results[MAX_PLOT_RESULTS];
        uint32 count = CollectFilteredResults(results, MAX_PLOT_RESULTS);
        RenderTrendGraph(rp, box, results, count);
    }
    return 0;
}

/**
 * InitVizFilterLabels
 *
 * Populates the Chooser label lists for visualization filters.
 * Must be called before CreateMainLayout and after test_labels is built.
 */
void InitVizFilterLabels(void)
{
    IExec->NewList(&ui.viz_volume_labels);
    IExec->NewList(&ui.viz_test_labels);
    IExec->NewList(&ui.viz_metric_labels);

    /* Volume filter: "All Volumes" + populate from history on refresh */
    struct Node *n;
    n = IChooser->AllocChooserNode(CNA_Text, "All Volumes", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_volume_labels, n);

    /* Test type filter: "All Tests" + each test type */
    n = IChooser->AllocChooserNode(CNA_Text, "All Tests", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_test_labels, n);

    for (int i = 0; i < TEST_COUNT; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, TestTypeToDisplayName((BenchTestType)i), CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_test_labels, n);
    }

    /* Metric filter: MB/s, IOPS */
    n = IChooser->AllocChooserNode(CNA_Text, "MB/s", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_metric_labels, n);
    n = IChooser->AllocChooserNode(CNA_Text, "IOPS", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_metric_labels, n);

    ui.viz_filter_volume_idx = 0;
    ui.viz_filter_test_idx = 0;
    ui.viz_filter_metric_idx = 0;
}

/**
 * RefreshVizVolumeFilter
 *
 * Scans history to find unique volume names and rebuilds the volume filter Chooser.
 * Called after RefreshHistory to keep the filter list current.
 */
void RefreshVizVolumeFilter(void)
{
    if (!ui.viz_filter_volume || !ui.window)
        return;

    /* Detach labels from chooser */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_filter_volume, ui.window, NULL, CHOOSER_Labels, (ULONG)-1,
                               TAG_DONE);

    /* Free existing nodes */
    struct Node *node, *next;
    node = IExec->GetHead(&ui.viz_volume_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_volume_labels);

    /* Re-add "All Volumes" */
    struct Node *n;
    n = IChooser->AllocChooserNode(CNA_Text, "All Volumes", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_volume_labels, n);

    /* Collect unique volume names from history */
    char seen_volumes[50][32];
    int seen_count = 0;

    struct Node *hnode = IExec->GetHead(&ui.history_labels);
    while (hnode && seen_count < 50) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(hnode, LBNA_UserData, &res, TAG_DONE);
        if (res) {
            BOOL found = FALSE;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen_volumes[i], res->volume_name) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                snprintf(seen_volumes[seen_count], sizeof(seen_volumes[seen_count]), "%s", res->volume_name);
                seen_count++;
                n = IChooser->AllocChooserNode(CNA_Text, res->volume_name, CNA_CopyText, TRUE, TAG_DONE);
                if (n)
                    IExec->AddTail(&ui.viz_volume_labels, n);
            }
        }
        hnode = IExec->GetSucc(hnode);
    }

    /* Reattach labels and reset selection */
    ui.viz_filter_volume_idx = 0;
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_filter_volume, ui.window, NULL, CHOOSER_Labels,
                               (uint32)&ui.viz_volume_labels, CHOOSER_Selected, 0, TAG_DONE);
}

/**
 * CleanupVizFilterLabels
 *
 * Frees all Chooser nodes for the visualization filter lists.
 * Must be called during GUI shutdown.
 */
void CleanupVizFilterLabels(void)
{
    struct Node *node, *next;

    node = IExec->GetHead(&ui.viz_volume_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_volume_labels);

    node = IExec->GetHead(&ui.viz_test_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_test_labels);

    node = IExec->GetHead(&ui.viz_metric_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_metric_labels);
}
