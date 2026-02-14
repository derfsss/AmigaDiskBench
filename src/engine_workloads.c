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
