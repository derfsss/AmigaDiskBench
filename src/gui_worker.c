/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"
#include <stdint.h>

/* Static pointer to GUI reply port for progress callback */
static struct MsgPort *s_gui_reply_port = NULL;

/**
 * @brief Send progress update to GUI
 *
 * This function is called by the engine's progress callback to send intermediate
 * status messages during multi-pass benchmarks.
 */
static void SendProgressUpdate(const char *status_text, BOOL finished)
{
    if (!s_gui_reply_port || !status_text)
        return;

    BenchStatus *status =
        IExec->AllocVecTags(sizeof(BenchStatus), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (status) {
        status->msg_type = MSG_TYPE_STATUS;
        status->finished = finished;
        status->success = TRUE;
        snprintf(status->status_text, sizeof(status->status_text), "%s", status_text);
        IExec->PutMsg(s_gui_reply_port, &status->msg);
        LOG_DEBUG("Worker: Sent progress - %s", status_text);
    }
}

void BenchmarkWorker(void)
{
    struct Process *me = (struct Process *)IExec->FindTask(NULL);
    struct MsgPort *job_port = &me->pr_MsgPort;
    BOOL running = TRUE;

    if (!InitEngine()) {
        LOG_DEBUG("Worker FAILED to initialize engine");
        return;
    }

    LOG_DEBUG("Worker process started successfully");

    while (running) {
        IExec->WaitPort(job_port);
        BenchJob *job;
        while ((job = (BenchJob *)IExec->GetMsg(job_port))) {
            if (job) {
                LOG_DEBUG("Worker: Received Job message...");
                if (job->type == (BenchTestType)-1) {
                    running = FALSE;
                    IExec->ReplyMsg(&job->msg);
                    break; /* Exit inner loop, outer loop will terminate */
                } else {
                    BenchStatus *status = IExec->AllocVecTags(sizeof(BenchStatus), AVT_Type, MEMF_SHARED,
                                                              AVT_ClearWithValue, 0, TAG_DONE);
                    if (status) {
                        status->msg_type = MSG_TYPE_STATUS;
                        status->finished = FALSE;
                        LOG_DEBUG("Worker: Type=%d, Passes=%u, BS=%u", job->type, (unsigned int)job->num_passes,
                                  (unsigned int)job->block_size);

                        /* Store GUI reply port for progress callback */
                        s_gui_reply_port = job->msg.mn_ReplyPort;

                        status->success = RunBenchmark(job->type, job->target_path, job->num_passes, job->block_size,
                                                       job->averaging_method, job->flush_cache, SendProgressUpdate,
                                                       &status->result, &status->sample_data);
                        status->finished = TRUE;

                        /* Clear static pointer */
                        s_gui_reply_port = NULL;

                        if (status->success) {
                            SaveResultToCSV(ui.csv_path, &status->result);
                            snprintf(status->status_text, sizeof(status->status_text), "Complete");
                        } else {
                            snprintf(status->status_text, sizeof(status->status_text), "Failed");
                        }
                        IExec->PutMsg(job->msg.mn_ReplyPort, &status->msg);
                    } else {
                        /* If allocation fails, we must at least reply to the job to prevent deadlock */
                        IExec->ReplyMsg(&job->msg);
                        continue;
                    }
                }
                IExec->ReplyMsg(&job->msg);
            }
        }
    }

    LOG_DEBUG("Worker process exiting...");
    CleanupEngine();
}

void LaunchBenchmarkJob(void)
{
    char path[MAX_PATH_LEN];
    uint32 passes = 3;
    uint32 test_type_idx = TEST_SPRINTER;
    struct Node *node = NULL;

    if (ui.target_chooser) {
        IIntuition->GetAttr(CHOOSER_SelectedNode, ui.target_chooser, (uint32 *)&node);
    }
    if (ui.test_chooser) {
        IIntuition->GetAttr(CHOOSER_Selected, ui.test_chooser, &test_type_idx);
    }

    if (node) {
        struct DriveNodeData *ddata = NULL;
        IChooser->GetChooserNodeAttrs(node, CNA_UserData, &ddata, TAG_DONE);
        if (ddata && ddata->bare_name) {
            snprintf(path, sizeof(path), "%s", ddata->bare_name);
            path[sizeof(path) - 1] = '\0';
        } else {
            snprintf(path, sizeof(path), "RAM:");
        }
    } else {
        snprintf(path, sizeof(path), "RAM:");
    }

    if (ui.pass_gad) {
        IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &passes);
    }

    /* Update Visual Indicators */
    // ui.worker_busy = TRUE; /* Removed to fix deadlock */

    /* Increment total_jobs to support appending to an active queue (cumulative progress) */
    ui.total_jobs++;
    /* ui.completed_jobs preserves its value */

    // Traffic light refresh removed - handled by DispatchNextJob

    if (ui.fuel_gauge) {
        char buf[32];
        uint32 percent = 0;
        if (ui.total_jobs > 0) {
            percent = (ui.completed_jobs * 100) / ui.total_jobs;
        }
        snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)ui.completed_jobs, (unsigned long)ui.total_jobs);
        IIntuition->SetGadgetAttrs((struct Gadget *)ui.fuel_gauge, ui.window, NULL, FUELGAUGE_Level, percent,
                                   FUELGAUGE_VarArgs, (uint32)buf, TAG_DONE);
    }

    uint32 block_val = 4096;
    if (ui.block_chooser) {
        struct Node *bnode = NULL;
        IIntuition->GetAttr(CHOOSER_SelectedNode, ui.block_chooser, (uint32 *)&bnode);
        if (bnode) {
            void *ud = NULL;
            IChooser->GetChooserNodeAttrs(bnode, CNA_UserData, &ud, TAG_DONE);
            if (ud)
                block_val = (uint32)((uintptr_t)ud);
        }
    }

    /* Force Block Size to 0 (Mixed) for fixed-behavior tests */
    if (test_type_idx == TEST_DAILY_GRIND || test_type_idx == TEST_PROFILER) {
        block_val = 0;
    }

    LOG_DEBUG("LaunchJob: path='%s', test=%u, passes=%u, block_val=%u, avg_method=%u", path, (unsigned int)test_type_idx,
              passes, block_val, (unsigned int)ui.averaging_method);

    BenchJob *job = IExec->AllocVecTags(sizeof(BenchJob), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (job) {
        job->msg_type = MSG_TYPE_JOB;
        job->type = (BenchTestType)test_type_idx;
        snprintf(job->target_path, sizeof(job->target_path), "%s", path);
        job->target_path[sizeof(job->target_path) - 1] = '\0';
        job->num_passes = passes;
        job->block_size = block_val;
        job->averaging_method = ui.averaging_method;
        job->flush_cache = ui.flush_cache;
        job->msg.mn_ReplyPort = ui.worker_reply_port;

        /* Queue the job instead of sending directly */
        EnqueueBenchmarkJob(job);
    }
}
