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
#include <graphics/rpattr.h>
#include <proto/graphics.h>
#include <stdlib.h>

/* Graph layout constants - margins in pixels */
#define MARGIN_LEFT 60
#define MARGIN_RIGHT 16
#define MARGIN_TOP 16
#define MARGIN_BOTTOM 28
#define TICK_LEN 4
#define MAX_GRAPH_POINTS 200

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
};
#define NUM_SERIES_COLORS (sizeof(series_colors) / sizeof(series_colors[0]))

/* --- Pen management using ObtainBestPen --- */

static LONG ObtainColorPen(struct RastPort *rp, uint32 argb)
{
    struct ColorMap *cm = NULL;

    /* Use the window's screen colormap if available (safest) */
    if (ui.window && ui.window->WScreen) {
        cm = ui.window->WScreen->ViewPort.ColorMap;
    }

    if (!cm) {
        return 1; /* TextPen fallback */
    }

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

/* --- Main graph rendering --- */

void RenderTrendGraph(struct RastPort *rp, struct IBox *box, BenchResult **results, uint32 count)
{
    if (!rp || !box || box->Width < 120 || box->Height < 80)
        return;

    int gx = box->Left;
    int gy = box->Top;
    int gw = box->Width;
    int gh = box->Height;

    /* Plot area within margins */
    int px = gx + MARGIN_LEFT;
    int py = gy + MARGIN_TOP;
    int pw = gw - MARGIN_LEFT - MARGIN_RIGHT;
    int ph = gh - MARGIN_TOP - MARGIN_BOTTOM;

    if (pw < 40 || ph < 40)
        return;

    /* Obtain pens */
    LONG bg_pen = ObtainColorPen(rp, 0x00222233);
    LONG grid_pen = ObtainColorPen(rp, 0x00444466);
    LONG axis_pen = ObtainColorPen(rp, 0x00AAAACC);
    LONG text_pen = ObtainColorPen(rp, 0x00CCCCDD);

    /* Clear background */
    IGraphics->SetAPen(rp, bg_pen);
    IGraphics->RectFill(rp, gx, gy, gx + gw - 1, gy + gh - 1);

    /* Draw "No Data" message if empty */
    if (count == 0) {
        IGraphics->SetAPen(rp, text_pen);
        DrawSmallText(rp, px + pw / 2 - 30, py + ph / 2, "No data");
        goto cleanup;
    }

    /* Determine metric: use viz_filter_metric_idx (0=MB/s, 1=IOPS) */
    BOOL use_iops = (ui.viz_filter_metric_idx == 1);

    /* Find min/max values and time range */
    float min_val = 1e30f, max_val = 0.0f;
    for (uint32 i = 0; i < count; i++) {
        float v = use_iops ? (float)results[i]->iops : results[i]->mb_per_sec;
        if (v < min_val)
            min_val = v;
        if (v > max_val)
            max_val = v;
    }

    /* Add padding to value range */
    if (max_val <= min_val) {
        max_val = min_val + 1.0f;
    }
    float range = max_val - min_val;
    min_val -= range * 0.05f;
    if (min_val < 0.0f)
        min_val = 0.0f;
    max_val += range * 0.05f;
    float val_range = max_val - min_val;
    if (val_range < 0.01f)
        val_range = 0.01f;

    /* Draw horizontal grid lines (5 lines) */
    IGraphics->SetAPen(rp, grid_pen);
    for (int i = 0; i <= 4; i++) {
        int ly = py + ph - (int)((float)(i * ph) / 4.0f);
        DrawDashedHLine(rp, px, px + pw - 1, ly, 4);
    }

    /* Draw Y-axis labels */
    IGraphics->SetAPen(rp, text_pen);
    for (int i = 0; i <= 4; i++) {
        float val = min_val + (val_range * (float)i / 4.0f);
        int ly = py + ph - (int)((float)(i * ph) / 4.0f);
        char label[32];
        if (use_iops) {
            if (val >= 1000.0f)
                snprintf(label, sizeof(label), "%.0fK", val / 1000.0f);
            else
                snprintf(label, sizeof(label), "%.0f", val);
        } else {
            snprintf(label, sizeof(label), "%.1f", val);
        }
        /* Right-align label in margin */
        struct TextExtent te;
        IGraphics->TextExtent(rp, label, strlen(label), &te);
        DrawSmallText(rp, px - te.te_Width - 4, ly + 4, label);
    }

    /* Draw axes */
    IGraphics->SetAPen(rp, axis_pen);
    IGraphics->Move(rp, px, py);
    IGraphics->Draw(rp, px, py + ph);
    IGraphics->Draw(rp, px + pw, py + ph);

    /* Plot data points as connected lines */
    LONG data_pen = ObtainColorPen(rp, series_colors[0]);
    LONG dot_pen = ObtainColorPen(rp, 0x00FFFFFF);
    IGraphics->SetAPen(rp, data_pen);

    for (uint32 i = 0; i < count; i++) {
        float v = use_iops ? (float)results[i]->iops : results[i]->mb_per_sec;
        /* X position: evenly spaced points */
        int dx;
        if (count == 1) {
            dx = px + pw / 2;
        } else {
            dx = px + (int)((float)i * (float)pw / (float)(count - 1));
        }
        /* Y position: scaled to plot area */
        int dy = py + ph - (int)(((v - min_val) / val_range) * (float)ph);
        if (dy < py)
            dy = py;
        if (dy > py + ph)
            dy = py + ph;

        if (i == 0) {
            IGraphics->Move(rp, dx, dy);
        } else {
            IGraphics->Draw(rp, dx, dy);
        }

        /* Draw data point dot */
        IGraphics->SetAPen(rp, dot_pen);
        IGraphics->RectFill(rp, dx - 2, dy - 2, dx + 2, dy + 2);
        IGraphics->SetAPen(rp, data_pen);
    }

    /* Draw X-axis labels (show first, middle, last timestamps) */
    IGraphics->SetAPen(rp, text_pen);
    int label_y = py + ph + 14;
    if (count >= 1) {
        /* First */
        char *ts = results[0]->timestamp;
        DrawSmallText(rp, px, label_y, ts);
    }
    if (count >= 3) {
        /* Middle */
        uint32 mid = count / 2;
        char *ts = results[mid]->timestamp;
        struct TextExtent te;
        IGraphics->TextExtent(rp, ts, strlen(ts), &te);
        int mx = px + pw / 2 - te.te_Width / 2;
        DrawSmallText(rp, mx, label_y, ts);
    }
    if (count >= 2) {
        /* Last */
        char *ts = results[count - 1]->timestamp;
        struct TextExtent te;
        IGraphics->TextExtent(rp, ts, strlen(ts), &te);
        DrawSmallText(rp, px + pw - te.te_Width, label_y, ts);
    }

    /* Y-axis title */
    const char *ytitle = use_iops ? "IOPS" : "MB/s";
    DrawSmallText(rp, gx + 2, py - 2, ytitle);

    ReleaseColorPen(rp, data_pen);
    ReleaseColorPen(rp, dot_pen);

cleanup:
    ReleaseColorPen(rp, bg_pen);
    ReleaseColorPen(rp, grid_pen);
    ReleaseColorPen(rp, axis_pen);
    ReleaseColorPen(rp, text_pen);
}
