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

#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

#include "debug.h"
#include "gui.h"
#include "version.h"

static const char *const __attribute__((used)) ver = VER_STRING;

int main(int argc, char **argv)
{
    LOG_DEBUG("Program starting: %s %s (%s)...", APP_TITLE, APP_VERSION_STR, APP_DATE);
    int result = StartGUI();
    LOG_DEBUG("Program exiting with code %d", result);
    return result;
}
