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
#include <proto/graphics.h>
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
#include <proto/utility.h>

#include <proto/utility.h>

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
#include <gadgets/space.h>
#include <gadgets/string.h>
#include <gadgets/texteditor.h>
#include <images/label.h>

/* Standard C library headers used by all GUI modules */
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "gui.h"

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

/**
 * @brief Format a block size in bytes (e.g., 4096) to a label (e.g., "4KB").
 */
const char *FormatPresetBlockSize(uint32 bytes);

/**
 * @brief Format a large byte count to a human-readable string (e.g., "1.5 GB").
 */
const char *FormatByteSize(uint64 bytes);

/**
 * @brief Thread-safe formatted size helper.
 * @param bytes Size in bytes.
 * @param out Output buffer (must be at least 32 bytes).
 * @param out_size Size of the output buffer in bytes.
 */
void FormatSize(uint64 bytes, char *out, uint32 out_size);
/**
 * @brief Retrieve a localized string or fallback default.
 * @param id The string ID.
 * @param default_str The default string to return if catalog is missing.
 */
CONST_STRPTR GetString(uint32 id, CONST_STRPTR default_str);

/**
 * @brief Display a standard AmigaOS requester message.
 */
void ShowMessage(const char *title, const char *body, const char *gadgets);

/**
 * @brief Enable or disable a gadget by ID.
 * @param gid The gadget ID.
 * @param disabled TRUE to disable, FALSE to enable.
 */
void SetGadgetState(uint16 gid, BOOL disabled);

/**
 * @brief Display a confirmation requester.
 * @return TRUE if affirmative action selected.
 */
BOOL ShowConfirm(const char *title, const char *body, const char *gadgets);

/* [gui_system.c] - OS and System Info */

/**
 * @brief Refresh the list of available drives/volumes in the chooser.
 */
void RefreshDriveList(void);

/**
 * @brief Allocate system resources (Icons, AppLibrary, etc.).
 * @return TRUE on success.
 */
BOOL InitSystemResources(void);

/**
 * @brief Free system resources.
 */
void CleanupSystemResources(void);

/**
 * @brief Update the volume information labels based on the selected drive.
 * @param volume The volume name (e.g., "DH0:").
 */
void UpdateVolumeInfo(const char *volume);

/* [gui_history.c] - CSV History Management */

/**
 * @brief Reload history from CSV and update the listbrowser.
 */
void RefreshHistory(void);

/**
 * @brief Check if a result already exists in the history (by ID).
 * @param current The result to check.
 * @param out_prev [Optional] Pointer to store the found previous result.
 * @return TRUE if found.
 */
BOOL FindMatchInList(struct List *list, BenchResult *current, BenchResult *out_prev, BOOL reverse);
BOOL FindMatchingResult(BenchResult *current, BenchResult *out_prev);
void DeleteSelectedHistoryItems(void);
void ClearHistory(void);
void ExportHistoryToCSV(const char *filename);
void DeselectAllHistoryItems(void);
void ClearBenchmarkList(void);

/* [gui_prefs.c] - Preferences Management */

/**
 * @brief Load application preferences from ENV/ENVARC.
 */
void LoadPrefs(void);

/**
 * @brief Open the ASL file requester to browse for a CSV file.
 */
void BrowseCSV(void);

/**
 * @brief Open the Preferences window.
 */
void OpenPrefsWindow(void);

/**
 * @brief Apply changes from the Preferences window and save them.
 */
void UpdatePreferences(void);

/* [gui_layout.c] - UI Layout Construction */

/**
 * @brief Create the main application window layout.
 * @param icon The application disk object (icon).
 * @param tab_list Pointer to the list of tab labels.
 * @return The WindowObject (Application Window).
 */
Object *CreateMainLayout(struct DiskObject *icon, struct List *tab_list);

/* [gui_events.c] - Event Handlers */
void HandleWorkerReply(struct Message *m);
void HandleWorkbenchMessage(struct ApplicationMsg *amsg, BOOL *running);
void HandleGUIEvent(uint32 result, uint16 code, BOOL *running);
void HandlePrefsEvent(uint32 result, uint16 code);
void UpdateBulkTabInfo(void);

/* [gui_worker.c] - Worker Thread */
void BenchmarkWorker(void);
void LaunchBenchmarkJob(void);

/* [gui_report.c] - Global Reports */
void ShowGlobalReport(void);

/* [gui_viz.c] - Visualizations */
void UpdateVisualization(void);
void InitVizFilterLabels(void);
void RefreshVizVolumeFilter(void);
void CleanupVizFilterLabels(void);
uint32 VizRenderHook(struct Hook *hook, Object *space_obj, struct gpRender *gpr);

/* [gui_viz_render.c] - Graph Rendering */
void RenderTrendGraph(struct RastPort *rp, struct IBox *box, BenchResult **results, uint32 count);

/* [gui_bulk.c] - Bulk Testing */
void RefreshBulkList(void);
void LaunchBulkJobs(void);

/* [gui_details_window.c] - Details Window management */

/**
 * @brief Handle "Show Details" action for selected list item.
 * @param list_obj Pointer to the ListBrowser object (History or Bench).
 */
void ShowBenchmarkDetails(Object *list_obj);

/**
 * @brief Open the Details Window for a specific result.
 * @param res The benchmark result to display.
 */
void OpenDetailsWindow(BenchResult *res);

/**
 * @brief Close the Details Window.
 */
void CloseDetailsWindow(void);

/**
 * @brief Handle events for the Details Window.
 */
void HandleDetailsWindowEvent(uint16 code, uint32 result);

/**
 * @brief Handle events for the Compare Window.
 */
void HandleCompareWindowEvent(uint16 code, uint32 result);

/**
 * @brief Open the comparison window to compare two benchmark results side-by-side.
 *
 * @param result1 Pointer to the first BenchResult to compare.
 * @param result2 Pointer to the second BenchResult to compare.
 */
void OpenCompareWindow(BenchResult *result1, BenchResult *result2);

/**
 * @brief Close the comparison window.
 */
void CloseCompareWindow(void);

#endif /* GUI_INTERNAL_H */
