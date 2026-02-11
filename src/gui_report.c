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
#include <stdio.h>

void ShowGlobalReport(void)
{
    GlobalReport report;
    if (GenerateGlobalReport(ui.csv_path, &report)) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "AmigaDiskBench Global Report\n"
                 "Total Benchmarks: %u\n\n"
                 "Sprinter:    Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "HeavyLifter: Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Legacy:      Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "DailyGrind:  Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Sequential:  Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Random 4K:   Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Profiler:    Avg %.2f MB/s, Max %.2f MB/s (%u runs)",
                 (unsigned int)report.total_benchmarks, report.stats[TEST_SPRINTER].avg_mbps,
                 report.stats[TEST_SPRINTER].max_mbps, (unsigned int)report.stats[TEST_SPRINTER].total_runs,
                 report.stats[TEST_HEAVY_LIFTER].avg_mbps, report.stats[TEST_HEAVY_LIFTER].max_mbps,
                 (unsigned int)report.stats[TEST_HEAVY_LIFTER].total_runs, report.stats[TEST_LEGACY].avg_mbps,
                 report.stats[TEST_LEGACY].max_mbps, (unsigned int)report.stats[TEST_LEGACY].total_runs,
                 report.stats[TEST_DAILY_GRIND].avg_mbps, report.stats[TEST_DAILY_GRIND].max_mbps,
                 (unsigned int)report.stats[TEST_DAILY_GRIND].total_runs, report.stats[TEST_SEQUENTIAL].avg_mbps,
                 report.stats[TEST_SEQUENTIAL].max_mbps, (unsigned int)report.stats[TEST_SEQUENTIAL].total_runs,
                 report.stats[TEST_RANDOM_4K].avg_mbps, report.stats[TEST_RANDOM_4K].max_mbps,
                 (unsigned int)report.stats[TEST_RANDOM_4K].total_runs, report.stats[TEST_PROFILER].avg_mbps,
                 report.stats[TEST_PROFILER].max_mbps, (unsigned int)report.stats[TEST_PROFILER].total_runs);
        ShowMessage("AmigaDiskBench Report", msg, "Close");
    } else {
        ShowMessage("AmigaDiskBench Error", "No historical data found or CSV error.", "OK");
    }
}
