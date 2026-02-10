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

#ifndef GUI_H
#define GUI_H

#include <intuition/menuclass.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>

/* Standard ReAction consolidated headers and macros */
#define ALL_REACTION_CLASSES
#define ALL_REACTION_MACROS
#include <reaction/reaction.h>

#include "engine.h"

#define MINVERSION 53

#define MSG_TYPE_STATUS 1
#define MSG_TYPE_JOB 2

/* Message sent from GUI to Benchmark Process */
typedef struct
{
    struct Message msg;
    uint32 msg_type; /* MSG_TYPE_JOB */
    BenchTestType type;
    char target_path[256];
    uint32 num_passes;
    uint32 block_size;
    BOOL use_trimmed_mean;
    struct MsgPort *reply_port;
} BenchJob;

/* GUI State structure */
typedef struct
{
    Object *win_obj;
    struct Window *window;
    Object *tabs;
    Object *page_obj;
    Object *bench_list;
    Object *history_list;
    Object *status_light_obj;
    Object *run_button;
    Object *test_chooser;
    Object *pass_gad;
    Object *block_chooser;
    Object *target_chooser;
    Object *prefs_win_obj;
    struct Window *prefs_window;
    Object *prefs_block_chooser;
    Object *prefs_pass_gad;
    Object *prefs_csv_path;
    Object *prefs_trimmed_check;
    Object *prefs_test_chooser;
    Object *prefs_target_chooser;
    Object *details_win_obj;
    struct Window *details_window;
    Object *details_editor;
    Object *details_vscroll;
    Object *details_hscroll;
    Object *details_menu;
    Object *details_context_menu;

    struct List history_labels;
    struct List bench_labels;
    struct List drive_list;
    struct List test_labels;
    struct List block_list;

    uint32 app_id;
    struct MsgPort *gui_port;
    struct MsgPort *worker_port;
    struct MsgPort *worker_reply_port;
    struct MsgPort *prefs_port;
    struct MsgPort *app_port;
    struct MsgPort *popup_port;
    struct Catalog *catalog;

    Object *history_popup;

    struct Library *IconBase, *LocaleBase, *ApplicationBase, *AslBase;

    /* Class library bases. Many are now handled by reaction.h but
       we keep those we explicitly manage via OpenClass for safety. */
    struct ClassLibrary *WindowBase, *LayoutBase, *ButtonBase, *ListBrowserBase, *ChooserBase;
    struct ClassLibrary *CheckBoxBase, *ClickTabBase, *PageBase, *LabelBase, *StringBase, *IntegerBase;
    struct ClassLibrary *TextEditorBase, *ScrollerBase;

    /* BOOPSI class pointers. reaction.h macros often use class strings (e.g. "button.gadget"),
       but we retain these for cases where explicit class pointers are preferred.
       Note: MenuClass/MenuBase removed as they are built-in to Intuition. */
    Class *WindowClass, *LayoutClass, *ButtonClass, *ListBrowserClass, *ChooserClass;
    Class *CheckBoxClass, *ClickTabClass, *PageClass, *LabelClass, *StringClass, *IntegerClass;
    Class *TextEditorClass, *ScrollerClass;

    struct ApplicationIFace *IApp;
    struct LocaleIFace *ILoc;
    struct IconIFace *IIcn;
    struct IntegerIFace *IInteger;
    struct AslIFace *IAsl;
    struct PopupMenuIFace *IPopupMenu;
    struct PrefsObjectsIFace *IPrefsObjects;

    BOOL PageAvailable;
    BOOL use_trimmed_mean;
    uint32 default_test_type;
    uint32 default_block_size_idx;
    char default_drive[256];
    char csv_path[256];
    BOOL delete_prefs_needed;
} GUIState;

/* Message sent from Benchmark Process to GUI for progress/results */
typedef struct
{
    struct Message msg;
    uint32 msg_type; /* MSG_TYPE_STATUS */
    BOOL finished;
    BOOL success;
    BenchResult result;
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
    GID_DETAILS_CLOSE
};

#define MID_ABOUT 1
#define MID_PREFS 2
#define MID_QUIT 3
#define MID_DELETE_PREFS 4
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
#define COL_DUMMY 10

/* GUI Functions */
int StartGUI(void);
const char *FormatPresetBlockSize(uint32 bytes);
const char *FormatByteSize(uint64 bytes);

#endif /* GUI_H */
