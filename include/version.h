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

#ifndef VERSION_H
#define VERSION_H

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define VERSION_MAJOR 2
#define VERSION_MINOR 2
#define VERSION_REV 13
#define VERSION_BUILD 1017
#define VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REV) "." STR(VERSION_BUILD)
#define APP_VERSION_STR VERSION_STR
#define APP_DATE "18.02.2026"
#define APP_TITLE "AmigaDiskBench"
#define APP_VER_TITLE APP_TITLE " v" VERSION_STR
#define APP_DESCRIPTION "Amiga Disk Benchmark Utility v" APP_VERSION_STR
#define APP_COPYRIGHT "(C) 2026 Team Derfs"
#define APP_ABOUT_MSG APP_TITLE " v" APP_VERSION_STR "\n" APP_COPYRIGHT "\nA modern benchmark for AmigaOS 4.x"

/* Amiga style $VER string */
#define VER_STRING "$VER: " APP_TITLE " " VERSION_STR " (" APP_DATE ")\r\n"

#endif /* VERSION_H */
