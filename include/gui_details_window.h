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
