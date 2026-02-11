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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern GUIState ui;
extern struct IntuitionIFace *IIntuition;
extern struct ExecIFace *IExec;
extern struct ListBrowserIFace *IListBrowser;

/* Helper for sorting BenchResult pointers by speed descending */
static int compare_results(const void *a, const void *b)
{
    BenchResult *ra = *(BenchResult **)a;
    BenchResult *rb = *(BenchResult **)b;
    if (ra->mb_per_sec < rb->mb_per_sec)
        return 1;
    if (ra->mb_per_sec > rb->mb_per_sec)
        return -1;
    return 0;
}

/**
 * UpdateVisualization
 *
 * Scans the history list, sorts the top results by speed, and updates
 * the FuelGauge bars and labels in the Visualization tab.
 */
void UpdateVisualization(void)
{
    /* Safety check: ensure visualization gadgets and window are valid */
    if (!ui.vis_bars[0] || !ui.window)
        return;

    LOG_DEBUG("Updating Visualization...");

    /* 1. Collect results from history list.
     * We use a fixed-size array for the top matches to avoid excessive allocation.
     */
    BenchResult *all_results[100];
    int count = 0;

    struct Node *node = IExec->GetHead(&ui.history_labels);
    while (node && count < 100) {
        BenchResult *res = NULL;
        IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &res, TAG_DONE);
        if (res) {
            all_results[count++] = res;
        }
        node = IExec->GetSucc(node);
    }

    if (count == 0) {
        /* No results to display, clear bars is handled in the loop if count < 5 */
        return;
    }

    /* 2. Sort by speed descending (fastest at index 0) */
    if (count > 1) {
        qsort(all_results, count, sizeof(BenchResult *), compare_results);
    }

    /* 3. Determine Max scale (the fastest overall) for relative bar length */
    float max_speed = all_results[0]->mb_per_sec;
    if (max_speed < 0.01f)
        max_speed = 0.01f;

    /* 4. Update the Top 5 Bars/Labels */
    for (int i = 0; i < 5; i++) {
        if (i < count) {
            BenchResult *r = all_results[i];
            char label_text[128];
            snprintf(label_text, sizeof(label_text), " %d. %s (%s) - %.2f MB/s", i + 1, r->volume_name,
                     FormatPresetBlockSize(r->block_size), r->mb_per_sec);

            IIntuition->SetGadgetAttrs((struct Gadget *)ui.vis_labels[i], ui.window, NULL, LABEL_Text,
                                       (uint32)label_text, TAG_DONE);

            /* Use a fixed scale (multiplied by 100 for integer fuelgauge) */
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.vis_bars[i], ui.window, NULL, FUELGAUGE_Max,
                                       (uint32)(max_speed * 100), FUELGAUGE_Level, (uint32)(r->mb_per_sec * 100),
                                       TAG_DONE);
        } else {
            /* Clear unused slots if there are fewer than 5 results */
            char label_text[32];
            snprintf(label_text, sizeof(label_text), " %d. N/A", i + 1);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.vis_labels[i], ui.window, NULL, LABEL_Text,
                                       (uint32)label_text, TAG_DONE);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.vis_bars[i], ui.window, NULL, FUELGAUGE_Level, 0, TAG_DONE);
        }
    }

    LOG_DEBUG("Visualization updated. Top speed: %.2f MB/s", max_speed);
}
