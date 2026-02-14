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

#ifndef GUI_H
#define GUI_H

/*
 * Lightweight public API header for the GUI module.
 * Only types, struct definitions, and function prototypes.
 * Heavy OS/ReAction includes live in gui_internal.h.
 */

#include "engine.h"
#include <dos/dos.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <intuition/classes.h>   /* Class typedef */
#include <intuition/classusr.h>  /* Object typedef */
#include <intuition/intuition.h> /* struct Window */
#include <intuition/menuclass.h>
#include <libraries/application.h>

#define MINVERSION 53

#define MSG_TYPE_STATUS 1
#define MSG_TYPE_JOB 2

/**
 * @brief Message sent from GUI to Benchmark Process.
 * Defines the parameters for a new benchmark job.
 */
typedef struct
{
    struct Message msg;
    uint32 msg_type; /**< Message Type (e.g., MSG_TYPE_JOB) */
    BenchTestType type;
    char target_path[256];
    uint32 num_passes;
    uint32 block_size;
    BOOL use_trimmed_mean;
    BOOL flush_cache;
    struct MsgPort *reply_port;
} BenchJob;

/**
 * @brief Main GUI State structure.
 * Holds all ReAction objects, system resources, and application state.
 */
typedef struct
{
    Object *win_obj;
    struct Window *window;
    Object *tabs;
    Object *page_obj;

    /* Gadgets - Application Window */
    Object *bench_list;
    Object *history_list;
    Object *bulk_list;
    Object *status_light_obj;
    Object *run_button;
    Object *test_chooser;
    Object *pass_gad;
    Object *block_chooser;
    Object *target_chooser;

    /* Gadgets - Preferences Window */
    Object *prefs_win_obj;
    struct Window *prefs_window;
    Object *prefs_block_chooser;
    Object *prefs_pass_gad;
    Object *prefs_csv_path;
    Object *prefs_trimmed_check;
    Object *prefs_test_chooser;
    Object *prefs_target_chooser;

    /* Gadgets - Details Window */
    Object *details_win_obj;
    struct Window *details_window;
    Object *details_editor;
    Object *details_vscroll;
    Object *details_hscroll;
    Object *details_menu;
    Object *details_context_menu;

    /* Lists for Choosers/ListBrowsers */
    struct List history_labels;
    struct List bench_labels;
    struct List drive_list;
    struct List test_labels;
    struct List block_list;
    struct List bulk_labels;

    /* System Integration */
    uint32 app_id;
    struct MsgPort *gui_port;
    struct MsgPort *worker_port;
    struct MsgPort *worker_reply_port;
    struct MsgPort *prefs_port;
    struct MsgPort *app_port;
    struct MsgPort *popup_port;
    struct Catalog *catalog;

    Object *history_popup;
    Object *vis_bars[5];
    Object *vis_labels[5];

    /* Library Bases */
    struct Library *IconBase, *LocaleBase, *ApplicationBase, *AslBase;

    /* Class library bases. Many are now handled by reaction.h but
       we keep those we explicitly manage via OpenClass for safety. */
    struct ClassLibrary *WindowBase, *LayoutBase, *ButtonBase, *ListBrowserBase, *ChooserBase;
    struct ClassLibrary *CheckBoxBase, *ClickTabBase, *PageBase, *LabelBase, *StringBase, *IntegerBase;
    struct ClassLibrary *TextEditorBase, *ScrollerBase, *FuelGaugeBase;

    /* BOOPSI class pointers. reaction.h macros often use class strings (e.g. "button.gadget"),
       but we retain these for cases where explicit class pointers are preferred.
       Note: MenuClass/MenuBase removed as they are built-in to Intuition. */
    Class *WindowClass, *LayoutClass, *ButtonClass, *ListBrowserClass, *ChooserClass;
    Class *CheckBoxClass, *ClickTabClass, *PageClass, *LabelClass, *StringClass, *IntegerClass;
    Class *TextEditorClass, *ScrollerClass, *FuelGaugeClass;

    /* Interfaces */
    struct ApplicationIFace *IApp;
    struct LocaleIFace *ILoc;
    struct IconIFace *IIcn;
    struct IntegerIFace *IInteger;
    struct AslIFace *IAsl;
    struct PopupMenuIFace *IPopupMenu;
    struct PrefsObjectsIFace *IPrefsObjects;

    /* Application State */
    BOOL PageAvailable;
    BOOL use_trimmed_mean;
    uint32 default_test_type;
    uint32 default_block_size_idx;
    char default_drive[256];
    char csv_path[256];
    BOOL delete_prefs_needed;
    BOOL flush_cache;
    uint32 jobs_pending;

    /* Current Benchmark Settings (Decoupled from Gadgets) */
    uint32 current_test_type;
    uint32 current_passes;
    uint32 current_block_size;

    /* Benchmark Queue State */
    struct List benchmark_queue;
    BOOL worker_busy;

    /* Persistent buffers for Visualization Labels */
    char vis_label_buffers[5][128];

    /* Bulk Tab Gadgets */
    Object *bulk_info_label;
    Object *bulk_all_tests_check;
    Object *bulk_all_blocks_check;
} GUIState;

