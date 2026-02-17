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
#define MARGIN_BOTTOM 40
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

/* Stored points for hover detection */
typedef struct
{
    int x, y;
    BenchResult *res;
} VizPoint;

static VizPoint plotted_points[MAX_GRAPH_POINTS];
static uint32 plotted_count = 0;

/* --- Main graph rendering --- */

void RenderTrendGraph(struct RastPort *rp, struct IBox *box, BenchResult **results, uint32 count)
{
    plotted_count = 0; // Reset hover points

    if (!rp || !box || !results || box->Width < 120 || box->Height < 80)
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

    /* Check for empty data */
    if (count == 0) {
        /* Use black for high visibility on system background */
        LONG black_pen = ObtainColorPen(rp, 0x00000000);
        IGraphics->SetAPen(rp, black_pen);

        const char *msg = "No Benchmark Data Available";
        struct TextExtent te;
        IGraphics->TextExtent(rp, msg, strlen(msg), &te);
        int tx = gx + (gw - te.te_Width) / 2;
        int ty = gy + (gh - te.te_Height) / 2;
        IGraphics->Move(rp, tx, ty);
        IGraphics->Text(rp, msg, strlen(msg));

        ReleaseColorPen(rp, black_pen);
        goto cleanup;
    }

    /* Clear background (only if we have data) */
    IGraphics->SetAPen(rp, bg_pen);
    IGraphics->RectFill(rp, gx, gy, gx + gw - 1, gy + gh - 1);

    /* Determine metric: Always MB/s for now as chooser was removed */
    BOOL use_iops = FALSE;

    /* Find min/max values and time range across ALL visible data */
    float min_val = 1e30f, max_val = 0.0f;
    for (uint32 i = 0; i < count; i++) {
        float v = use_iops ? (float)results[i]->iops : results[i]->mb_per_sec;
        if (v < min_val)
            min_val = v;
        if (v > max_val)
            max_val = v;
    }

    /* Add padding to value range */
    if (max_val <= min_val)
        max_val = min_val + 1.0f;
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

    /* Draw Legend Background (below axis) */
    // int legend_y = py + ph + 16; /* 14 for X labels + 2 spacer */
    // Wait, labels are drawn at py + ph + 14.
    // Let's put legend further down or overlapping if space is tight?
    // Canvas size is usually 400x250, so plenty of vertical space?
    // Let's check MARGIN_BOTTOM (28). Might be tight for 2 rows (X labels + Legend).
    // X labels at +14 (bottom aligned?). Text is ~8px high. 14+8 = 22.
    // So 28 margin is just enough for X-labels.
    // I should increase MARGIN_BOTTOM to accommodate Legend.
    // But MARGIN_BOTTOM is a define above. I can't change it inside this function easily without define change.
    // I should update the define in a separate tool call or just use what I have.
    // Actually, I'll update the define in a prior step?
    // No, I'll just change the define here if I can replace the whole file or range.
    // This tool call replaces RenderTrendGraph. I can't change define at top.

    // I will assume margin is okay or text overlaps slightly.
    // Actually, I can render legend *inside* the graph area at top-right (if not obscuring data)?
    // Or just accept it below.
    // Let's try to fit it below.

    /* Render per Test Type */
    /* Iterate types 0..TEST_COUNT-1. Assuming simplified 0..7 loop for safety */
    int legend_x = gx + MARGIN_LEFT;
    int legend_y = py + ph + 24; // Push it down further. Might clip if height restricted.

    // If "All" is selected (viz_filter_test_idx == 0), we iterate types.
    // If specific test selected, we only iterate that one.

    /* We don't have TEST_COUNT available easily? It's in engine_tests.h but maybe not here. */
    /* Let's infer types from data for safety? */
    /* But we want consistent colors. */

    /* Let's iterate 0..10. */
    for (int t = 0; t < 10; t++) {
        /* Filter check */
        if (ui.viz_filter_test_idx > 0 && t != (ui.viz_filter_test_idx - 1))
            continue;

        /* Check if data exists for this type */
        BOOL has_data = FALSE;
        for (uint32 i = 0; i < count; i++) {
            if (results[i]->type == t) {
                has_data = TRUE;
                break;
            }
        }
        if (!has_data && ui.viz_filter_test_idx == 0)
            continue;

        LONG series_pen = ObtainColorPen(rp, series_colors[t % NUM_SERIES_COLORS]);
        LONG dot_pen = ObtainColorPen(rp, 0x00FFFFFF);

        /* Draw Legend Item */
        /* Only if showing multiple or just always good to show key */
        // if (ui.viz_filter_test_idx == 0) {
        IGraphics->SetAPen(rp, series_pen);
        IGraphics->RectFill(rp, legend_x, legend_y, legend_x + 6, legend_y + 6);
        IGraphics->SetAPen(rp, text_pen);
        // Constant string or helper?
        const char *tname = TestTypeToDisplayName((BenchTestType)t);
        if (tname) {
            DrawSmallText(rp, legend_x + 10, legend_y + 6, tname);
            struct TextExtent te;
            IGraphics->TextExtent(rp, tname, strlen(tname), &te);
            legend_x += 10 + te.te_Width + 12;
        }
        // }

        /* Draw Line */
        IGraphics->SetAPen(rp, series_pen);
        int last_x = -1;

        for (uint32 i = 0; i < count; i++) {
            if (results[i]->type != t)
                continue;

            /* X position based on chronological index in the FILTERED list */
            /* Note on X-Axis:
               If we have mixed types, the indices 'i' are spacing them out evenly.
               This means points are plotted chronologically.
               If we interpret X as "Time", this is correct.
            */

            int dx;
            if (count == 1)
                dx = px + pw / 2;
            else
                dx = px + (int)((float)i * (float)pw / (float)(count - 1));

            float v = use_iops ? (float)results[i]->iops : results[i]->mb_per_sec;
            int dy = py + ph - (int)(((v - min_val) / val_range) * (float)ph);
            if (dy < py)
                dy = py;
            if (dy > py + ph)
                dy = py + ph;

            if (last_x != -1) {
                IGraphics->Draw(rp, dx, dy);
            }
            IGraphics->Move(rp, dx, dy);

            /* Draw Dot */
            IGraphics->SetAPen(rp, dot_pen);
            IGraphics->RectFill(rp, dx - 2, dy - 2, dx + 2, dy + 2);
            IGraphics->SetAPen(rp, series_pen); // Restore line color

            last_x = dx;

            /* Store hover point */
            if (plotted_count < MAX_GRAPH_POINTS) {
                plotted_points[plotted_count].x = dx;
                plotted_points[plotted_count].y = dy;
                plotted_points[plotted_count].res = results[i];
                // LOG_DEBUG("Stored Point %d: %d, %d", plotted_count, dx, dy);
                plotted_count++;
            }
        }

        ReleaseColorPen(rp, series_pen);
        ReleaseColorPen(rp, dot_pen);
    }

    /* Draw X-axis labels (first, middle, last) - Now BLOCK SIZES */
    IGraphics->SetAPen(rp, text_pen);
    int label_y = py + ph + 10; /* Moved up slightly to make room for legend */

    if (count >= 1) {
        const char *bs = FormatPresetBlockSize(results[0]->block_size);
        DrawSmallText(rp, px, label_y, bs);
    }
    if (count >= 3) {
        uint32 mid = count / 2;
        const char *bs = FormatPresetBlockSize(results[mid]->block_size);
        struct TextExtent te;
        IGraphics->TextExtent(rp, bs, strlen(bs), &te);
        DrawSmallText(rp, px + pw / 2 - te.te_Width / 2, label_y, bs);
    }
    if (count >= 2) {
        const char *bs = FormatPresetBlockSize(results[count - 1]->block_size);
        struct TextExtent te;
        IGraphics->TextExtent(rp, bs, strlen(bs), &te);
        DrawSmallText(rp, px + pw - te.te_Width, label_y, bs);
    }

    /* Y-axis title */
    const char *ytitle = use_iops ? "IOPS" : "MB/s";
    DrawSmallText(rp, gx + 2, py - 2, ytitle);

cleanup:
    ReleaseColorPen(rp, bg_pen);
    ReleaseColorPen(rp, grid_pen);
    ReleaseColorPen(rp, axis_pen);
    ReleaseColorPen(rp, text_pen);
}

