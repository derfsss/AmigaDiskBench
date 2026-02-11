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

#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

/* Standardized ReAction and AmigaOS 4 header inclusion for modularized GUI */

#include <dos/dos.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <libraries/application.h>

/* ReAction configuration flags MUST come before reaction.h */
#ifndef ALL_REACTION_CLASSES
#define ALL_REACTION_CLASSES
#endif
#ifndef ALL_REACTION_MACROS
#define ALL_REACTION_MACROS
#endif

#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

/* OS Interface and Protocol Headers */
#include <proto/application.h>
#include <proto/asl.h>
#include <proto/checkbox.h>
#include <proto/chooser.h>
#include <proto/clicktab.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/integer.h>
#include <proto/intuition.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/locale.h>
#include <proto/popupmenu.h>
#include <proto/scroller.h>
#include <proto/string.h>
#include <proto/texteditor.h>

/* Specific Class headers for tags and constants (e.g. WM_OPEN, CNA_Text) */
#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/checkbox.h>
#include <gadgets/chooser.h>
#include <gadgets/clicktab.h>
#include <gadgets/integer.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/scroller.h>
#include <gadgets/string.h>
#include <gadgets/texteditor.h>
#include <images/label.h>

#include "debug.h"
#include "gui.h"
#include "version.h"

/* Data structure for drive selection nodes */
struct DriveNodeData
{
    char *bare_name;
    char *display_name;
};
typedef struct DriveNodeData DriveNodeData;

/* Global UI state provided by gui.c */
extern GUIState ui;

/* Default Preferences */
#define DEFAULT_CSV_PATH "PROGDIR:bench_history.csv"
#define DEFAULT_LAST_TEST 3      /* 3 = All Tests (default) */
#define DEFAULT_BLOCK_SIZE_IDX 0 /* 0 = 4K (default) */
#define DEFAULT_PASSES 3
#define DEFAULT_TRIMMED_MEAN TRUE

/* Prototypes for functions split out of gui.c */

/* [gui_utils.c] - Formatting and Localization Helpers */
const char *FormatPresetBlockSize(uint32 bytes);
const char *FormatByteSize(uint64 bytes);
void FormatSize(uint64 bytes, char *out);
CONST_STRPTR GetString(uint32 id, CONST_STRPTR default_str);
void ShowMessage(const char *title, const char *body, const char *gadgets);
void SetGadgetState(uint16 gid, BOOL disabled);
BOOL ShowConfirm(const char *title, const char *body, const char *gadgets);

/* [gui_system.c] - OS and System Info */
void RefreshDriveList(void);
BOOL InitSystemResources(void);
void CleanupSystemResources(void);

/* [gui_history.c] - CSV History Management */
void RefreshHistory(void);
BOOL FindMatchingResult(BenchResult *current, BenchResult *out_prev);

/* [gui_prefs.c] - Preferences Management */
void LoadPrefs(void);
void BrowseCSV(void);
void OpenPrefsWindow(void);
void UpdatePreferences(void);

/* [gui_layout.c] - UI Layout Construction */
Object *CreateMainLayout(struct DiskObject *icon, struct List *tab_list);

/* [gui_events.c] - Event Handlers */
void HandleWorkerReply(struct Message *m);
void HandleWorkbenchMessage(struct ApplicationMsg *amsg, BOOL *running);
void HandleGUIEvent(uint32 result, uint16 code, BOOL *running);
void HandlePrefsEvent(uint32 result, uint16 code);

/* [gui_worker.c] - Worker Thread */
void BenchmarkWorker(void);
void LaunchBenchmarkJob(void);

/* [gui_report.c] - Global Reports */
void ShowGlobalReport(void);

/* [gui_viz.c] - Visualizations */
void UpdateVisualization(void);

/* [gui_bulk.c] - Bulk Testing */
void RefreshBulkList(void);
void LaunchBulkJobs(void);

/* [gui_details_window.c] - Details Window management */
void ShowBenchmarkDetails(Object *list_obj);
void OpenDetailsWindow(BenchResult *res);
void CloseDetailsWindow(void);
void HandleDetailsWindowEvent(uint16 code, uint32 result);

#endif /* GUI_INTERNAL_H */
