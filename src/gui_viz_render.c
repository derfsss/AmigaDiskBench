/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 */

#include "gui_internal.h"
#include "viz_profile.h"
#include <graphics/rpattr.h>
#include <proto/graphics.h>
#include <stdlib.h>

/* Graph layout constants - margins in pixels */
#define MARGIN_LEFT 60
#define MARGIN_RIGHT 50 /* Increased to accommodate IOPS labels */
#define MARGIN_TOP 24
#define MARGIN_BOTTOM 60 /* Increased for multi-line legend */
#define TICK_LEN 4
#define MAX_GRAPH_POINTS 400

/* Fixed color palette indices for graph series - ARGB32 values */
static const uint32 series_colors[] = {
    0x00BBDD00, /* Bright green */
    0x000088FF, /* Blue */
    0x00FF4444, /* Red */
    0x00FFAA00, /* Orange */
    0x00AA44FF, /* Purple */
    0x0000CCCC, /* Teal */
    0x00FF66AA, /* Pink */
    0x00888800, /* Olive */
    0x00EEFF22, /* Yellow-Green */
    0x0022AAFF, /* Sky Blue */
    0x00FF77EE, /* Magenta */
    0x0077FF77, /* Pastel Green */
    0x00FFBB00, /* Gold */
    0x0000CCEE, /* Cyan */
    0x00CCAAFF, /* Lavender */
    0x0088CC88, /* Sage */
};
#define NUM_SERIES_COLORS (sizeof(series_colors) / sizeof(series_colors[0]))

/**
 * @brief Get the color for a series, using profile custom colors if available.
 */
static uint32 GetSeriesColor(uint32 idx)
{
    VizProfile *p = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    if (p && p->color_count > 0)
        return p->colors[idx % p->color_count];
    return series_colors[idx % NUM_SERIES_COLORS];
}

/* Stored points for hover detection */
typedef struct
{
    int x, y;
    BenchResult *res;
} VizPoint;

static VizPoint plotted_points[MAX_GRAPH_POINTS];
static uint32 plotted_count = 0;

/* --- Pen management using ObtainBestPen --- */

static LONG ObtainColorPen(struct RastPort *rp, uint32 argb)
{
    struct ColorMap *cm = NULL;
    if (ui.window && ui.window->WScreen) {
        cm = ui.window->WScreen->ViewPort.ColorMap;
    }
    if (!cm)
        return 1;

    uint8 r = (argb >> 16) & 0xFF;
    uint8 g = (argb >> 8) & 0xFF;
    uint8 b = argb & 0xFF;

    return IGraphics->ObtainBestPen(cm, (uint32)r << 24 | (uint32)r << 16 | (uint32)r << 8 | r,
                                    (uint32)g << 24 | (uint32)g << 16 | (uint32)g << 8 | g,
                                    (uint32)b << 24 | (uint32)b << 16 | (uint32)b << 8 | b, OBP_Precision,
                                    PRECISION_IMAGE, TAG_DONE);
}

static void ReleaseColorPen(struct RastPort *rp, LONG pen)
{
    struct ColorMap *cm = NULL;
    if (ui.window && ui.window->WScreen) {
        cm = ui.window->WScreen->ViewPort.ColorMap;
    }
    if (cm && pen >= 0)
        IGraphics->ReleasePen(cm, pen);
}

/* --- Drawing primitives --- */

static void DrawDashedHLine(struct RastPort *rp, int x1, int x2, int y, int dash_len)
{
    int on = 1;
    for (int x = x1; x <= x2; x += dash_len) {
        int end = x + dash_len - 1;
        if (end > x2)
            end = x2;
        if (on) {
            IGraphics->Move(rp, x, y);
            IGraphics->Draw(rp, end, y);
        }
        on = !on;
    }
}

static void DrawSmallText(struct RastPort *rp, int x, int y, const char *text)
{
    IGraphics->Move(rp, x, y);
    IGraphics->Text(rp, text, strlen(text));
}

