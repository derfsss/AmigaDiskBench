/*
 * AmigaDiskBench - Benchmark Queue System
 * Handles serialization of benchmark jobs from GUI to Worker.
 */

#ifndef BENCHMARK_QUEUE_H
#define BENCHMARK_QUEUE_H

#include "gui.h"

/* Initializes the benchmark queue list */
void InitBenchmarkQueue(void);

/* Adds a job to the queue and dispatches if worker is idle */
void EnqueueBenchmarkJob(BenchJob *job);

/* Checks if the queue has pending jobs */
BOOL IsQueueEmpty(void);

/* Dispatches the next job in the queue to the worker */
void DispatchNextJob(void);

/* Frees all pending jobs in the queue */
void CleanupBenchmarkQueue(void);

#endif /* BENCHMARK_QUEUE_H */
