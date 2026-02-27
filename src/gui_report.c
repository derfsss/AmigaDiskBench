/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"

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
                 "Seq Read:    Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Random 4K:   Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Rnd 4K Read: Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Mixed 70/30: Avg %.2f MB/s, Max %.2f MB/s (%u runs)\n"
                 "Profiler:    Avg %.2f MB/s, Max %.2f MB/s (%u runs)",
                 (unsigned int)report.total_benchmarks, report.stats[TEST_SPRINTER].avg_mbps,
                 report.stats[TEST_SPRINTER].max_mbps, (unsigned int)report.stats[TEST_SPRINTER].total_runs,
                 report.stats[TEST_HEAVY_LIFTER].avg_mbps, report.stats[TEST_HEAVY_LIFTER].max_mbps,
                 (unsigned int)report.stats[TEST_HEAVY_LIFTER].total_runs, report.stats[TEST_LEGACY].avg_mbps,
                 report.stats[TEST_LEGACY].max_mbps, (unsigned int)report.stats[TEST_LEGACY].total_runs,
                 report.stats[TEST_DAILY_GRIND].avg_mbps, report.stats[TEST_DAILY_GRIND].max_mbps,
                 (unsigned int)report.stats[TEST_DAILY_GRIND].total_runs, report.stats[TEST_SEQUENTIAL_WRITE].avg_mbps,
                 report.stats[TEST_SEQUENTIAL_WRITE].max_mbps,
                 (unsigned int)report.stats[TEST_SEQUENTIAL_WRITE].total_runs,
                 report.stats[TEST_SEQUENTIAL_READ].avg_mbps, report.stats[TEST_SEQUENTIAL_READ].max_mbps,
                 (unsigned int)report.stats[TEST_SEQUENTIAL_READ].total_runs, report.stats[TEST_RANDOM_WRITE].avg_mbps,
                 report.stats[TEST_RANDOM_WRITE].max_mbps, (unsigned int)report.stats[TEST_RANDOM_WRITE].total_runs,
                 report.stats[TEST_RANDOM_READ].avg_mbps, report.stats[TEST_RANDOM_READ].max_mbps,
                 (unsigned int)report.stats[TEST_RANDOM_READ].total_runs, report.stats[TEST_MIXED_RW_70_30].avg_mbps,
                 report.stats[TEST_MIXED_RW_70_30].max_mbps, (unsigned int)report.stats[TEST_MIXED_RW_70_30].total_runs,
                 report.stats[TEST_PROFILER].avg_mbps, report.stats[TEST_PROFILER].max_mbps,
                 (unsigned int)report.stats[TEST_PROFILER].total_runs);
        ShowMessage("AmigaDiskBench Report", msg, "Close");
    } else {
        ShowMessage("AmigaDiskBench Error", "No historical data found or CSV error.", "OK");
    }
}
