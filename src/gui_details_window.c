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

#include "gui_details_window.h"
#include "debug.h"
#include "gui_internal.h"
#include <classes/window.h>
#include <gadgets/scroller.h>
#include <gadgets/texteditor.h>
#include <images/label.h>
#include <intuition/menuclass.h>
#include <intuition/pointerclass.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/scroller.h>
#include <proto/texteditor.h>
#include <reaction/reaction_macros.h>
#include <stdio.h>

extern GUIState ui;
extern struct IntuitionIFace *IIntuition;
extern struct ExecIFace *IExec;

static char report_buffer[4096];

void ShowBenchmarkDetails(Object *list_obj)
{
    struct Node *sel = NULL;
    IIntuition->GetAttr(LISTBROWSER_SelectedNode, list_obj, (uint32 *)&sel);
    if (!sel)
        return;

    BenchResult *res = NULL;
    IListBrowser->GetListBrowserNodeAttrs(sel, LBNA_UserData, &res, TAG_DONE);

    if (!res) {
        /* Fallback for nodes that might still have the old string UserData or are NULL */
        char *extra_info = NULL;
        IListBrowser->GetListBrowserNodeAttrs(sel, LBNA_UserData, &extra_info, TAG_DONE);
        ShowMessage("Benchmark Details", extra_info ? extra_info : "No record data available.", "Close");
        return;
    }

    OpenDetailsWindow(res);
}