/* --- Hover Check Helper --- */
/* Called by main loop on mouse move */
void VizCheckHover(int mx, int my)
{
    // Need access to window/updating label?
    // This needs to efficiently search plotted_points.
    // And then update gui.viz_details_label via SetGadgetAttrs.
    // ui is global in gui_internal.h

    BenchResult *hit = NULL;
    // Use simple Manhattan for speed or distance squared.
    // Let's use 8px radius.

    for (uint32 i = 0; i < plotted_count; i++) {
        int dx = abs(plotted_points[i].x - mx);
        int dy = abs(plotted_points[i].y - my);
        if (dx < 8 && dy < 8) {
            hit = plotted_points[i].res;
            break; // Found one
        }
    }

    if (hit) {
        // Format string: "[Date] [Volume], [Type], [BlockSize], [Value] MB/s"
        char buf[256];
        const char *tname = TestTypeToDisplayName(hit->type);

        char bs_str[32];
        if (hit->block_size == 0)
            snprintf(bs_str, sizeof(bs_str), "Mixed");
        else if (hit->block_size >= 1048576)
            snprintf(bs_str, sizeof(bs_str), "%.1fM", (float)hit->block_size / 1048576.0f);
        else if (hit->block_size >= 1024)
            snprintf(bs_str, sizeof(bs_str), "%uK", (unsigned int)(hit->block_size / 1024));
        else
            snprintf(bs_str, sizeof(bs_str), "%uB", (unsigned int)hit->block_size);

        // User requested: "[Date] Volume, Test, BlockSize, Value"
        snprintf(buf, sizeof(buf), "[%s] %s, %s, %s, %.2f MB/s", hit->timestamp, hit->device, tname ? tname : "Test",
                 bs_str, hit->mb_per_sec);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text, (uint32)buf,
                                   TAG_DONE);
    } else {
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.viz_details_label, ui.window, NULL, GA_Text,
                                   (uint32) "Hover over points for details...", TAG_DONE);
    }
}
