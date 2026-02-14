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

/* Standard C library headers used by all engine modules */
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "engine.h"

/* Global library bases and interfaces shared within engine */
extern struct TimerIFace *IBenchTimer;
extern struct TimeRequest *BenchTimerReq;

/* SCSI Inquiry Constants */
#define SCSI_INQ_STD_LEN 36
#define SCSI_INQ_VPD_LEN 255
#define SCSI_CMD_INQUIRY 0x12

/* Internal prototypes */

/* Engine info helpers */

/**
 * @brief Retrieve hardware details for a volume path (Internal).
 * Wrapper around GetScsiHardwareInfo.
 *
 * @param path The path to query.
 * @param result Pointer to the BenchResult structure to populate.
 */
void GetHardwareInfo(const char *path, BenchResult *result);

/**
 * @brief Clear the internal hardware info cache.
 */
void ClearHardwareInfoCache(void);

/* Engine test implementation */

/**
 * @brief Create a dummy file of a specified size for testing.
 *
 * @param path Full path to the file to create.
 * @param size Total size of the file in bytes.
 * @param chunk_size Size of chunks to write (for buffer alignment testing).
 * @return Total bytes written, or 0 on error.
 */
uint32 WriteDummyFile(const char *path, uint32 size, uint32 chunk_size);

/**
 * @brief Get the current high-resolution system time (Microseconds).
 *
 * @param tv Pointer to a TimeVal structure to store the result.
 */
void GetMicroTime(struct TimeVal *tv);

/**
 * @brief Calculate duration between two time values in seconds.
 *
 * @param start Start time.
 * @param end End time.
 * @return Duration in seconds (with fractional part).
 */
float GetDuration(struct TimeVal *start, struct TimeVal *end);

/**
 * @brief Execute a single iteration of a benchmark test.
 *
 * @param type Test type.
 * @param target_path Path to test.
 * @param block_size Block size to use.
 * @param out_result Pointer to store the result of this single run.
 * @return TRUE if successful, FALSE on error.
 */
BOOL RunSingleBenchmark(BenchTestType type, const char *target_path, uint32 block_size, BenchResult *out_result);

/**
 * @brief Attempt to flush the disk cache for a specific path.
 *
 * @param path The path to flush.
 * @return TRUE if successful (or not applicable), FALSE on error.
 */
BOOL FlushDiskCache(const char *path);

/* Engine persistence helpers */
/* No internal prototypes currently needed, public ones are in engine.h */

#endif /* ENGINE_INTERNAL_H */
