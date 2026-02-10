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

#ifndef VERSION_H
#define VERSION_H

#define APP_VERSION 2
#define APP_REVISION 0
#define APP_PATCH 2
#define APP_VERSION_STR "2.0.2"
#define APP_DATE "10.02.2026"
#define APP_TITLE "AmigaDiskBench"
#define APP_VER_TITLE APP_TITLE " v" APP_VERSION_STR
#define APP_DESCRIPTION "Amiga Disk Benchmark Utility v" APP_VERSION_STR
#define APP_COPYRIGHT "(C) 2026 Team Derfs"
#define APP_ABOUT_MSG APP_TITLE " v" APP_VERSION_STR "\n" APP_COPYRIGHT "\nA modern benchmark for AmigaOS 4.x"

/* Amiga style $VER string */
#define VER_STRING "$VER: " APP_TITLE " " APP_VERSION_STR " (" APP_DATE ")\r\n"

#endif /* VERSION_H */
