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

/* Parse "YYYY-MM-DD HH:MM:SS" into year, month, day */
static void ParseDate(const char *timestamp, int *y, int *m, int *d)
{
    if (strlen(timestamp) >= 10) {
        sscanf(timestamp, "%d-%d-%d", y, m, d);
    } else {
        *y = 0;
        *m = 0;
        *d = 0;
    }
}

/**
 * @brief Checks if a benchmark result's timestamp falls within the selected date range.
 *
 * Supports filtering by Today, Last Week, Last Month, Last Year, or All Time.
 * Uses system time via DateStamp to calculate the range.
 */
static BOOL IsDateInRange(const char *timestamp, VizDateRange range)
{
    if (range == VIZ_DATE_ALL)
        return TRUE;

    int ty, tm, td;
    ParseDate(timestamp, &ty, &tm, &td);

    struct DateStamp ds;
    IDOS->DateStamp(&ds);

    struct ClockData cd;
    if (ui.IUtility) {
        uint32 seconds = (ds.ds_Days * 86400) + (ds.ds_Minute * 60) + (ds.ds_Tick / 50);
        ui.IUtility->Amiga2Date(seconds, &cd);
    } else {
        return TRUE;
    }

    int cy = cd.year;
    int cm = cd.month;
    int cd_day = cd.mday;

    if (range == VIZ_DATE_TODAY) {
        return (ty == cy && tm == cm && td == cd_day);
    }

    if (range == VIZ_DATE_WEEK) {
        if (ty == cy && tm == cm) {
            if (abs(cd_day - td) <= 7)
                return TRUE;
        }
    }

    if (range == VIZ_DATE_MONTH) {
        return (ty == cy && tm == cm);
    }

    if (range == VIZ_DATE_YEAR) {
        return (ty == cy);
    }

    return TRUE;
}

/**
 * @brief Comparison function for qsort to sort results by block size.
 */
static int compare_by_block_size(const void *a, const void *b)
{
    BenchResult *resA = *(BenchResult **)a;
    BenchResult *resB = *(BenchResult **)b;
    if (resA->block_size < resB->block_size)
        return -1;
    if (resA->block_size > resB->block_size)
        return 1;
    return 0;
}

/**
 * @brief Internal helper to find/create a data series for a given categorical label.
 */
static VizSeries *GetSeries(VizData *vd, const char *label)
{
    for (uint32 i = 0; i < vd->series_count; i++) {
        if (strcmp(vd->series[i].label, label) == 0)
            return &vd->series[i];
    }
    if (vd->series_count < MAX_SERIES) {
        VizSeries *s = &vd->series[vd->series_count++];
        snprintf(s->label, sizeof(s->label), "%s", label);
        s->count = 0;
        s->max_val = 0.0f;
        return s;
    }
    return NULL;
}

/**
 * @brief Collects and filters benchmark results from the history and session lists.
 *
 * This is the core data engine for the visualization tab. It applies all active
 * filters (Drive, Test, Date) and groups the resulting data into series based
 * on the "Color By" selection.
 *
 * @param vd Output structure to be populated with filtered and grouped data.
 * @return Number of data series generated.
 */
/**
 * @brief Comparison function for qsort to sort results by test type.
 * Used for "Workload" charts to group bars by operation (Read/Write, Seq/Rand).
 */
static int compare_by_test_type(const void *a, const void *b)
{
    BenchResult *resA = *(BenchResult **)a;
    BenchResult *resB = *(BenchResult **)b;
    if (resA->type < resB->type)
        return -1;
    if (resA->type > resB->type)
        return 1;
    return 0;
}

static uint32 CollectVizData(VizData *vd)
{
    memset(vd, 0, sizeof(VizData));
    uint32 filter_test = ui.viz_filter_test_idx;
    uint32 filter_vol = ui.viz_filter_volume_idx;
    uint32 filter_ver = ui.viz_filter_version_idx;
    VizDateRange filter_date = (VizDateRange)ui.viz_date_range_idx;
    uint32 color_by = ui.viz_color_by_idx;

    struct List *lists[] = {&ui.history_labels, &ui.bench_labels};
    for (int l = 0; l < 2; l++) {
        struct Node *node = IExec->GetHead(lists[l]);
        while (node) {
            BenchResult *res = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);
            if (res) {
                BOOL match = TRUE;
                if (l == 0 && !IsDateInRange(res->timestamp, filter_date))
                    match = FALSE;

                if (filter_test > 0 && (uint32)res->type != (filter_test - 1))
                    match = FALSE;

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

                if (filter_ver > 0 && match) {
                    struct Node *ver_node = IExec->GetHead(&ui.viz_version_labels);
                    uint32 idx = 0;
                    const char *filter_name = NULL;
                    while (ver_node) {
                        if (idx == filter_ver) {
                            IChooser->GetChooserNodeAttrs(ver_node, CNA_Text, &filter_name, TAG_DONE);
                            break;
                        }
                        ver_node = IExec->GetSucc(ver_node);
                        idx++;
                    }
                    if (filter_name && strcmp(res->app_version, filter_name) != 0)
                        match = FALSE;
                }

                if (match) {
                    /* Determine series label */
                    char label[64];
                    if (color_by == 0) /* Drive */
                        snprintf(label, sizeof(label), "%s", res->volume_name);
                    else if (color_by == 1) /* Test Type */
                        snprintf(label, sizeof(label), "%s", TestTypeToDisplayName(res->type));
                    else if (color_by == 2) /* Block Size */
                        snprintf(label, sizeof(label), "%s", FormatPresetBlockSize(res->block_size));
                    else
                        snprintf(label, sizeof(label), "Default");

                    VizSeries *s = GetSeries(vd, label);
                    if (s && s->count < 200) {
                        s->results[s->count++] = res;
                        vd->total_points++;
                        if (res->mb_per_sec > s->max_val)
                            s->max_val = res->mb_per_sec;
                        if (res->mb_per_sec > vd->global_max_y1)
                            vd->global_max_y1 = res->mb_per_sec;
                        if ((float)res->iops > vd->global_max_y2)
                            vd->global_max_y2 = (float)res->iops;
                    }
                }
            }
            node = IExec->GetSucc(node);
        }
    }

    /* Sort results within each series based on chart type (X-axis) */
    for (uint32 i = 0; i < vd->series_count; i++) {
        if (ui.viz_chart_type_idx == 1) { /* Trend (Time) */
            // History is usually sorted by time, but benchmarking might be random.
            // For now assume chronological order is fine or implement sort by timestamp if needed.
        } else if (ui.viz_chart_type_idx == 3) { /* Workload (Test Type) */
            qsort(vd->series[i].results, vd->series[i].count, sizeof(BenchResult *), compare_by_test_type);
        } else { /* Scaling (Block Size) or others */
            qsort(vd->series[i].results, vd->series[i].count, sizeof(BenchResult *), compare_by_block_size);
        }
    }

    return vd->series_count;
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

    LOG_DEBUG("Updating Visualization (Chart Type %u)...", ui.viz_chart_type_idx);

    /* Force SpaceObject to redraw via RefreshGList */
    IIntuition->RefreshGList((struct Gadget *)ui.viz_canvas, ui.window, NULL, 1);
}

