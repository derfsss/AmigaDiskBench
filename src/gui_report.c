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