/**
 * @brief Draw annotation reference lines from the profile.
 */
static void DrawAnnotations(struct RastPort *rp, int px, int py, int pw, int ph, float max_y)
{
    VizProfile *ap = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    if (!ap || ap->ref_line_count == 0 || max_y <= 0.0f)
        return;

    LONG ref_pen = ObtainColorPen(rp, 0x00FFFFFF); /* White dashed lines */
    LONG label_pen = ObtainColorPen(rp, 0x00DDDDEE);

    for (uint32 i = 0; i < ap->ref_line_count; i++) {
        float val = ap->ref_line_values[i];
        int y = py + ph - (int)((val / max_y) * (float)ph);
        /* Only draw if within chart bounds */
        if (y >= py && y <= py + ph) {
            IGraphics->SetAPen(rp, ref_pen);
            DrawDashedHLine(rp, px, px + pw - 1, y, 6);
            /* Draw label right-aligned */
            if (ap->ref_line_labels[i][0]) {
                IGraphics->SetAPen(rp, label_pen);
                struct TextExtent te;
                IGraphics->TextExtent(rp, ap->ref_line_labels[i], strlen(ap->ref_line_labels[i]), &te);
                DrawSmallText(rp, px + pw - te.te_Width - 2, y - 2, ap->ref_line_labels[i]);
            }
        }
    }

    ReleaseColorPen(rp, ref_pen);
    ReleaseColorPen(rp, label_pen);
}

/**
 * @brief Renders the grid, axes, and Y-axis labels.
 */
static void DrawGridAndAxes(struct RastPort *rp, int px, int py, int pw, int ph, float max_y, LONG grid_pen,
                            LONG axis_pen, LONG text_pen)
{
    /* Grid & Axis */
    IGraphics->SetAPen(rp, grid_pen);
    for (int i = 0; i <= 4; i++) {
        int ly = py + ph - (int)((float)(i * ph) / 4.0f);
        DrawDashedHLine(rp, px, px + pw - 1, ly, 4);
    }
    IGraphics->SetAPen(rp, axis_pen);
    IGraphics->Move(rp, px, py);
    IGraphics->Draw(rp, px, py + ph);
    IGraphics->Draw(rp, px + pw, py + ph);

    /* Y Labels (MB/s) */
    IGraphics->SetAPen(rp, text_pen);
    for (int i = 0; i <= 4; i++) {
        float val = (max_y * (float)i / 4.0f);
        int ly = py + ph - (int)((float)(i * ph) / 4.0f);
        char label[32];
        snprintf(label, sizeof(label), "%.1f", val);
        struct TextExtent te;
        IGraphics->TextExtent(rp, label, strlen(label), &te);
        DrawSmallText(rp, px - te.te_Width - 4, ly + 4, label);
    }

    /* Y-Axis Title — use profile label if available */
    VizProfile *axis_profile = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    const char *y_title = (axis_profile && axis_profile->y_label[0]) ? axis_profile->y_label : "MB/s";
    DrawSmallText(rp, px - MARGIN_LEFT + 4, py - 12, y_title);

    /* X-Axis Title — use profile label if available */
    const char *x_title = (axis_profile && axis_profile->x_label[0]) ? axis_profile->x_label : "Index";
    struct TextExtent te;
    IGraphics->TextExtent(rp, x_title, strlen(x_title), &te);
    /* Draw title at bottom-right, below the X-axis labels area */
    /* Positioned at py + ph + 24 to ensure it clears the axis labels */
    DrawSmallText(rp, px + pw - te.te_Width, py + ph + 24, x_title);
}

/**
 * @brief Renders X-axis labels (Block Sizes or Category names).
 */
/**
 * @brief Get an X-axis label for a result based on the profile's x_source.
 */
