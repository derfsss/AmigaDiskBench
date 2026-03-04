/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_workloads.h"
#include "engine_internal.h"

/* Forward declaration */
void RegisterWorkload(const BenchWorkload *workload);

/* Maximum number of registered workloads */
#define MAX_WORKLOADS 16

static const BenchWorkload *WorkloadRegistry[MAX_WORKLOADS];
static uint32 WorkloadCount = 0;

void InitWorkloadRegistry(void)
{
    LOG_DEBUG("Initializing workload registry...");
    WorkloadCount = 0;
    memset(WorkloadRegistry, 0, sizeof(WorkloadRegistry));

    /* Register legacy workloads */
    RegisterWorkload(&Workload_Legacy_Sprinter);
    RegisterWorkload(&Workload_Legacy_Heavy);
    RegisterWorkload(&Workload_Legacy_Legacy);
    RegisterWorkload(&Workload_Legacy_Grind);
    RegisterWorkload(&Workload_Sequential);
    RegisterWorkload(&Workload_Random4K);
    RegisterWorkload(&Workload_Profiler);

    /* Register new read and mixed workloads */
    RegisterWorkload(&Workload_SequentialRead);
    RegisterWorkload(&Workload_Random4KRead);
    RegisterWorkload(&Workload_MixedRW);
}

void CleanupWorkloadRegistry(void)
{
    LOG_DEBUG("Cleaning up workload registry...");
    WorkloadCount = 0;
}

const BenchWorkload *GetWorkloadByType(BenchTestType type)
{
    for (uint32 i = 0; i < WorkloadCount; i++) {
        if (WorkloadRegistry[i]->type == type) {
            return WorkloadRegistry[i];
        }
    }
    return NULL;
}

const char *GetWorkloadDetailedInfo(BenchTestType type)
{
    const BenchWorkload *w = GetWorkloadByType(type);
    if (w && w->detailed_info)
        return w->detailed_info;
    return "No description available.";
}

/* Internal helper for Phase 2+ */
void RegisterWorkload(const BenchWorkload *workload)
{
    if (WorkloadCount < MAX_WORKLOADS) {
        WorkloadRegistry[WorkloadCount++] = workload;
        LOG_DEBUG("Registered workload: %s", workload->name);
    } else {
        LOG_DEBUG("FAILED to register workload: Registry full");
    }
}