void OpenDetailsWindow(BenchResult *res)
{
    if (!res)
        return;

    /* If window already open, close it first to refresh */
    if (ui.details_win_obj) {
        CloseDetailsWindow();
    }

    /* Format the report into the local buffer */
    snprintf(report_buffer, sizeof(report_buffer),
             " Detailed Benchmark Report\n"
             " -------------------------\n"
             " Date/Time:  %s\n"
             " Test Type:  %s\n"
             " Disk Name:  %s\n\n"
             " Settings & Env:\n"
             "  ID:         %s\n"
             "  FileSystem: %s\n"
             "  Passes:     %u (Trimmed: %s)\n"
             "  Block Size: %s\n\n"
             " Performance:\n"
             "  Avg. Speed: %.2f MB/s\n"
             "  Min. Speed: %.2f MB/s\n"
             "  Max. Speed: %.2f MB/s\n"
             "  Avg. IOPS:  %u\n\n"
             " Statistics:\n"
             "  Total Time: %.2f seconds\n"
             "  Total Data: %.2f MB\n"
             "  Pass Spread: %.1f\n\n"
             " Hardware:\n"
             "  Device:     %s (Unit %u)\n"
             "  Vendor:     %s\n"
             "  Product:    %s\n"
             "  Firmware:   %s\n"
             "  Serial:     %s\n\n"
             " Historical Trend:\n"
             "  Previous:   %s\n"
             "  Prev Speed: %.2f MB/s\n"
             "  Difference: %+.1f%% %s\n\n"
             " Application:\n"
             "  Version:    %s\n",
             res->timestamp,
             (res->type == TEST_SPRINTER)       ? "Sprinter"
             : (res->type == TEST_HEAVY_LIFTER) ? "HeavyLifter"
             : (res->type == TEST_LEGACY)       ? "Legacy"
             : (res->type == TEST_DAILY_GRIND)  ? "DailyGrind"
             : (res->type == TEST_SEQUENTIAL)   ? "Sequential"
             : (res->type == TEST_RANDOM_4K)    ? "Random4K"
             : (res->type == TEST_PROFILER)     ? "FullProfiler"
                                                : "Unknown",
             res->volume_name, res->result_id, res->fs_type, (unsigned int)res->passes,
             res->use_trimmed_mean ? "Yes" : "No", FormatPresetBlockSize(res->block_size), res->mb_per_sec,
             res->min_mbps, res->max_mbps, (unsigned int)res->iops, res->total_duration,
             (double)res->cumulative_bytes / 1048576.0, res->max_mbps - res->min_mbps, res->device_name,
             (unsigned int)res->device_unit, res->vendor, res->product, res->firmware_rev, res->serial_number,
             (res->prev_mbps > 0) ? res->prev_timestamp : "None found", res->prev_mbps, res->diff_per,
             (res->diff_per > 0)   ? "(FASTER)"
             : (res->diff_per < 0) ? "(SLOWER)"
                                   : "(SAME)",
             res->app_version);

    /* Fixed labels and shortcut display:
       - Titlebar (Window Menu): Use clean "Copy" + MA_Key. Icon is auto-added.
       - Context Menu: Use "C|Copy" to attempt to force hint rendering.
       Manually building the tree avoids TagItem/NewMenu mismatch issues. */

    ui.details_menu = IIntuition->NewObject(
        NULL, "menuclass", MA_Type, T_ROOT, MA_AddChild,
        IIntuition->NewObject(NULL, "menuclass", MA_Type, T_MENU, MA_Label, "Edit", MA_AddChild,
                              IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ITEM, MA_Label, "Copy", MA_ID,
                                                    MID_DETAILS_COPY, MA_Key, "C", TAG_DONE),
                              TAG_DONE),
        TAG_DONE);

    ui.details_context_menu =
        IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ROOT, MA_AddChild,
                              IIntuition->NewObject(NULL, "menuclass", MA_Type, T_MENU, MA_Label, "Edit", MA_AddChild,
                                                    IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ITEM, MA_Label,
                                                                          "C|Copy", MA_ID, MID_DETAILS_COPY, TAG_DONE),
                                                    TAG_DONE),
                              TAG_DONE);

    /* Create the Window and Layout */
    ui.details_win_obj = WindowObject, WA_Title, "Benchmark Details", WA_SizeGadget, TRUE, WA_DepthGadget, TRUE,
    WA_DragBar, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE, WA_SmartRefresh, TRUE, WA_InnerWidth, 500,
    WA_InnerHeight, 550, WINDOW_MenuStrip, ui.details_menu, WINDOW_ParentGroup, VLayoutObject, LAYOUT_AddChild,
    HLayoutObject, LAYOUT_AddChild,
    ui.details_editor = IIntuition->NewObject(ui.TextEditorClass, NULL, GA_ID, GID_DETAILS_EDITOR, GA_RelVerify, TRUE,
                                              GA_ReadOnly, TRUE, GA_TEXTEDITOR_FixedFont, TRUE, GA_TEXTEDITOR_Contents,
                                              (uint32)report_buffer, GA_ContextMenu, ui.details_context_menu, TAG_DONE),
    LAYOUT_AddChild,
    ui.details_vscroll = IIntuition->NewObject(ui.ScrollerClass, NULL, GA_ID, GID_DETAILS_VSCROLL, GA_RelVerify, TRUE,
                                               SCROLLER_Orientation, SORIENT_VERT, SCROLLER_Arrows, TRUE, TAG_DONE),
    CHILD_WeightedWidth, 0, LayoutEnd, CHILD_WeightedHeight, 100, /* Give editor full dominance */

        LAYOUT_AddChild, HLayoutObject, LAYOUT_ShrinkWrap, TRUE, LAYOUT_HorizAlignment, LALIGN_CENTER,
    LAYOUT_TopSpacing, 4, LAYOUT_BottomSpacing, 4, LAYOUT_AddChild, ButtonObject, GA_ID, GID_DETAILS_CLOSE,
    GA_RelVerify, TRUE, GA_Text, "Close", ButtonEnd, CHILD_WeightedWidth, 0, LayoutEnd, CHILD_WeightedHeight,
    0, /* Fixed height for button row */
        LayoutEnd, WindowEnd;

    /* Link vertical scroller to texteditor */
    if (ui.details_editor) {
        IIntuition->SetAttrs(ui.details_editor, GA_TEXTEDITOR_VertScroller, ui.details_vscroll, TAG_DONE);
    }

    if (ui.details_win_obj) {
        ui.details_window = (struct Window *)IIntuition->IDoMethod(ui.details_win_obj, WM_OPEN);
        if (ui.details_window) {
            /* Force pointer type after open to ensure feedback */
            IIntuition->SetWindowPointer(ui.details_window, WA_PointerType, POINTERTYPE_TEXT, TAG_DONE);
        } else {
            LOG_DEBUG("FAILED to open details window");
            IIntuition->DisposeObject(ui.details_win_obj);
            ui.details_win_obj = NULL;
        }
    } else {
        LOG_DEBUG("FAILED to create details window object");
        /* Cleanup individual objects if layout failed */
        if (ui.details_menu)
            IIntuition->DisposeObject(ui.details_menu);
        if (ui.details_context_menu)
            IIntuition->DisposeObject(ui.details_context_menu);
        ui.details_menu = NULL;
        ui.details_context_menu = NULL;
    }
}

