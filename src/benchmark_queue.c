/*
 * AmigaDiskBench - Benchmark Queue System
 * Handles serialization of benchmark jobs from GUI to Worker.
 */

#include "benchmark_queue.h"
#include "gui_internal.h"
#include <proto/exec.h>
#include <string.h>

/**
 * Node structure for the queue
 * Just wraps the BenchJob message
 */
struct BenchJobNode
{
    struct Node node;
    BenchJob *job;
};

void InitBenchmarkQueue(void)
{
    IExec->NewList(&ui.benchmark_queue);
    ui.worker_busy = FALSE;
    LOG_DEBUG("BenchmarkQueue: Initialized");
}

void EnqueueBenchmarkJob(BenchJob *job)
{
    if (!job)
        return;

    /* Create a list node to hold the job */
    struct BenchJobNode *qn =
        IExec->AllocVecTags(sizeof(struct BenchJobNode), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (!qn) {
        LOG_DEBUG("BenchmarkQueue: Failed to allocate queue node!");
        /* Fallback: If we can't queue, try to send directly if idle, or fail */
        if (!ui.worker_busy) {
            LOG_DEBUG("BenchmarkQueue: Sending directly (emergency fallback)");
            ui.worker_busy = TRUE;
            SetGadgetState(GID_RUN_ALL, TRUE);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                       (uint32) "[ BUSY ]", TAG_DONE);
            IExec->PutMsg(ui.worker_port, &job->msg);
        } else {
            LOG_DEBUG("BenchmarkQueue: Dropping job due to OOM");
            IExec->FreeVec(job);
        }
        return;
    }

    qn->job = job;
    qn->node.ln_Name = "BenchJobNode";

    IExec->AddTail(&ui.benchmark_queue, (struct Node *)qn);
    LOG_DEBUG("BenchmarkQueue: Enqueued job for '%s'", job->target_path);

    /* If worker is idle, dispatch immediately */
    if (!ui.worker_busy) {
        DispatchNextJob();
    }
}

void DispatchNextJob(void)
{
    if (IsListEmpty(&ui.benchmark_queue)) {
        LOG_DEBUG("BenchmarkQueue: Queue empty");
        ui.worker_busy = FALSE;
        return;
    }

    /* Get head of queue */
    struct BenchJobNode *qn = (struct BenchJobNode *)IExec->RemHead(&ui.benchmark_queue);
    if (qn) {
        BenchJob *job = qn->job;

        /* We are about to send the job message, so we don't free the job structure yet.
           We ONLY free the list node wrapper. */
        IExec->FreeVec(qn);

        if (job) {
            LOG_DEBUG("BenchmarkQueue: Dispatching job for '%s'", job->target_path);

            ui.worker_busy = TRUE;

            /* Ensure UI reflects busy state */
            SetGadgetState(GID_RUN_ALL, TRUE);
            IIntuition->SetGadgetAttrs((struct Gadget *)ui.status_light_obj, ui.window, NULL, LABEL_Text,
                                       (uint32) "[ BUSY ]", TAG_DONE);

            IExec->PutMsg(ui.worker_port, &job->msg);
        }
    }
}

BOOL IsQueueEmpty(void)
{
    return IsListEmpty(&ui.benchmark_queue);
}

void CleanupBenchmarkQueue(void)
{
    struct BenchJobNode *qn;
    while ((qn = (struct BenchJobNode *)IExec->RemHead(&ui.benchmark_queue))) {
        /* Free the job payload */
        if (qn->job) {
            IExec->FreeVec(qn->job);
        }
        /* Free the node wrapper */
        IExec->FreeVec(qn);
    }
    LOG_DEBUG("BenchmarkQueue: Cleaned up");
}
