/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"
#include "viz_profile.h"
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

static int float_compare(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/**
 * @brief Collapse an array of Y values into a single representative value.
 */
static float CollapseYValues(float *vals, uint32 count, VizCollapseMethod method)
{
    if (count == 0) return 0.0f;
    if (count == 1) return vals[0];

    switch (method) {
    case VIZ_COLLAPSE_MEAN: {
        float sum = 0.0f;
        for (uint32 i = 0; i < count; i++) sum += vals[i];
        return sum / (float)count;
    }
    case VIZ_COLLAPSE_MEDIAN: {
        qsort(vals, count, sizeof(float), float_compare);
        if (count % 2 == 1) return vals[count / 2];
        return (vals[count / 2 - 1] + vals[count / 2]) / 2.0f;
    }
    case VIZ_COLLAPSE_MIN: {
        float m = vals[0];
        for (uint32 i = 1; i < count; i++)
            if (vals[i] < m) m = vals[i];
        return m;
    }
    case VIZ_COLLAPSE_MAX: {
        float m = vals[0];
        for (uint32 i = 1; i < count; i++)
            if (vals[i] > m) m = vals[i];
        return m;
    }
    default:
        return vals[0];
    }
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
 * @brief Case-insensitive substring match helper for profile filters.
 * Returns TRUE if the value passes the filter (is allowed through).
 */
static BOOL ApplyFilterList(VizFilterList *f, const char *value)
{
    if (f->count == 0)
        return TRUE; /* No filter entries = pass everything */

    for (uint32 i = 0; i < f->count; i++) {
        /* Case-insensitive substring match */
        const char *s = value;
        const char *p = f->values[i];
        uint32 plen = strlen(p);
        uint32 slen = strlen(s);
        BOOL found = FALSE;
        if (plen <= slen) {
            for (uint32 j = 0; j <= slen - plen; j++) {
                BOOL eq = TRUE;
                for (uint32 k = 0; k < plen; k++) {
                    char a = s[j + k];
                    char b = p[k];
                    if (a >= 'A' && a <= 'Z') a += 32;
                    if (b >= 'A' && b <= 'Z') b += 32;
                    if (a != b) { eq = FALSE; break; }
                }
                if (eq) { found = TRUE; break; }
            }
        }
        if (f->mode == VIZ_FILTER_INCLUDE && found)
            return TRUE;
        if (f->mode == VIZ_FILTER_EXCLUDE && found)
            return FALSE;
    }

    /* Include mode: none matched = reject. Exclude mode: none matched = pass. */
    return (f->mode == VIZ_FILTER_EXCLUDE) ? TRUE : FALSE;
}

/**
 * @brief Get the averaging method as a string for filter matching.
 */
static const char *AveragingToString(uint32 method)
{
    switch (method) {
    case AVERAGE_TRIMMED_MEAN: return "TrimmedMean";
    case AVERAGE_MEDIAN:       return "Median";
    default:                   return "AllPasses";
    }
}

/**
 * @brief Extract the Y-axis value from a result based on the profile's y_source.
 */
static float GetYValue(BenchResult *res, VizYSource src)
{
    switch (src) {
    case VIZ_SRC_IOPS:          return (float)res->iops;
    case VIZ_SRC_MIN_MBPS:      return res->min_mbps;
    case VIZ_SRC_MAX_MBPS:      return res->max_mbps;
    case VIZ_SRC_DURATION_SECS: return res->total_duration;
    case VIZ_SRC_TOTAL_BYTES:   return (float)res->cumulative_bytes;
    default:                    return res->mb_per_sec;
    }
}

/**
 * @brief Collects and filters benchmark results from the history and session lists.
 *
 * Profile-driven: uses the active VizProfile for grouping, filtering, Y source,
 * and sorting. On-screen chooser filters (volume, test, date, version) are applied
 * on top of profile-level filters.
 */
static uint32 CollectVizData(VizData *vd)
{
    memset(vd, 0, sizeof(VizData));
    VizDateRange filter_date = (VizDateRange)ui.viz_date_range_idx;

    VizProfile *profile = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;

    struct List *lists[] = {&ui.history_labels, &ui.bench_labels};
    for (int l = 0; l < 2; l++) {
        struct Node *node = IExec->GetHead(lists[l]);
        while (node) {
            BenchResult *res = NULL;
            IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);
            if (res) {
                BOOL match = TRUE;

                /* On-screen date filter (history list only) */
                if (l == 0 && !IsDateInRange(res->timestamp, filter_date))
                    match = FALSE;

                /* Profile-level filters */
                if (match && profile) {
                    if (!ApplyFilterList(&profile->filter_test, TestTypeToString(res->type)))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_block_size, FormatPresetBlockSize(res->block_size)))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_volume, res->volume_name))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_filesystem, res->fs_type))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_hardware, res->device_name))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_vendor, res->vendor))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_product, res->product))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_averaging, AveragingToString(res->averaging_method)))
                        match = FALSE;
                    if (match && !ApplyFilterList(&profile->filter_version, res->app_version))
                        match = FALSE;
                    /* Numeric threshold filters */
                    if (match && profile->min_passes > 0 && res->passes < profile->min_passes)
                        match = FALSE;
                    if (match && profile->min_mbs > 0.0f && res->mb_per_sec < profile->min_mbs)
                        match = FALSE;
                    if (match && profile->max_mbs > 0.0f && res->mb_per_sec > profile->max_mbs)
                        match = FALSE;
                    if (match && profile->min_duration_secs > 0.0f && res->total_duration < profile->min_duration_secs)
                        match = FALSE;
                    if (match && profile->max_duration_secs > 0.0f && res->total_duration > profile->max_duration_secs)
                        match = FALSE;
                }

                if (match) {
                    /* Determine series label from profile's group_by */
                    char label[64];
                    VizGroupBy group = profile ? profile->group_by : VIZ_GROUP_DRIVE;
                    switch (group) {
                    case VIZ_GROUP_TEST_TYPE:
                        snprintf(label, sizeof(label), "%s", TestTypeToDisplayName(res->type));
                        break;
                    case VIZ_GROUP_BLOCK_SIZE:
                        snprintf(label, sizeof(label), "%s", FormatPresetBlockSize(res->block_size));
                        break;
                    case VIZ_GROUP_FILESYSTEM:
                        snprintf(label, sizeof(label), "%s", res->fs_type);
                        break;
                    case VIZ_GROUP_HARDWARE:
                        snprintf(label, sizeof(label), "%s", res->device_name);
                        break;
                    case VIZ_GROUP_VENDOR:
                        snprintf(label, sizeof(label), "%s", res->vendor);
                        break;
                    case VIZ_GROUP_APP_VERSION:
                        snprintf(label, sizeof(label), "%s", res->app_version);
                        break;
                    case VIZ_GROUP_AVERAGING:
                        snprintf(label, sizeof(label), "%s", AveragingToString(res->averaging_method));
                        break;
                    default: /* VIZ_GROUP_DRIVE */
                        snprintf(label, sizeof(label), "%s", res->volume_name);
                        /* Replace underscores with spaces for display */
                        for (char *p = label; *p; p++) {
                            if (*p == '_') *p = ' ';
                        }
                        break;
                    }

                    VizSeries *s = GetSeries(vd, label);
                    if (s && s->count < 200) {
                        s->results[s->count++] = res;
                        vd->total_points++;
                        float yval = profile ? GetYValue(res, profile->y_source) : res->mb_per_sec;
                        if (yval > s->max_val)
                            s->max_val = yval;
                        if (yval > vd->global_max_y1)
                            vd->global_max_y1 = yval;
                        if ((float)res->iops > vd->global_max_y2)
                            vd->global_max_y2 = (float)res->iops;
                    }
                }
            }
            node = IExec->GetSucc(node);
        }
    }

    /* Apply max_series cap from profile */
    if (profile && profile->max_series > 0 && vd->series_count > profile->max_series)
        vd->series_count = profile->max_series;

    /* Sort results within each series based on profile X-axis source */
    for (uint32 i = 0; i < vd->series_count; i++) {
        if (profile && profile->sort_x_by_value) {
            qsort(vd->series[i].results, vd->series[i].count, sizeof(BenchResult *), compare_by_block_size);
        } else if (profile && profile->x_source == VIZ_SRC_BLOCK_SIZE) {
            qsort(vd->series[i].results, vd->series[i].count, sizeof(BenchResult *), compare_by_block_size);
        }
        /* VIZ_SRC_TIMESTAMP / VIZ_SRC_TEST_INDEX: keep chronological insertion order */
    }

    /* Apply collapse aggregation if profile requests it */
    if (profile && profile->collapse_method != VIZ_COLLAPSE_NONE) {
        VizYSource ysrc = profile->y_source;
        /* Static buffer for collapsed result copies (avoids mutating shared results) */
        static BenchResult collapsed_buf[MAX_SERIES * 200];
        uint32 buf_idx = 0;

        for (uint32 si = 0; si < vd->series_count; si++) {
            VizSeries *s = &vd->series[si];
            if (s->count < 2) continue;

            if (profile->x_source != VIZ_SRC_BLOCK_SIZE) {
                /* Non-block_size X: collapse entire series into one point */
                if (buf_idx >= MAX_SERIES * 200) continue;
                float y_vals[200];
                uint32 n = (s->count < 200) ? s->count : 200;
                for (uint32 k = 0; k < n; k++)
                    y_vals[k] = GetYValue(s->results[k], ysrc);
                float collapsed_y = CollapseYValues(y_vals, n, profile->collapse_method);
                collapsed_buf[buf_idx] = *s->results[0];
                switch (ysrc) {
                case VIZ_SRC_IOPS:          collapsed_buf[buf_idx].iops = (uint32)collapsed_y; break;
                case VIZ_SRC_MIN_MBPS:      collapsed_buf[buf_idx].min_mbps = collapsed_y; break;
                case VIZ_SRC_MAX_MBPS:      collapsed_buf[buf_idx].max_mbps = collapsed_y; break;
                case VIZ_SRC_DURATION_SECS: collapsed_buf[buf_idx].total_duration = collapsed_y; break;
                case VIZ_SRC_TOTAL_BYTES:   collapsed_buf[buf_idx].cumulative_bytes = (uint64)collapsed_y; break;
                default:                    collapsed_buf[buf_idx].mb_per_sec = collapsed_y; break;
                }
                vd->total_points -= (s->count - 1);
                s->results[0] = &collapsed_buf[buf_idx];
                s->count = 1;
                buf_idx++;
            } else {
                /* Block size X: collapse runs of same block size */
                uint32 out = 0;
                uint32 j = 0;
                while (j < s->count) {
                    uint32 run_start = j;
                    uint32 x_key = s->results[j]->block_size;
                    uint32 run_end = j + 1;
                    while (run_end < s->count && s->results[run_end]->block_size == x_key)
                        run_end++;
                    uint32 run_len = run_end - run_start;

                    if (run_len == 1 || buf_idx >= MAX_SERIES * 200) {
                        s->results[out++] = s->results[j];
                        j = run_end;
                        continue;
                    }

                    float y_vals[200];
                    for (uint32 k = 0; k < run_len && k < 200; k++)
                        y_vals[k] = GetYValue(s->results[run_start + k], ysrc);
                    float collapsed_y = CollapseYValues(y_vals, run_len, profile->collapse_method);
                    collapsed_buf[buf_idx] = *s->results[run_start];
                    switch (ysrc) {
                    case VIZ_SRC_IOPS:          collapsed_buf[buf_idx].iops = (uint32)collapsed_y; break;
                    case VIZ_SRC_MIN_MBPS:      collapsed_buf[buf_idx].min_mbps = collapsed_y; break;
                    case VIZ_SRC_MAX_MBPS:      collapsed_buf[buf_idx].max_mbps = collapsed_y; break;
                    case VIZ_SRC_DURATION_SECS: collapsed_buf[buf_idx].total_duration = collapsed_y; break;
                    case VIZ_SRC_TOTAL_BYTES:   collapsed_buf[buf_idx].cumulative_bytes = (uint64)collapsed_y; break;
                    default:                    collapsed_buf[buf_idx].mb_per_sec = collapsed_y; break;
                    }
                    s->results[out++] = &collapsed_buf[buf_idx];
                    buf_idx++;
                    j = run_end;
                }
                vd->total_points -= (s->count - out);
                s->count = out;
            }
        }

        /* Recompute global max after collapse */
        vd->global_max_y1 = 0.0f;
        vd->global_max_y2 = 0.0f;
        for (uint32 si = 0; si < vd->series_count; si++) {
            vd->series[si].max_val = 0.0f;
            for (uint32 ri = 0; ri < vd->series[si].count; ri++) {
                BenchResult *r = vd->series[si].results[ri];
                float yval = GetYValue(r, ysrc);
                if (yval > vd->series[si].max_val)
                    vd->series[si].max_val = yval;
                if (yval > vd->global_max_y1)
                    vd->global_max_y1 = yval;
                if ((float)r->iops > vd->global_max_y2)
                    vd->global_max_y2 = (float)r->iops;
            }
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
    IExec->NewList(&ui.viz_metric_labels);
    IExec->NewList(&ui.viz_chart_type_labels);

    /* Date Range Filter Options (Today, Last Week, etc.) */
    struct Node *n;
    const char *date_opts[] = {"Today", "Last Week", "Last Month", "Last Year", "All Time"};
    for (int i = 0; i < 5; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, date_opts[i], CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_metric_labels, n);
    }

    /* Chart Type Labels — populated from loaded .viz profiles */
    for (uint32 i = 0; i < g_viz_profile_count; i++) {
        n = IChooser->AllocChooserNode(CNA_Text, g_viz_profiles[i].name, CNA_CopyText, TRUE, TAG_DONE);
        if (n)
            IExec->AddTail(&ui.viz_chart_type_labels, n);
    }

    ui.viz_date_range_idx = 4;  /* Default to All Time */
    ui.viz_chart_type_idx = 0;  /* Default to first profile */
}

