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

/* Internal prototypes */

/* Engine info helpers */
/* No internal prototypes currently needed, public ones are in engine.h */

/* Engine test implementation */
uint32 WriteDummyFile(const char *path, uint32 size, uint32 chunk_size);
void GetMicroTime(struct TimeVal *tv);
float GetDuration(struct TimeVal *start, struct TimeVal *end);
BOOL RunSingleBenchmark(BenchTestType type, const char *target_path, uint32 block_size, BenchResult *out_result);

/* Engine persistence helpers */
/* No internal prototypes currently needed, public ones are in engine.h */

#endif /* ENGINE_INTERNAL_H */
