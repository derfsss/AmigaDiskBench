/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef WORKLOAD_INTERFACE_H
#define WORKLOAD_INTERFACE_H

#include "engine.h"
#include <exec/types.h>

/*
 * Workload Lifecycle Hooks
 *
 * Setup: Called before measurement starts. 'data' is a pointer to private workload data.
 * Run: The timed portion of the benchmark.
 * Cleanup: Called after measurement ends (even on failure).
 */

typedef struct
{
    BenchTestType type;
    const char *name;
    const char *description;
    const char *detailed_info; /* Paragraph-length explanation for user popup */

    /* Lifecycle hooks */
    BOOL (*Setup)(const char *path, uint32 block_size, void **data);
    BOOL (*Run)(void *data, uint32 *bytes_processed, uint32 *op_count);
    void (*Cleanup)(void *data);

    /* Metadata hooks */
    void (*GetDefaultSettings)(uint32 *block_size, uint32 *passes);
} BenchWorkload;

#endif /* WORKLOAD_INTERFACE_H */
