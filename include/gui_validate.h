/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef GUI_VALIDATE_H
#define GUI_VALIDATE_H

#include <exec/types.h>
#include <exec/lists.h>

/**
 * @brief A single validation finding (error or warning) with source location.
 */
typedef struct
{
    struct Node node;
    uint32 line_number;
    char severity;       /* 'E' = error, 'W' = warning */
    char message[256];
} VizFinding;

/**
 * @brief Run the VALIDATE mode.
 * Scans all .viz files in PROGDIR:Visualizations/ and reports errors/warnings.
 * Shell: prints report to stdout.
 * Workbench: opens a standalone ReAction window with results.
 */
void RunValidation(void);

#endif /* GUI_VALIDATE_H */
