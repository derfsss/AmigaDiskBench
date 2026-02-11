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

#ifndef ENGINE_INTERNAL_H
#define ENGINE_INTERNAL_H

#include <devices/timer.h>
#include <dos/dos.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include "debug.h"
#include "engine.h"
#include "version.h"

/* Global library bases and interfaces shared within engine */
extern struct TimerIFace *IBenchTimer;
extern struct TimeRequest *BenchTimerReq;

/* SCSI Inquiry Constants */
#define SCSI_INQ_STD_LEN 36
#define SCSI_INQ_VPD_LEN 255
#define SCSI_CMD_INQUIRY 0x12

/* Internal prototypes */

/* Engine info helpers */
/* No internal prototypes currently needed, public ones are in engine.h */

/* Engine test implementation */
uint32 WriteDummyFile(const char *path, uint32 size, uint32 chunk_size);
void GetMicroTime(struct TimeVal *tv);
float GetDuration(struct TimeVal *start, struct TimeVal *end);
BOOL RunSingleBenchmark(BenchTestType type, const char *target_path, uint32 block_size, BenchResult *out_result);
BOOL FlushDiskCache(const char *path);

/* Engine persistence helpers */
/* No internal prototypes currently needed, public ones are in engine.h */

#endif /* ENGINE_INTERNAL_H */
