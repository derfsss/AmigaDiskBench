/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <proto/exec.h>

/* Global Debug Switch */
#define DEBUG_ENABLED 0

/* Debug Macro - Filtered to diskinfo modules */
#if DEBUG_ENABLED
#include <string.h>
#define LOG_DEBUG(fmt, ...)                                                                                            \
    do {                                                                                                               \
        IExec->DebugPrintF("[ADB] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                               \
    } while (0)
#else
#define LOG_DEBUG(fmt, ...)                                                                                            \
    do {                                                                                                               \
    } while (0)
#endif

#endif /* DEBUG_H */
