/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (C) 2026 Team Derfs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
        BenchJob *job = (BenchJob *)IExec->GetMsg(job_port);
        if (job) {
            LOG_DEBUG("Worker: Received Job message...");
            if (job->type == (BenchTestType)-1) {
                running = FALSE;
            } else {
                BenchStatus *status =
                    IExec->AllocVecTags(sizeof(BenchStatus), AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
                if (status) {
                    status->msg_type = MSG_TYPE_STATUS;
                    status->finished = FALSE;
                    LOG_DEBUG("Worker: Type=%d, Passes=%u, BS=%u", job->type, (unsigned int)job->num_passes,
                              (unsigned int)job->block_size);
                    status->success = RunBenchmark(job->type, job->target_path, job->num_passes, job->block_size,
                                                   job->use_trimmed_mean, &status->result);
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
        job->msg.mn_ReplyPort = ui.worker_reply_port;

        IIntuition->SetGadgetAttrs((struct Gadget *)ui.run_button, ui.window, NULL, GA_Disabled, TRUE, TAG_DONE);
        IExec->PutMsg(ui.worker_port, &job->msg);
    }
}
