/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

#include "amiupdate.h"
#include "debug.h"
#include "gui.h"

static const char *const __attribute__((used)) ver = VER_STRING;

int main(int argc, char **argv)
{
    LOG_DEBUG("Program starting: %s %s (%s)...", APP_TITLE, APP_VERSION_STR, APP_DATE);
    SetAmiUpdateENVVariable("AmigaDiskBench");
    int result = StartGUI();
    LOG_DEBUG("Program exiting with code %d", result);
    return result;
}
