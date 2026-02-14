/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * Engine Warmup Module
 */

#ifndef ENGINE_WARMUP_H
#define ENGINE_WARMUP_H

#include <exec/types.h>

/*
 * RunWarmup
 *
 * Performs a quick write/read/delete cycle on a temporary file
 * in the target directory to wake up the drive and I/O subsystem.
 *
 * @param target_path: Directory/Volume to perform warmup on.
 */
void RunWarmup(const char *target_path);

#endif /* ENGINE_WARMUP_H */
