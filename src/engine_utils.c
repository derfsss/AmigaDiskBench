/**
 * engine_utils.c - Centralised test type utilities
 *
 * Maintains a single lookup table for test-type-to-string
 * mappings, eliminating duplication across the codebase.
 */

#include "engine_internal.h"

/* Single source of truth for test type string mappings */
static const struct
{
    BenchTestType type;
    const char *csv_name;     /* For CSV persistence / data */
    const char *display_name; /* For UI display */
    const char *match_key;    /* Substring key for fuzzy parsing */
} test_type_table[] = {
    {TEST_SPRINTER, "Sprinter", "Sprinter", "Sprinter"},
    {TEST_HEAVY_LIFTER, "HeavyLifter", "HeavyLifter", "Heavy"},
    {TEST_LEGACY, "Legacy", "Legacy", "Legacy"},
    {TEST_DAILY_GRIND, "DailyGrind", "DailyGrind", "Daily"},
    {TEST_SEQUENTIAL, "Sequential", "Sequential", "Sequential"},
    {TEST_RANDOM_4K, "Random4K", "Random 4K", "Random"},
    {TEST_PROFILER, "Profiler", "Profiler", "Profiler"},
    {TEST_SEQUENTIAL_READ, "SequentialRead", "Sequential Read", "SequentialRead"},
    {TEST_RANDOM_4K_READ, "Random4KRead", "Random 4K Read", "Random4KRead"},
    {TEST_MIXED_RW_70_30, "MixedRW70/30", "Mixed R/W 70/30", "Mixed"},
};

#define TEST_TYPE_TABLE_SIZE (sizeof(test_type_table) / sizeof(test_type_table[0]))

const char *TestTypeToString(BenchTestType type)
{
    for (uint32 i = 0; i < TEST_TYPE_TABLE_SIZE; i++) {
        if (test_type_table[i].type == type)
            return test_type_table[i].csv_name;
    }
    return "Unknown";
}

const char *TestTypeToDisplayName(BenchTestType type)
{
    for (uint32 i = 0; i < TEST_TYPE_TABLE_SIZE; i++) {
        if (test_type_table[i].type == type)
            return test_type_table[i].display_name;
    }
    return "Unknown";
}

BenchTestType StringToTestType(const char *name)
{
    if (!name)
        return TEST_COUNT;

    /* Try exact match first (fast path for CSV data) */
    for (uint32 i = 0; i < TEST_TYPE_TABLE_SIZE; i++) {
        if (strcmp(name, test_type_table[i].csv_name) == 0)
            return test_type_table[i].type;
    }

    /* Fall back to substring match (for legacy/fuzzy data) */
    for (uint32 i = 0; i < TEST_TYPE_TABLE_SIZE; i++) {
        if (strstr(name, test_type_table[i].match_key))
            return test_type_table[i].type;
    }

    return TEST_COUNT; /* Not found */
}