/**
 * RefreshVizVolumeFilter
 *
 * Stub — volume filter removed in v2.5.1. Profile [Filters] handle this.
 */
void RefreshVizVolumeFilter(void)
{
    /* No-op: volume filtering is now profile-driven */
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

    FreeVizProfiles();
}

/**
 * ReloadVizProfiles
 *
 * Re-scans PROGDIR:Visualizations/ and rebuilds the chart type chooser.
 * Non-fatal on failure — keeps previous profiles.
 */
void ReloadVizProfiles(void)
{
    if (!ui.viz_chart_type || !ui.window)
        return;

    /* Detach labels from chooser */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_chart_type, ui.window, NULL,
                               CHOOSER_Labels, (ULONG)-1, TAG_DONE);

    /* Free existing chooser nodes */
    struct Node *node, *next;
    node = IExec->GetHead(&ui.viz_chart_type_labels);
    while (node) {
        next = IExec->GetSucc(node);
        IChooser->FreeChooserNode(node);
        node = next;
    }
    IExec->NewList(&ui.viz_chart_type_labels);

    /* Reload profiles */
    FreeVizProfiles();
    if (!LoadVizProfiles()) {
        ShowMessage("Reload Failed",
                    "No valid .viz files found in\nPROGDIR:Visualizations/\n\nPrevious profiles retained.",
                    "OK");
        /* Re-add placeholder */
        struct Node *n = IChooser->AllocChooserNode(CNA_Text, "(no profiles)", CNA_CopyText, TRUE, TAG_DONE);
        if (n) IExec->AddTail(&ui.viz_chart_type_labels, n);
    } else {
        /* Rebuild chooser from new profiles */
        for (uint32 i = 0; i < g_viz_profile_count; i++) {
            struct Node *n = IChooser->AllocChooserNode(CNA_Text, g_viz_profiles[i].name,
                                                         CNA_CopyText, TRUE, TAG_DONE);
            if (n) IExec->AddTail(&ui.viz_chart_type_labels, n);
        }
    }

    /* Reattach and reset */
    ui.viz_chart_type_idx = 0;
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_chart_type, ui.window, NULL,
                               CHOOSER_Labels, (uint32)&ui.viz_chart_type_labels,
                               CHOOSER_Selected, 0, TAG_DONE);

    /* Update Color By display for new profile */
    if (g_viz_profile_count > 0 && ui.viz_color_by_display) {
        static const char *group_names[] = {"Drive", "Test Type", "Block Size", "Filesystem",
                                            "Hardware", "Vendor", "App Version", "Averaging"};
        VizProfile *p = &g_viz_profiles[0];
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_color_by_display, ui.window, NULL,
                                   GA_Text, (uint32)group_names[p->group_by], TAG_DONE);

        /* Reset date range to profile default */
        ui.viz_date_range_idx = (uint32)p->default_date_range;
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_filter_metric, ui.window, NULL,
                                   CHOOSER_Selected, ui.viz_date_range_idx, TAG_DONE);
    }

    UpdateVisualization();
}

/**
 * RefreshVizVersionFilter
 *
 * Stub — version filter removed in v2.5.1. Profile [Filters] handle this.
 */
void RefreshVizVersionFilter(void)
{
    /* No-op: version filtering is now profile-driven */
}
