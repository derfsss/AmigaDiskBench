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

#ifndef GUI_DETAILS_WINDOW_H
#define GUI_DETAILS_WINDOW_H

#include "gui.h"

/* Opens the selectable details window for a given benchmark result */
void OpenDetailsWindow(BenchResult *res);

/* Closes the details window if open */
void CloseDetailsWindow(void);

/* Handles events for the details window */
void HandleDetailsWindowEvent(uint16 code, uint32 result);

#endif /* GUI_DETAILS_WINDOW_H */
