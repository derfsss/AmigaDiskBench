/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"
#include "engine_workloads.h"

void OpenDescribeWindow(uint32 test_type)
{
    if (ui.describe_win_obj) {
        CloseDescribeWindow();
    }

    const char *info = GetWorkloadDetailedInfo((BenchTestType)test_type);

    ui.describe_win_obj = WindowObject,
        WA_Title, "Test Description",
        WA_SizeGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_Activate, TRUE,
        WA_SmartRefresh, TRUE,
        WA_InnerWidth, 420,
        WA_InnerHeight, 350,
        WINDOW_ParentGroup, VLayoutObject,
            LAYOUT_AddChild, HLayoutObject,
                LAYOUT_AddChild,
                    ui.describe_editor = IIntuition->NewObject(
                        ui.TextEditorClass, NULL,
                        GA_ID, GID_DESCRIBE_EDITOR,
                        GA_ReadOnly, TRUE,
                        GA_TEXTEDITOR_FixedFont, TRUE,
                        GA_TEXTEDITOR_Contents, (uint32)info,
                        TAG_DONE),
                LAYOUT_AddChild,
                    ui.describe_vscroll = IIntuition->NewObject(
                        ui.ScrollerClass, NULL,
                        GA_ID, GID_DESCRIBE_VSCROLL,
                        SCROLLER_Orientation, SORIENT_VERT,
                        SCROLLER_Arrows, TRUE,
                        TAG_DONE),
                CHILD_WeightedWidth, 0,
            LayoutEnd,
            CHILD_WeightedHeight, 100,
        LayoutEnd,
    WindowEnd;

    if (ui.describe_editor) {
        IIntuition->SetAttrs(ui.describe_editor,
                             GA_TEXTEDITOR_VertScroller, ui.describe_vscroll,
                             TAG_DONE);
    }

    if (ui.describe_win_obj) {
        ui.describe_window = (struct Window *)IIntuition->IDoMethod(ui.describe_win_obj, WM_OPEN);
        if (!ui.describe_window) {
            LOG_DEBUG("FAILED to open describe window");
            IIntuition->DisposeObject(ui.describe_win_obj);
            ui.describe_win_obj = NULL;
            ui.describe_editor = NULL;
            ui.describe_vscroll = NULL;
        }
    } else {
        LOG_DEBUG("FAILED to create describe window object");
        ui.describe_editor = NULL;
        ui.describe_vscroll = NULL;
    }
}

void CloseDescribeWindow(void)
{
    if (ui.describe_win_obj) {
        IIntuition->DisposeObject(ui.describe_win_obj);
        ui.describe_win_obj = NULL;
        ui.describe_window = NULL;
        ui.describe_editor = NULL;
        ui.describe_vscroll = NULL;
    }
}

void HandleDescribeWindowEvent(uint16 code, uint32 result)
{
    uint32 class = result & WMHI_CLASSMASK;

    switch (class) {
    case WMHI_CLOSEWINDOW:
        CloseDescribeWindow();
        break;
    }
}