/**
 * VizRenderHook
 *
 * SPACE_RenderHook callback. Called by space.gadget during GM_RENDER.
 * Renders the graph directly into the SpaceObject's RastPort.
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
        VizData vd;
        CollectVizData(&vd);
        RenderGraph(rp, box, &vd);
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
    IExec->NewList(&ui.viz_version_labels);
    IExec->NewList(&ui.viz_chart_type_labels);
    IExec->NewList(&ui.viz_color_by_labels);

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

    /* Version filter: "All Versions" */
    n = IChooser->AllocChooserNode(CNA_Text, "All Versions", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_version_labels, n);

    /* Date Range Filter Options (Today, Last Week, etc.) */
    const char *date_opts[] = {"Today", "Last Week", "Last Month", "Last Year", "All Time"};
    for (int i = 0; i < 5; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, date_opts[i], CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_metric_labels, n); // Using existing list for Date Range
    }

    /* Chart Type Labels */
    const char *chart_types[] = {"Scaling (Line)", "Trend (Time Line)", "Battle (Drive Bar)", "Workload (Test Bar)",
                                 "Hybrid (MB/s+IOPS)"};
    for (int i = 0; i < 5; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, chart_types[i], CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_chart_type_labels, n);
    }

    /* Color By Labels */
    const char *color_by_opts[] = {"Drive", "Test Type", "Block Size"};
    for (int i = 0; i < 3; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, color_by_opts[i], CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_color_by_labels, n);
    }

    ui.viz_filter_volume_idx = 0;
    ui.viz_filter_test_idx = 0;
    ui.viz_date_range_idx = 4;     // Default to All Time
    ui.viz_filter_version_idx = 0; // Default to All Versions
    ui.viz_chart_type_idx = 0;     // Default to Scaling
    ui.viz_color_by_idx = 0;       // Default to Drive
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

    node = IExec->GetHead(&ui.viz_chart_type_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_chart_type_labels);

    IExec->NewList(&ui.viz_color_by_labels);

    node = IExec->GetHead(&ui.viz_version_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_version_labels);
}

/**
 * RefreshVizVersionFilter
 *
 * Scans history to find unique app version strings and rebuilds the version filter Chooser.
 * Called after RefreshHistory to keep the filter list current.
 */
void RefreshVizVersionFilter(void)
{
    if (!ui.viz_filter_version || !ui.window)
        return;

    /* Detach labels from chooser */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_filter_version, ui.window, NULL, CHOOSER_Labels, (ULONG)-1,
                               TAG_DONE);

    /* Free existing nodes */
    struct Node *node, *next;
    node = IExec->GetHead(&ui.viz_version_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_version_labels);

    /* Re-add "All Versions" */
    struct Node *n;
    n = IChooser->AllocChooserNode(CNA_Text, "All Versions", CNA_CopyText, TRUE, TAG_DONE);
    if (n)
        IExec->AddTail(&ui.viz_version_labels, n);

    /* Collect unique version strings from history */
    char seen_versions[50][16];
    int seen_count = 0;

    struct Node *hnode = IExec->GetHead(&ui.history_labels);
    while (hnode && seen_count < 50) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(hnode, LBNA_UserData, &res, TAG_DONE);
        if (res && res->app_version[0]) {
            BOOL found = FALSE;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen_versions[i], res->app_version) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                snprintf(seen_versions[seen_count], sizeof(seen_versions[seen_count]), "%s", res->app_version);
                seen_count++;
                n = IChooser->AllocChooserNode(CNA_Text, res->app_version, CNA_CopyText, TRUE, TAG_DONE);
                if (n)
                    IExec->AddTail(&ui.viz_version_labels, n);
            }
        }
        hnode = IExec->GetSucc(hnode);
    }

    /* Reattach labels and reset selection */
    ui.viz_filter_version_idx = 0;
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_filter_version, ui.window, NULL, CHOOSER_Labels,
                               (uint32)&ui.viz_version_labels, CHOOSER_Selected, 0, TAG_DONE);
}
