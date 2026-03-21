/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef VERSION_H
#define VERSION_H

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define VERSION 2
#define MINOR 8
#define APP_DATE "21.03.2026"
#define APP_TITLE "AmigaDiskBench"
#define VERSION_STR STR(VERSION) "." STR(MINOR)
#define APP_VERSION_STR VERSION_STR
#define VSTRING APP_TITLE " " VERSION_STR " (" APP_DATE ")\r\n"
#define VERSTAG "\0$VER: " APP_TITLE " " VERSION_STR " (" APP_DATE ")"
#define APP_VER_TITLE APP_TITLE " v" VERSION_STR
#define APP_DESCRIPTION "Amiga Disk Benchmark Utility v" APP_VERSION_STR
#define APP_COPYRIGHT "(C) 2026 Team Derfs"
#define APP_ABOUT_MSG APP_TITLE " v" APP_VERSION_STR "\n" APP_COPYRIGHT "\nA modern benchmark for AmigaOS 4.x"

#define VER_STRING "$VER: " APP_TITLE " " VERSION_STR " (" APP_DATE ")\r\n"

#endif /* VERSION_H */
