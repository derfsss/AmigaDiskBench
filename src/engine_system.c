/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "engine_internal.h"

BOOL FlushDiskCache(const char *path)
{
    if (!path || !path[0])
        return FALSE;

    LOG_DEBUG("Flushing volume cache for %s...", path);

    /* Modern OS4 approach (dos.library 53.58+) */
    if (IDOS->FlushVolume(path)) {
        LOG_DEBUG("FlushVolume() succeeded.");
        return TRUE;
    }

    LOG_DEBUG("FlushVolume failed. Filesystem may not support explicit flush.");
    return FALSE;
}
