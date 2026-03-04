/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef ENGINE_WORKLOADS_H
#define ENGINE_WORKLOADS_H

#include "workload_interface.h"

/*
 * Initialize the workload registry.
 * Registers all supported benchmarks.
 */
void InitWorkloadRegistry(void);

/*
 * Cleanup the workload registry.
 */
void CleanupWorkloadRegistry(void);

/*
 * Find a workload by its test type.
 * Returns NULL if not found.
 */
const BenchWorkload *GetWorkloadByType(BenchTestType type);

/*
 * Get the detailed description text for a test type.
 * Returns a fallback string if the workload has no detailed_info.
 */
const char *GetWorkloadDetailedInfo(BenchTestType type);

/* Workload descriptors defined in individual workload source files */
extern const BenchWorkload Workload_Legacy_Sprinter;
extern const BenchWorkload Workload_Legacy_Heavy;
extern const BenchWorkload Workload_Legacy_Legacy;
extern const BenchWorkload Workload_Legacy_Grind;
extern const BenchWorkload Workload_Sequential;
extern const BenchWorkload Workload_Random4K;
extern const BenchWorkload Workload_Profiler;
extern const BenchWorkload Workload_SequentialRead;
extern const BenchWorkload Workload_Random4KRead;
extern const BenchWorkload Workload_MixedRW;

#endif /* ENGINE_WORKLOADS_H */
