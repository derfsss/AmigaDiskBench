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

#include "engine.h"
#include "gui_internal.h"
#include <stdint.h>
#include <string.h>

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
                        status->success = RunBenchmark(job->type, job->target_path, job->num_passes, job->block_size,
                                                       job->use_trimmed_mean, job->flush_cache, &status->result);
                        status->finished = TRUE;
                        if (status->success) {
                            SaveResultToCSV(ui.csv_path, &status->result);
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
    char path[256];
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
            strncpy(path, ddata->bare_name, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        } else {
            strcpy(path, "RAM:");
        }
    } else {
        strcpy(path, "RAM:");
    }

    if (ui.pass_gad) {
        IIntuition->GetAttr(INTEGER_Number, ui.pass_gad, &passes);
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

    LOG_DEBUG("LaunchJob: path='%s', test=%u, passes=%u, block_val=%u, trimmed=%d", path, (unsigned int)test_type_idx,
              passes, block_val, (int)ui.use_trimmed_mean);

    BenchJob *job = IExec->AllocVecTags(sizeof(BenchJob), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (job) {
        job->msg_type = MSG_TYPE_JOB;
        job->type = (BenchTestType)test_type_idx;
        strncpy(job->target_path, path, sizeof(job->target_path) - 1);
        job->target_path[sizeof(job->target_path) - 1] = '\0';
        job->num_passes = passes;
        job->block_size = block_val;
        job->use_trimmed_mean = ui.use_trimmed_mean;
        job->flush_cache = ui.flush_cache;
        job->msg.mn_ReplyPort = ui.worker_reply_port;

        /* Queue the job instead of sending directly */
        EnqueueBenchmarkJob(job);
    }
}