#include "benchmark_queue.h"

/**
 * @brief Message status sent from Benchmark Process to GUI.
 * Updates the GUI on the progress or completion of a job.
 */
typedef struct
{
    struct Message msg;
    uint32 msg_type; /**< Message Type (e.g., MSG_TYPE_STATUS) */
    BOOL finished;
    BOOL success;
    BenchResult result;
    BenchSampleData sample_data; /**< Time-series data for graphing */
    char status_text[128];
} BenchStatus;

/* Gadget IDs */
enum
{
    GID_MAIN_LAYOUT = 1,
    GID_TABS,
    GID_VOL_CHOOSER,
    GID_TEST_CHOOSER,
    GID_TARGET_PATH,
    GID_STATUS_LIGHT,
    GID_RUN_ALL,
    GID_RUN_SPRINTER,
    GID_RUN_HEAVY,
    GID_RUN_LEGACY,
    GID_RUN_DAILY,
    GID_CURRENT_RESULTS,
    GID_HISTORY_LIST,
    GID_REFRESH_HISTORY,
    GID_VIEW_REPORT,
    GID_TEST_DESCRIPTION,
    GID_NUM_PASSES,
    GID_BLOCK_SIZE,
    GID_PREFS_WINDOW,
    GID_PREFS_BLOCK,
    GID_PREFS_PASSES,
    GID_PREFS_CSV,
    GID_PREFS_CSV_BR,
    GID_PREFS_TRIMMED,
    GID_PREFS_TEST_TYPE,
    GID_PREFS_TARGET,
    GID_PREFS_SAVE,
    GID_PREFS_CANCEL,
    GID_DETAILS_WINDOW,
    GID_DETAILS_EDITOR,
    GID_DETAILS_VSCROLL,
    GID_DETAILS_HSCROLL,
    GID_DETAILS_CLOSE,
    GID_FLUSH_CACHE,
    GID_VIS_BAR_1,
    GID_VIS_BAR_2,
    GID_VIS_BAR_3,
    GID_VIS_BAR_4,
    GID_VIS_BAR_5,
    GID_VIS_LABEL_1,
    GID_VIS_LABEL_2,
    GID_VIS_LABEL_3,
    GID_VIS_LABEL_4,
    GID_VIS_LABEL_5,
    GID_BULK_LIST,
    GID_BULK_RUN,
    GID_BULK_INFO,
    GID_BULK_ALL_TESTS,
    GID_BULK_ALL_BLOCKS,
    GID_REFRESH_DRIVES
};

#define MID_ABOUT 1
#define MID_PREFS 2
#define MID_QUIT 3
#define MID_DELETE_PREFS 4
#define MID_EXPORT_TEXT 7
#define MID_SHOW_DETAILS 5
#define MID_DETAILS_COPY 6

#define COL_DATE 0
#define COL_VOL 1
#define COL_TEST 2
#define COL_BS 3
#define COL_PASSES 4
#define COL_MBPS 5
#define COL_IOPS 6
#define COL_DEVICE 7
#define COL_UNIT 8
#define COL_VER 9
#define COL_DIFF 10
#define COL_DUMMY 11

/* GUI Functions */

/**
 * @brief Initialize and start the GUI application.
 * Defines the event loop and manages the application lifecycle.
 *
 * @return 0 on normal exit.
 */
int StartGUI(void);

/**
 * @brief Format a block size in bytes to a readable string with unit.
 *
 * @param bytes The block size in bytes.
 * @return String representation (e.g., "4MB").
 */
const char *FormatPresetBlockSize(uint32 bytes);

/**
 * @brief Format a generic byte size to a human-readable string.
 *
 * @param bytes The size in bytes.
 * @return String representation (e.g., "1.5 GB").
 */
const char *FormatByteSize(uint64 bytes);

/**
 * @brief Export current benchmark history to an ANSI text file.
 *
 * @param filename The destination filename.
 */
void ExportToAnsiText(const char *filename);

#endif /* GUI_H */