static const char *GetXLabel(BenchResult *res, VizXSource src, uint32 index, char *buf, uint32 buf_size)
{
    switch (src) {
    case VIZ_SRC_BLOCK_SIZE:
        return FormatPresetBlockSize(res->block_size);
    case VIZ_SRC_TIMESTAMP:
        /* Show date portion only (first 10 chars of "YYYY-MM-DD HH:MM:SS") */
        snprintf(buf, buf_size, "%.10s", res->timestamp);
        return buf;
    default: /* VIZ_SRC_TEST_INDEX */
        snprintf(buf, buf_size, "#%u", (unsigned int)(index + 1));
        return buf;
    }
}

static void DrawXAxisLabels(struct RastPort *rp, int px, int py, int pw, int ph, VizData *vd, BOOL is_trend,
                            LONG text_pen)
{
    IGraphics->SetAPen(rp, text_pen);
    int label_y = py + ph + 12;
    struct TextExtent te;

    VizProfile *xp = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    VizXSource xsrc = xp ? xp->x_source : VIZ_SRC_BLOCK_SIZE;

    if (vd->series_count > 0 && vd->series[0].count > 0) {
        uint32 count = vd->series[0].count;
        char buf[32], first_buf[32], last_buf[32];

        /* First */
        const char *label = GetXLabel(vd->series[0].results[0], xsrc, 0, buf, sizeof(buf));
        snprintf(first_buf, sizeof(first_buf), "%s", label);
        DrawSmallText(rp, px, label_y, first_buf);

        /* Last (skip if same text as first) */
        if (count > 1) {
            label = GetXLabel(vd->series[0].results[count - 1], xsrc, count - 1, buf, sizeof(buf));
            snprintf(last_buf, sizeof(last_buf), "%s", label);
            if (strcmp(last_buf, first_buf) != 0) {
                IGraphics->TextExtent(rp, last_buf, strlen(last_buf), &te);
                DrawSmallText(rp, px + pw - te.te_Width, label_y, last_buf);
            }
        }

        /* Middle (skip if same text as first or last) */
        if (count > 2) {
            label = GetXLabel(vd->series[0].results[count / 2], xsrc, count / 2, buf, sizeof(buf));
            if (strcmp(label, first_buf) != 0 && strcmp(label, last_buf) != 0) {
                IGraphics->TextExtent(rp, label, strlen(label), &te);
                DrawSmallText(rp, px + pw / 2 - te.te_Width / 2, label_y, label);
            }
        }
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

/* --- Trend Lines --- */

/**
 * @brief Clamp an integer value to [lo, hi].
 */
static int ClampInt(int val, int lo, int hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/**
 * @brief Lighten a color by averaging each channel with 255.
 */
static uint32 LightenColor(uint32 argb)
{
    uint8 r = ((argb >> 16) & 0xFF);
    uint8 g = ((argb >> 8) & 0xFF);
    uint8 b = (argb & 0xFF);
    r = (uint8)((r + 255) / 2);
    g = (uint8)((g + 255) / 2);
    b = (uint8)((b + 255) / 2);
    return ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b;
}

/**
 * @brief Draw trend line(s) for the chart based on profile settings.
 * Called after the main chart data is plotted, before legend.
 */
static void DrawTrendLines(struct RastPort *rp, int px, int py, int pw, int ph,
                           VizData *vd, float max_y)
{
    VizProfile *tp = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    if (!tp || tp->trend_style == VIZ_TREND_NONE || max_y <= 0.0f)
        return;

    /* Collect x positions and y values, either per-series or aggregated */
    float x_data[200], y_data[200], y_fit[200];
    uint32 n;

    if (tp->trend_per_series) {
        /* Draw one trend line per series */
        for (uint32 s = 0; s < vd->series_count; s++) {
            n = vd->series[s].count;
            if (n < 2) continue;
            if (n > 200) n = 200;

            for (uint32 i = 0; i < n; i++) {
                x_data[i] = (float)i;
                y_data[i] = GetYValue(vd->series[s].results[i], tp->y_source);
            }

            switch (tp->trend_style) {
            case VIZ_TREND_LINEAR:
                ComputeLinearFit(x_data, y_data, n, y_fit);
                break;
            case VIZ_TREND_MOVING_AVERAGE:
                ComputeMovingAverage(y_data, n, tp->trend_window, y_fit);
                break;
            case VIZ_TREND_POLYNOMIAL:
                ComputePolynomialFit(x_data, y_data, n, tp->trend_degree, y_fit);
                break;
            default:
                continue;
            }

            uint32 color = LightenColor(GetSeriesColor(s));
            LONG tpen = ObtainColorPen(rp, color);
            IGraphics->SetAPen(rp, tpen);

            for (uint32 i = 0; i < n; i++) {
                int dx = px + (int)((float)i * (float)pw / (float)(n > 1 ? n - 1 : 1));
                int dy = py + ph - (int)((y_fit[i] / max_y) * (float)ph);
                dx = ClampInt(dx, px, px + pw);
                dy = ClampInt(dy, py, py + ph);
                if (i > 0)
                    IGraphics->Draw(rp, dx, dy);
                IGraphics->Move(rp, dx, dy);
            }
            ReleaseColorPen(rp, tpen);
        }
    } else {
        /* Single aggregate trend line across all series */
        n = 0;
        for (uint32 s = 0; s < vd->series_count && n < 200; s++) {
            for (uint32 i = 0; i < vd->series[s].count && n < 200; i++) {
                x_data[n] = (float)n;
                y_data[n] = GetYValue(vd->series[s].results[i], tp->y_source);
                n++;
            }
        }
        if (n < 2) return;

        switch (tp->trend_style) {
        case VIZ_TREND_LINEAR:
            ComputeLinearFit(x_data, y_data, n, y_fit);
            break;
        case VIZ_TREND_MOVING_AVERAGE:
            ComputeMovingAverage(y_data, n, tp->trend_window, y_fit);
            break;
        case VIZ_TREND_POLYNOMIAL:
            ComputePolynomialFit(x_data, y_data, n, tp->trend_degree, y_fit);
            break;
        default:
            return;
        }

        uint32 color = LightenColor(0x00FFFFFF); /* White-ish for aggregate */
        LONG tpen = ObtainColorPen(rp, color);
        IGraphics->SetAPen(rp, tpen);

        for (uint32 i = 0; i < n; i++) {
            int dx = px + (int)((float)i * (float)pw / (float)(n > 1 ? n - 1 : 1));
            int dy = py + ph - (int)((y_fit[i] / max_y) * (float)ph);
            dx = ClampInt(dx, px, px + pw);
            dy = ClampInt(dy, py, py + ph);
            if (i > 0)
                IGraphics->Draw(rp, dx, dy);
            IGraphics->Move(rp, dx, dy);
        }
        ReleaseColorPen(rp, tpen);
    }
}

/* --- Legend with Wrapping --- */

/**
 * @brief Renders the graph legend with automatic text wrapping.
 *
 * Calculates the bounding box for each legend item and wraps to new rows
 * if the item exceeds the available width defined by the margins.
 */
static void RenderLegend(struct RastPort *rp, struct IBox *box, VizData *vd, int px, int py, int pw, int ph,
                         LONG text_pen)
{
    int cur_x = px;
    int cur_y = py + ph + 38; /* Start below the X-axis title */
    int max_x = px + pw;
    int row_h = 10;

    for (uint32 i = 0; i < vd->series_count; i++) {
        struct TextExtent te;
        IGraphics->TextExtent(rp, vd->series[i].label, strlen(vd->series[i].label), &te);

        /* Wrap to next row if current label overflows */
        if (cur_x + 10 + te.te_Width > max_x && cur_x > px) {
            cur_x = px;
            cur_y += row_h + 4;
        }

        LONG pen = ObtainColorPen(rp, GetSeriesColor(i));
        IGraphics->SetAPen(rp, pen);
        IGraphics->RectFill(rp, cur_x, cur_y - 6, cur_x + 6, cur_y);
        IGraphics->SetAPen(rp, text_pen);
        DrawSmallText(rp, cur_x + 10, cur_y, vd->series[i].label);

        cur_x += 10 + te.te_Width + 16;
        ReleaseColorPen(rp, pen);
    }
}

/* --- Chart Modules --- */

/**
 * @brief Renders a standard Line Chart with multi-series support.
 *
 * Supports both Scaling Profiles (X = discrete test index) and
 * Trend Profiles (X = chronological test index).
 */
static void RenderLineChart(struct RastPort *rp, struct IBox *box, VizData *vd, BOOL is_trend)
{
    int px = box->Left + MARGIN_LEFT;
    int py = box->Top + MARGIN_TOP;
    int pw = box->Width - MARGIN_LEFT - MARGIN_RIGHT;
    int ph = box->Height - MARGIN_TOP - MARGIN_BOTTOM;

    /* Get active profile for Y source and scale */
    VizProfile *lp = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    VizYSource ysrc = lp ? lp->y_source : VIZ_SRC_MB_PER_SEC;
    float max_y = vd->global_max_y1;
    if (lp && !lp->y_autoscale && lp->y_fixed_max > 0.0f)
        max_y = lp->y_fixed_max;

    LONG grid_pen = ObtainColorPen(rp, 0x00444466);
    LONG axis_pen = ObtainColorPen(rp, 0x00AAAACC);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    DrawGridAndAxes(rp, px, py, pw, ph, max_y, grid_pen, axis_pen, text_pen);
    DrawXAxisLabels(rp, px, py, pw, ph, vd, is_trend, text_pen);

    /* Plot Series */
    for (uint32 s = 0; s < vd->series_count; s++) {
        LONG spen = ObtainColorPen(rp, GetSeriesColor(s));
        IGraphics->SetAPen(rp, spen);
        int last_x = -1;

        for (uint32 i = 0; i < vd->series[s].count; i++) {
            BenchResult *res = vd->series[s].results[i];
            int dx;

            /* Calculate X position based on data point index */
            dx = px + (int)((float)i * (float)pw / (float)(vd->series[s].count > 1 ? vd->series[s].count - 1 : 1));

            /* Calculate Y position normalized to global maximum */
            float v;
            switch (ysrc) {
            case VIZ_SRC_IOPS:          v = (float)res->iops; break;
            case VIZ_SRC_MIN_MBPS:      v = res->min_mbps; break;
            case VIZ_SRC_MAX_MBPS:      v = res->max_mbps; break;
            case VIZ_SRC_DURATION_SECS: v = res->total_duration; break;
            case VIZ_SRC_TOTAL_BYTES:   v = (float)res->cumulative_bytes; break;
            default:                    v = res->mb_per_sec; break;
            }
            int dy = py + ph - (int)((v / (max_y > 0 ? max_y : 1)) * (float)ph);
            dx = ClampInt(dx, px, px + pw);
            dy = ClampInt(dy, py, py + ph);

            if (last_x != -1)
                IGraphics->Draw(rp, dx, dy);
            IGraphics->Move(rp, dx, dy);
            IGraphics->RectFill(rp, dx - 2, dy - 2, dx + 2, dy + 2);

            /* Store point for hover detail detection */
            if (plotted_count < MAX_GRAPH_POINTS) {
                plotted_points[plotted_count].x = dx;
                plotted_points[plotted_count].y = dy;
                plotted_points[plotted_count].res = res;
                plotted_count++;
            }
            last_x = dx;
        }
        ReleaseColorPen(rp, spen);
    }

    DrawAnnotations(rp, px, py, pw, ph, max_y);
    DrawTrendLines(rp, px, py, pw, ph, vd, max_y);
    RenderLegend(rp, box, vd, px, py, pw, ph, text_pen);
    ReleaseColorPen(rp, grid_pen);
    ReleaseColorPen(rp, axis_pen);
    ReleaseColorPen(rp, text_pen);
}

/**
 * @brief Renders a Grouped Bar Chart.
 *
 * Used for Battle Profiles where volumes are compared side-by-side
 * or Workload Profiles comparing performance across test types.
 */
static void RenderBarChart(struct RastPort *rp, struct IBox *box, VizData *vd, BOOL is_workload)
{
    int px = box->Left + MARGIN_LEFT;
    int py = box->Top + MARGIN_TOP;
    int pw = box->Width - MARGIN_LEFT - MARGIN_RIGHT;
    int ph = box->Height - MARGIN_TOP - MARGIN_BOTTOM;

    /* Get active profile for Y source and scale */
    VizProfile *bp = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    VizYSource bysrc = bp ? bp->y_source : VIZ_SRC_MB_PER_SEC;
    float bar_max_y = vd->global_max_y1;
    if (bp && !bp->y_autoscale && bp->y_fixed_max > 0.0f)
        bar_max_y = bp->y_fixed_max;

    LONG grid_pen = ObtainColorPen(rp, 0x00444466);
    LONG axis_pen = ObtainColorPen(rp, 0x00AAAACC);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    DrawGridAndAxes(rp, px, py, pw, ph, bar_max_y, grid_pen, axis_pen, text_pen);

    int total_bars = vd->total_points;
    int bar_pw = pw / (total_bars > 0 ? total_bars : 1);
    if (bar_pw > 40)
        bar_pw = 40;
    int cur_x = px + (pw - (total_bars * bar_pw)) / 2;

    /* Dynamic padding to prevent invalid rects on small bars */
    int pad = 2;
    if (bar_pw < 6)
        pad = 0;
    else if (bar_pw < 10)
        pad = 1;

    for (uint32 s = 0; s < vd->series_count; s++) {
        LONG spen = ObtainColorPen(rp, GetSeriesColor(s));
        IGraphics->SetAPen(rp, spen);
        for (uint32 i = 0; i < vd->series[s].count; i++) {
            BenchResult *res = vd->series[s].results[i];
            float v;
            switch (bysrc) {
            case VIZ_SRC_IOPS:          v = (float)res->iops; break;
            case VIZ_SRC_MIN_MBPS:      v = res->min_mbps; break;
            case VIZ_SRC_MAX_MBPS:      v = res->max_mbps; break;
            case VIZ_SRC_DURATION_SECS: v = res->total_duration; break;
            case VIZ_SRC_TOTAL_BYTES:   v = (float)res->cumulative_bytes; break;
            default:                    v = res->mb_per_sec; break;
            }
            int h = (int)((v / (bar_max_y > 0 ? bar_max_y : 1)) * (float)ph);
            if (h > ph) h = ph;
            if (h < 0) h = 0;

            IGraphics->RectFill(rp, cur_x + pad, py + ph - h, cur_x + bar_pw - pad, py + ph);

            if (plotted_count < MAX_GRAPH_POINTS) {
                plotted_points[plotted_count].x = cur_x + bar_pw / 2;
                plotted_points[plotted_count].y = py + ph - h / 2;
                plotted_points[plotted_count].res = res;
                plotted_count++;
            }
            cur_x += bar_pw;
        }
        ReleaseColorPen(rp, spen);
    }

    DrawAnnotations(rp, px, py, pw, ph, bar_max_y);
    DrawTrendLines(rp, px, py, pw, ph, vd, bar_max_y);
    RenderLegend(rp, box, vd, px, py, pw, ph, text_pen);
    ReleaseColorPen(rp, grid_pen);
    ReleaseColorPen(rp, axis_pen);
    ReleaseColorPen(rp, text_pen);
}

/**
 * @brief Renders a secondary Y-axis on the right side (for IOPS in Hybrid mode).
 */
static void DrawSecondaryYAxis(struct RastPort *rp, int px, int py, int pw, int ph, float max_y, LONG text_pen)
{
    IGraphics->SetAPen(rp, text_pen);
    for (int i = 0; i <= 4; i++) {
        float val = (max_y * (float)i / 4.0f);
        int ly = py + ph - (int)((float)(i * ph) / 4.0f);
        char label[32];

        /* Format IOPS as thousands if needed */
        if (val >= 1000.0f)
            snprintf(label, sizeof(label), "%.1fk", val / 1000.0f);
        else
            snprintf(label, sizeof(label), "%.0f", val);

        struct TextExtent te;
        IGraphics->TextExtent(rp, label, strlen(label), &te);
        DrawSmallText(rp, px + pw + 4, ly + 4, label);
    }
    /* Y2-Axis Title - Aligned with primary Y label (py - 12) */
    VizProfile *y2p = (ui.viz_chart_type_idx < g_viz_profile_count)
        ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
    const char *y2_title = (y2p && y2p->y2_label[0]) ? y2p->y2_label : "IOPS";
    DrawSmallText(rp, px + pw - 24, py - 12, y2_title);
}

/**
 * @brief Renders the Hybrid Diagnostic Profile (P5).
 *
 * Overlays a Bar Chart of MB/s with a Line Chart of IOPS for the first
 * filtered series, allowing for simultaneous throughput and bottleneck analysis.
 */
static void RenderHybridChart(struct RastPort *rp, struct IBox *box, VizData *vd)
{
    /* 1. Render the MB/s Background (Bars) */
    RenderBarChart(rp, box, vd, FALSE);

    /* 2. Overlay the IOPS Line on top of the bars */
    if (vd->series_count > 0) {
        int px = box->Left + MARGIN_LEFT;
        int py = box->Top + MARGIN_TOP;
        int pw = box->Width - MARGIN_LEFT - MARGIN_RIGHT;
        int ph = box->Height - MARGIN_TOP - MARGIN_BOTTOM;

        LONG line_pen = ObtainColorPen(rp, 0x00FFFFFF);
        LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

        DrawSecondaryYAxis(rp, px, py, pw, ph, vd->global_max_y2, text_pen);

        IGraphics->SetAPen(rp, line_pen);

        int total_bars = vd->total_points;
        int bar_pw = pw / (total_bars > 0 ? total_bars : 1);
        if (bar_pw > 40)
            bar_pw = 40;

        /* Reset cur_x to start for line plotting */
        int start_x = px + (pw - (total_bars * bar_pw)) / 2;
        int cur_x = start_x;

        for (uint32 s = 0; s < vd->series_count; s++) {
            LONG line_pen = ObtainColorPen(rp, GetSeriesColor(s)); /* Use series/profile color for line */
            IGraphics->SetAPen(rp, line_pen);

            int last_x = -1;

            for (uint32 i = 0; i < vd->series[s].count; i++) {
                BenchResult *res = vd->series[s].results[i];
                int dx = cur_x + bar_pw / 2;
                int dy =
                    py + ph - (int)(((float)res->iops / (vd->global_max_y2 > 0 ? vd->global_max_y2 : 1)) * (float)ph);
                dx = ClampInt(dx, px, px + pw);
                dy = ClampInt(dy, py, py + ph);

                if (last_x != -1)
                    IGraphics->Draw(rp, dx, dy);
                IGraphics->Move(rp, dx, dy);
                IGraphics->RectFill(rp, dx - 2, dy - 2, dx + 2, dy + 2);

                /* Store IOPS point for hover (overwrite bar point for better accuracy on line) */
                if (plotted_count < MAX_GRAPH_POINTS) {
                    plotted_points[plotted_count].x = dx;
                    plotted_points[plotted_count].y = dy;
                    plotted_points[plotted_count].res = res;
                    plotted_count++;
                }

                last_x = dx;
                cur_x += bar_pw;
            }
            ReleaseColorPen(rp, line_pen);
        }
        ReleaseColorPen(rp, line_pen);
        ReleaseColorPen(rp, text_pen);
    }
}

/**
 * @brief Primary entry point for graph rendering.
 *
 * Clears the background and dispatches rendering to the appropriate
 * chart module based on the user's current Chart Type selection.
 */
void RenderGraph(struct RastPort *rp, struct IBox *box, VizData *vd)
{
    plotted_count = 0;
    if (!rp || !box || !vd)
        return;

    LONG bg_pen = ObtainColorPen(rp, 0x00222233);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    IGraphics->SetAPen(rp, bg_pen);
    IGraphics->RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);

    if (vd->series_count == 0) {
        LOG_DEBUG("RenderGraph: No Data Matching Filters");
        IGraphics->SetAPen(rp, text_pen);
        const char *msg = "No Data Filtering...";
        struct TextExtent te;
        IGraphics->TextExtent(rp, msg, strlen(msg), &te);
        IGraphics->Move(rp, box->Left + (box->Width - te.te_Width) / 2, box->Top + (box->Height - te.te_Height) / 2);
        IGraphics->Text(rp, msg, strlen(msg));
    } else {
        VizProfile *rp_profile = (ui.viz_chart_type_idx < g_viz_profile_count)
            ? &g_viz_profiles[ui.viz_chart_type_idx] : NULL;
        VizChartType ctype = rp_profile ? rp_profile->chart_type : VIZ_CHART_LINE;

        LOG_DEBUG("RenderGraph: Rendering %lu series. MaxY=%.2f. Profile=%s", vd->series_count, vd->global_max_y1,
                  rp_profile ? rp_profile->name : "none");
        switch (ctype) {
        case VIZ_CHART_BAR:
            RenderBarChart(rp, box, vd, FALSE);
            break;
        case VIZ_CHART_HYBRID:
            RenderHybridChart(rp, box, vd);
            break;
        default:
            RenderLineChart(rp, box, vd, (rp_profile && rp_profile->x_source == VIZ_SRC_TIMESTAMP));
            break;
        }
    }

    ReleaseColorPen(rp, bg_pen);
    ReleaseColorPen(rp, text_pen);
}

/**
 * @brief Checks if the mouse cursor is hovering over a plotted data point.
 *
 * If a hit is detected (within a 15px radius), the Details Label in the UI
 * is updated with the benchmark metadata for that specific point.
 */
void VizCheckHover(int mx, int my)
{
    BenchResult *hit = NULL;
    /* Increased radius for easier hit detection on Amiga screens */
    for (uint32 i = 0; i < plotted_count; i++) {
        if (abs(plotted_points[i].x - mx) < 15 && abs(plotted_points[i].y - my) < 15) {
            hit = plotted_points[i].res;
            /* If multiple points are in range, we pick the one closest to the mouse */
        }
    }

    if (hit) {
        char buf[256];
        /* Sanitize underscores in volume name for display */
        char vol_display[32];
        const char *src = hit->volume_name;
        char *dst = vol_display;
        char *end = vol_display + sizeof(vol_display) - 1;
        while (*src && dst < end) {
            *dst++ = (*src == '_') ? ' ' : *src;
            src++;
        }
        *dst = '\0';
        snprintf(buf, sizeof(buf), "[%s] %s, %s, %s: %.2f MB/s (%u IOPS)", hit->timestamp, vol_display,
                 TestTypeToDisplayName(hit->type), FormatPresetBlockSize(hit->block_size), hit->mb_per_sec,
                 (unsigned int)hit->iops);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text, (uint32)buf,
                                   TAG_DONE);
    } else {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text,
                                   (uint32) "Hover over points for details...", TAG_DONE);
    }
}
