/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (C) 2026 Team Derfs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
                 "DailyGrind:  Avg %.2f MB/s, Max %.2f MB/s (%u runs)",
                 (unsigned int)report.total_benchmarks, report.stats[TEST_SPRINTER].avg_mbps,
                 report.stats[TEST_SPRINTER].max_mbps, (unsigned int)report.stats[TEST_SPRINTER].total_runs,
                 report.stats[TEST_HEAVY_LIFTER].avg_mbps, report.stats[TEST_HEAVY_LIFTER].max_mbps,
                 (unsigned int)report.stats[TEST_HEAVY_LIFTER].total_runs, report.stats[TEST_LEGACY].avg_mbps,
                 report.stats[TEST_LEGACY].max_mbps, (unsigned int)report.stats[TEST_LEGACY].total_runs,
                 report.stats[TEST_DAILY_GRIND].avg_mbps, report.stats[TEST_DAILY_GRIND].max_mbps,
                 (unsigned int)report.stats[TEST_DAILY_GRIND].total_runs);
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, "AmigaDiskBench Report", msg, "Close"};
        IIntuition->EasyRequest(ui.window, &es, NULL);
    } else {
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, "AmigaDiskBench Error",
                                "No historical data found or CSV error.", "OK"};
        IIntuition->EasyRequest(ui.window, &es, NULL);
    }
}