void CloseDetailsWindow(void)
{
    if (ui.details_win_obj) {
        IIntuition->DisposeObject(ui.details_win_obj);
        ui.details_win_obj = NULL;
        ui.details_window = NULL;
        ui.details_editor = NULL;
        ui.details_vscroll = NULL;
        ui.details_hscroll = NULL;
    }
    if (ui.details_menu) {
        IIntuition->DisposeObject(ui.details_menu);
        ui.details_menu = NULL;
    }
    if (ui.details_context_menu) {
        IIntuition->DisposeObject(ui.details_context_menu);
        ui.details_context_menu = NULL;
    }
}

void HandleDetailsWindowEvent(uint16 code, uint32 result)
{
    uint32 class = result & WMHI_CLASSMASK;
    uint32 gid = result & WMHI_GADGETMASK;

    switch (class) {
    case WMHI_CLOSEWINDOW:
        CloseDetailsWindow();
        break;

    case WMHI_GADGETUP:
        if (gid == GID_DETAILS_CLOSE) {
            CloseDetailsWindow();
        }
        break;

    case WMHI_MENUPICK:
        /* Context and Window menus both trigger WMHI_MENUPICK */
        {
            uint32 id = NO_MENU_ID;
            Object *menu_obj = NULL;

            LOG_DEBUG("Details Window selection: result=0x%08X", (unsigned int)result);

            /* Use WINDOW_MenuAddress to find which menu tree generated the event (V54) */
            IIntuition->GetAttr(WINDOW_MenuAddress, ui.details_win_obj, (uint32 *)&menu_obj);

            if (menu_obj) {
                while ((id = IIntuition->IDoMethod(menu_obj, MM_NEXTSELECT, 0, id)) != NO_MENU_ID) {
                    LOG_DEBUG("Menu ID selected (Address): %u", (unsigned int)id);
                    if (id == MID_DETAILS_COPY) {
                        if (ui.details_editor) {
                            LOG_DEBUG("Triggering COPY command on texteditor");
                            IIntuition->IDoMethod(ui.details_editor, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
                        }
                    }
                }
            } else {
                /* Robust Fallback: Check both menus if WINDOW_MenuAddress is NULL or unsupported */
                if (ui.details_context_menu) {
                    while ((id = IIntuition->IDoMethod(ui.details_context_menu, MM_NEXTSELECT, 0, id)) != NO_MENU_ID) {
                        LOG_DEBUG("Menu ID selected (Context): %u", (unsigned int)id);
                        if (id == MID_DETAILS_COPY) {
                            if (ui.details_editor) {
                                IIntuition->IDoMethod(ui.details_editor, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
                            }
                        }
                    }
                }
                id = NO_MENU_ID; /* Reset for next loop */
                if (ui.details_menu) {
                    while ((id = IIntuition->IDoMethod(ui.details_menu, MM_NEXTSELECT, 0, id)) != NO_MENU_ID) {
                        LOG_DEBUG("Menu ID selected (MenuStrip): %u", (unsigned int)id);
                        if (id == MID_DETAILS_COPY) {
                            if (ui.details_editor) {
                                IIntuition->IDoMethod(ui.details_editor, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
                            }
                        }
                    }
                }
            }
        }
        break;
    }
}
