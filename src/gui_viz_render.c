/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 */

#include "gui_internal.h"
#include <graphics/rpattr.h>
#include <proto/graphics.h>
#include <stdlib.h>

/* Graph layout constants - margins in pixels */
#define MARGIN_LEFT 60
#define MARGIN_RIGHT 16
#define MARGIN_TOP 16
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

    /* Y-Axis Title */
    DrawSmallText(rp, px - MARGIN_LEFT + 4, py - 4, "MB/s");
}

/**
 * @brief Renders X-axis labels (Block Sizes or Category names).
 */
static void DrawXAxisLabels(struct RastPort *rp, int px, int py, int pw, int ph, VizData *vd, LONG text_pen)
{
    IGraphics->SetAPen(rp, text_pen);
    int label_y = py + ph + 12;

    if (vd->series_count > 0 && vd->series[0].count > 0) {
        /* Draw First, Middle, and Last labels if space permits */
        uint32 count = vd->series[0].count;

        /* First */
        const char *label = FormatPresetBlockSize(vd->series[0].results[0]->block_size);
        DrawSmallText(rp, px, label_y, label);

        /* Last */
        if (count > 1) {
            label = FormatPresetBlockSize(vd->series[0].results[count - 1]->block_size);
            struct TextExtent te;
            IGraphics->TextExtent(rp, label, strlen(label), &te);
            DrawSmallText(rp, px + pw - te.te_Width, label_y, label);
        }

        /* Middle */
        if (count > 2) {
            label = FormatPresetBlockSize(vd->series[0].results[count / 2]->block_size);
            struct TextExtent te;
            IGraphics->TextExtent(rp, label, strlen(label), &te);
            DrawSmallText(rp, px + pw / 2 - te.te_Width / 2, label_y, label);
        }
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
    int cur_y = py + ph + 28; /* Start below the X-axis labels */
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

        LONG pen = ObtainColorPen(rp, series_colors[i % NUM_SERIES_COLORS]);
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

    LONG grid_pen = ObtainColorPen(rp, 0x00444466);
    LONG axis_pen = ObtainColorPen(rp, 0x00AAAACC);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    DrawGridAndAxes(rp, px, py, pw, ph, vd->global_max_y1, grid_pen, axis_pen, text_pen);
    DrawXAxisLabels(rp, px, py, pw, ph, vd, text_pen);

    /* Plot Series */
    for (uint32 s = 0; s < vd->series_count; s++) {
        LONG spen = ObtainColorPen(rp, series_colors[s % NUM_SERIES_COLORS]);
        IGraphics->SetAPen(rp, spen);
        int last_x = -1;

        for (uint32 i = 0; i < vd->series[s].count; i++) {
            BenchResult *res = vd->series[s].results[i];
            int dx;

            /* Calculate X position based on data point index */
            dx = px + (int)((float)i * (float)pw / (float)(vd->series[s].count > 1 ? vd->series[s].count - 1 : 1));

            /* Calculate Y position normalized to global MB/s maximum */
            float v = res->mb_per_sec;
            int dy = py + ph - (int)((v / (vd->global_max_y1 > 0 ? vd->global_max_y1 : 1)) * (float)ph);

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

    LONG grid_pen = ObtainColorPen(rp, 0x00444466);
    LONG axis_pen = ObtainColorPen(rp, 0x00AAAACC);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    DrawGridAndAxes(rp, px, py, pw, ph, vd->global_max_y1, grid_pen, axis_pen, text_pen);

    int total_bars = vd->total_points;
    int bar_pw = pw / (total_bars > 0 ? total_bars : 1);
    if (bar_pw > 40)
        bar_pw = 40;
    int cur_x = px + (pw - (total_bars * bar_pw)) / 2;

    for (uint32 s = 0; s < vd->series_count; s++) {
        LONG spen = ObtainColorPen(rp, series_colors[s % NUM_SERIES_COLORS]);
        IGraphics->SetAPen(rp, spen);
        for (uint32 i = 0; i < vd->series[s].count; i++) {
            BenchResult *res = vd->series[s].results[i];
            float v = res->mb_per_sec;
            int h = (int)((v / (vd->global_max_y1 > 0 ? vd->global_max_y1 : 1)) * (float)ph);
            IGraphics->RectFill(rp, cur_x + 2, py + ph - h, cur_x + bar_pw - 2, py + ph);

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
    /* Y2-Axis Title */
    DrawSmallText(rp, px + pw - 24, py - 4, "IOPS");
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
        int cur_x = px + (pw - (total_bars * bar_pw)) / 2;
        int last_x = -1;

        for (uint32 i = 0; i < vd->series[0].count; i++) {
            BenchResult *res = vd->series[0].results[i];
            int dx = cur_x + bar_pw / 2;
            int dy = py + ph - (int)(((float)res->iops / (vd->global_max_y2 > 0 ? vd->global_max_y2 : 1)) * (float)ph);

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
        IGraphics->SetAPen(rp, text_pen);
        const char *msg = "No Data Matching Filters";
        struct TextExtent te;
        IGraphics->TextExtent(rp, msg, strlen(msg), &te);
        IGraphics->Move(rp, box->Left + (box->Width - te.te_Width) / 2, box->Top + (box->Height - te.te_Height) / 2);
        IGraphics->Text(rp, msg, strlen(msg));
    } else {
        switch (ui.viz_chart_type_idx) {
        case 2:
        case 3:
            RenderBarChart(rp, box, vd, (ui.viz_chart_type_idx == 3));
            break;
        case 4:
            RenderHybridChart(rp, box, vd);
            break;
        default:
            RenderLineChart(rp, box, vd, (ui.viz_chart_type_idx == 1));
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
        snprintf(buf, sizeof(buf), "[%s] %s, %s, %s: %.2f MB/s (%u IOPS)", hit->timestamp, hit->volume_name,
                 TestTypeToDisplayName(hit->type), FormatPresetBlockSize(hit->block_size), hit->mb_per_sec,
                 (unsigned int)hit->iops);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text, (uint32)buf,
                                   TAG_DONE);
    } else {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text,
                                   (uint32) "Hover over points for details...", TAG_DONE);
    }
}
