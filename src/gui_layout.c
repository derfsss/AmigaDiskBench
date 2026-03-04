/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"
#include <classes/window.h>

static struct ColumnInfo bench_cols[] = {{20, "", CIF_FIXED}, /* Checkbox */
                                         {100, "Date", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {80, "Volume", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {100, "Test Type", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {80, "Block Size", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {100, "No. of Passes", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {120, "Average (MB/s)", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {60, "IOPS", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {80, "Device", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {40, "Unit", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {120, "App Version", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {80, "vs Prev (%)", CIF_SORTABLE | CIF_DRAGGABLE},
                                         {1, "", CIF_FIXED},
                                         {-1, NULL, 0}};

static struct ColumnInfo current_run_cols[] = {{100, "Date", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {80, "Volume", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {100, "Test Type", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {120, "Average (MB/s)", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {80, "vs Prev (%)", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {120, "App Version", CIF_SORTABLE | CIF_DRAGGABLE},
                                               {-1, NULL, 0}};

static struct ColumnInfo bulk_cols[] = {{20, "", CIF_FIXED}, /* Checkbox */
                                        {150, "Volume", CIF_FIXED | CIF_DRAGGABLE},
                                        {100, "FileSystem", CIF_FIXED | CIF_DRAGGABLE},
                                        {1, "", CIF_FIXED},
                                        {-1, NULL, 0}};


static struct ColumnInfo health_cols[] = {{30, "ID", CIF_SORTABLE | CIF_DRAGGABLE},                {200, "Attribute Name", CIF_SORTABLE | CIF_DRAGGABLE},
                                          {50, "Value", CIF_SORTABLE | CIF_DRAGGABLE},             {50, "Worst", CIF_SORTABLE | CIF_DRAGGABLE},
                                          {50, "Thresh", CIF_SORTABLE | CIF_DRAGGABLE},            {80, "Raw Value", CIF_SORTABLE | CIF_DRAGGABLE},
                                          {50, "Status", CIF_SORTABLE | CIF_DRAGGABLE},            {-1, NULL, 0}};

/**
 * @brief Creates the main application window layout.
 *
 * defines the ReAction GUI object hierarchy, including tabs for Benchmark,
 * History, Visualization, and Bulk testing.
 *
 * @param icon Pointer to the DiskObject (icon) for the window.
 * @param tab_list Pointer to the list of tab labels.
 * @return Pointer to the root WindowObject, or NULL on failure.
 */
Object *CreateMainLayout(struct DiskObject *icon, struct List *tab_list)
{
    /* Helper macro to maintain readability for the deep nesting */
    Object *page0, *page_diskinfo, *page1, *page2, *page3, *page4, *page5;

    /* Refactored Page 0 (Benchmark) using standard ReAction macros */
    page0 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_Label,
    GetString(3, "Benchmark Control"), LAYOUT_BevelStyle, BVS_GROUP,
    /* Drive Selector */
        LAYOUT_AddChild,
    (ui.target_chooser = ChooserObject, GA_ID, GID_VOL_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Labels,
     (uint32)&ui.drive_list, GA_HintInfo, "Select the target drive for benchmarking.", End),
    CHILD_Label, LabelObject, LABEL_Text, GetString(4, "Drive:"), End, CHILD_WeightedHeight, 0,
    /* Test Type Selector */
        LAYOUT_AddChild,
    (ui.test_chooser = ChooserObject, GA_ID, GID_TEST_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Selected, 0, CHOOSER_Labels,
     (uint32)&ui.test_labels, GA_HintInfo, "Select benchmark test. Right-click for detailed description.", End),
    CHILD_Label, LabelObject, LABEL_Text, GetString(5, "Test Type:"), End, CHILD_WeightedHeight, 0,
    /* Passes + Average Method (same row, aligned with Drive/Test/Block) */
        LAYOUT_AddChild, HLayoutObject, LAYOUT_HorizSpacing, 4,
    LAYOUT_AddChild,
    (ui.pass_gad = IntegerObject, GA_ID, GID_NUM_PASSES, GA_RelVerify, TRUE, INTEGER_MaxChars, 2,
     INTEGER_Minimum, 3, INTEGER_Maximum, 20,
     INTEGER_Number, 3, GA_HintInfo, "Number of times to repeat the test (3-20).", End),
    CHILD_WeightedWidth, 0, CHILD_MinWidth, 60,
    LAYOUT_AddChild,
    (ui.avg_method_label = ButtonObject, GA_ID, GID_AVG_METHOD_LABEL, GA_ReadOnly, TRUE,
     GA_Text, "Average: All Passes", BUTTON_BevelStyle, BVS_NONE, BUTTON_Transparent, TRUE,
     BUTTON_Justification, BCJ_LEFT, End),
    CHILD_WeightedWidth, 100, End,
    CHILD_Label, LabelObject, LABEL_Text, GetString(6, "Passes:"), End, CHILD_WeightedHeight, 0,
    /* Block Size */
        LAYOUT_AddChild,
    (ui.block_chooser = ChooserObject, GA_ID, GID_BLOCK_SIZE, GA_RelVerify, TRUE, CHOOSER_Selected, 0, CHOOSER_Labels,
     (uint32)&ui.block_list, GA_HintInfo, "Select the data block size for the test.", End),
    CHILD_Label, LabelObject, LABEL_Text, GetString(7, "Block Size:"), End, CHILD_WeightedHeight, 0, End,
    CHILD_WeightedHeight, 0, /* End Group "Benchmark Control" */

        /* Volume Information Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Volume Information", LAYOUT_BevelStyle, BVS_GROUP,
    LAYOUT_AddChild,
    (ui.vol_size_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", BUTTON_Justification, BCJ_LEFT, End),
    CHILD_Label, LabelObject, LABEL_Text, "Size:", End, CHILD_WeightedHeight, 0, LAYOUT_AddChild,
    (ui.vol_free_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", BUTTON_Justification, BCJ_LEFT, End),
    CHILD_Label, LabelObject, LABEL_Text, "Free:", End, CHILD_WeightedHeight, 0, LAYOUT_AddChild,
    (ui.vol_fs_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", BUTTON_Justification, BCJ_LEFT, End),
    CHILD_Label, LabelObject, LABEL_Text, "Filesystem:", End, CHILD_WeightedHeight, 0, LAYOUT_AddChild,
    (ui.vol_device_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", BUTTON_Justification, BCJ_LEFT, End),
    CHILD_Label, LabelObject, LABEL_Text, "Device:", End, CHILD_WeightedHeight, 0, End, CHILD_WeightedHeight, 0,

    /* Benchmark Actions Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Benchmark Actions", LAYOUT_BevelStyle, BVS_GROUP,
    LAYOUT_ShrinkWrap, TRUE, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild,
    (ui.run_button = ButtonObject, GA_ID, GID_RUN_ALL, GA_RelVerify, TRUE, GA_Text, GetString(8, "Run Benchmark"),
     GA_HintInfo, "Start the selected benchmark test.", End),
    LAYOUT_AddChild, ButtonObject, GA_ID, GID_REFRESH_DRIVES, GA_RelVerify, TRUE, GA_Text, "Refresh Drives",
    GA_HintInfo, "Reload the list of available drives.", End, End, CHILD_WeightedHeight, 0,
    End,                         /* End Group "Benchmark Actions" */
        CHILD_WeightedHeight, 0, /* Fix: Prevent group from expanding vertically in main page */
        /* Benchmark List */
        LAYOUT_AddChild,
    (ui.bench_list = ListBrowserObject, GA_ID, GID_CURRENT_RESULTS, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
     (uint32)current_run_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.bench_labels,
     LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE, GA_HintInfo,
     "Double-click an item to view detailed benchmark results.", End),
    CHILD_WeightedHeight, 100, End;

    /* Refactored Page 1 (History) using standard ReAction macros */
    page1 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild,
    (ui.history_list = ListBrowserObject, GA_ID, GID_HISTORY_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
     (uint32)bench_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.history_labels,
     LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE,
     LISTBROWSER_MultiSelect, TRUE, GA_HintInfo, "Double-click an item to view detailed benchmark results.", End),
    CHILD_WeightedHeight, 100, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID,
    GID_REFRESH_HISTORY, GA_Text, GetString(9, "Refresh History"), GA_HintInfo, "Reload the history from disk.", End,
    LAYOUT_AddChild, ButtonObject, GA_ID, GID_VIEW_REPORT, GA_Text, GetString(10, "Global Report"), GA_HintInfo,
    "View a summarized report of all tests.", End, LAYOUT_AddChild,
    (ui.compare_button = ButtonObject, GA_ID, GID_HISTORY_COMPARE, GA_Text, "Compare Selected", GA_Disabled, TRUE,
     GA_HintInfo, "Select exactly two items to compare.", End),
    LAYOUT_AddChild, ButtonObject, GA_ID, GID_HISTORY_DELETE, GA_Text, "Delete Selected", GA_HintInfo,
    "Delete the selected history items.", End, LAYOUT_AddChild, ButtonObject, GA_ID, GID_HISTORY_CLEAR_ALL, GA_Text,
    "Clear All", GA_HintInfo, "Delete ALL history items (cannot be undone).", End, LAYOUT_AddChild, ButtonObject, GA_ID,
    GID_HISTORY_EXPORT, GA_Text, "Export", GA_HintInfo, "Export history to a CSV file.", End, End, CHILD_WeightedHeight,
    0, End;

    /* Page 2 (Visualization) - History Trend Graph */
    page2 = VLayoutObject, LAYOUT_SpaceOuter, TRUE,

    /* Filter Controls: Chart, Date, Color By, Reload */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Filters", LAYOUT_BevelStyle, BVS_GROUP,
    /* Row 1: Chart, Date, Color By */
        LAYOUT_AddChild, HLayoutObject,
    LAYOUT_AddChild,
    (ui.viz_chart_type = ChooserObject, GA_ID, GID_VIZ_CHART_TYPE, GA_RelVerify, TRUE, CHOOSER_Labels,
     (uint32)&ui.viz_chart_type_labels, GA_HintInfo, "Select Chart Type.", End),
    CHILD_Label, LabelObject, LABEL_Text, "Chart:", LABEL_Justification, LJ_RIGHT, End, CHILD_WeightedWidth, 33,
    LAYOUT_AddChild,
    (ui.viz_filter_metric = ChooserObject, GA_ID, GID_VIZ_FILTER_METRIC, GA_RelVerify, TRUE, CHOOSER_Labels,
     (uint32)&ui.viz_metric_labels, CHOOSER_Selected, ui.viz_date_range_idx, GA_HintInfo, "Filter by Date Range.", End),
    CHILD_Label, LabelObject, LABEL_Text, "Date:", LABEL_Justification, LJ_RIGHT, End, CHILD_WeightedWidth, 33,
    LAYOUT_AddChild,
    (ui.viz_color_by_display = ButtonObject, GA_ID, GID_VIZ_COLOR_BY, GA_ReadOnly, TRUE,
     GA_Text, "Drive", GA_HintInfo, "Series grouping is determined by the selected profile.", End),
    CHILD_Label, LabelObject, LABEL_Text, "Color By:", LABEL_Justification, LJ_RIGHT, End, CHILD_WeightedWidth, 33, End,
    CHILD_WeightedHeight, 0,
    /* Row 2: Reload Profiles button (right-aligned) */
    LAYOUT_AddChild, HLayoutObject,
        LAYOUT_AddChild, SpaceObject, End,
        CHILD_WeightedWidth, 67,
        LAYOUT_AddChild,
        (ui.viz_reload_button = ButtonObject, GA_ID, GID_VIZ_RELOAD,
         GA_RelVerify, TRUE, GA_Text, "Reload Profiles",
         GA_HintInfo, "Re-scan Visualizations folder for updated .viz files.", End),
        CHILD_WeightedWidth, 33,
    End, CHILD_WeightedHeight, 0,
    End, CHILD_WeightedHeight, 0,

    /* Details Label (Hover Info) */
        LAYOUT_AddChild,
    (ui.viz_details_label = ButtonObject, GA_ID, GID_VIZ_DETAILS_LABEL, GA_ReadOnly, TRUE, GA_Text,
     "Hover over points for details...", End),
    CHILD_WeightedHeight, 0,

    /* Graph Canvas */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Performance Trend", LAYOUT_BevelStyle, BVS_GROUP,

    LAYOUT_AddChild,
    (ui.viz_canvas = SpaceObject, GA_ID, GID_VIZ_CANVAS, SPACE_MinWidth, 400, SPACE_MinHeight, 250, SPACE_BevelStyle,
     BVS_FIELD, SpaceEnd),
    CHILD_WeightedHeight, 100, End, CHILD_WeightedHeight, 100, End;

    /* Page 3 (Bulk Testing) */
    page3 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild,
    (ui.bulk_list = ListBrowserObject, GA_ID, GID_BULK_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
     (uint32)bulk_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.bulk_labels,
     LISTBROWSER_AutoFit, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
    CHILD_WeightedHeight, 100,
    /* Queued Job Settings & Options */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Queued Job Settings", LAYOUT_BevelStyle, BVS_GROUP,
    LAYOUT_AddChild,
    (ui.bulk_info_label = ButtonObject, GA_ID, GID_BULK_INFO, GA_ReadOnly, TRUE, GA_Text, "Init...", End),
    LAYOUT_AddChild,
    (ui.fuel_gauge = FuelGaugeObject, GA_ID, GID_FUEL_GAUGE, FUELGAUGE_Min, 0, FUELGAUGE_Max, 100, FUELGAUGE_Level, 0,
     FUELGAUGE_Justification, FGJ_CENTER, GA_Text, "0%", End),
    LAYOUT_AddChild,
    (ui.bulk_all_tests_check = CheckBoxObject, GA_ID, GID_BULK_ALL_TESTS, GA_RelVerify, TRUE, GA_Text,
     "Run All Test Types (Sprinter..Profiler)", CHECKBOX_Checked, FALSE, End),
    LAYOUT_AddChild,
    (ui.bulk_all_blocks_check = CheckBoxObject, GA_ID, GID_BULK_ALL_BLOCKS, GA_RelVerify, TRUE, GA_Text,
     "Run All Block Sizes (4K..1M)", CHECKBOX_Checked, FALSE, GA_HintInfo, "If checked, adds jobs for all block sizes.",
     End),
    End, CHILD_WeightedHeight, 0, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID, GID_BULK_RUN,
    GA_Text, "Run Bulk Benchmark on Selected", GA_HintInfo, "Execute the queued benchmark jobs.", End, End,
    CHILD_WeightedHeight, 0, End;

    /* Page 4 (Drive Health) */
    page4 = VLayoutObject, LAYOUT_SpaceOuter, TRUE,
    /* Health Summary Header */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Drive Health Summary", LAYOUT_BevelStyle, BVS_GROUP,
    /* Drive Selector for Health */
        LAYOUT_AddChild,
    (ui.health_target_chooser = ChooserObject, GA_ID, GID_HEALTH_DRIVE, GA_RelVerify, TRUE, CHOOSER_Labels,
     (uint32)&ui.drive_list, GA_HintInfo, "Select the drive to query for health information.", End),
    CHILD_Label, LabelObject, LABEL_Text, "Drive:", End, CHILD_WeightedHeight, 0,

    LAYOUT_AddChild,
    (ui.health_status_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "Select a drive...", BUTTON_Justification,
     BCJ_CENTER, End),
    CHILD_WeightedHeight, 0, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild,
    (ui.health_temp_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "Temp: N/A", End), LAYOUT_AddChild,
    (ui.health_power_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "Power-on: N/A", End), End, CHILD_WeightedHeight,
    0, LAYOUT_AddChild,
    (ui.health_refresh_btn = ButtonObject, GA_ID, GID_HEALTH_REFRESH, GA_Text, "Refresh Health Data", GA_RelVerify,
     TRUE, End),
    CHILD_WeightedHeight, 0, End, CHILD_WeightedHeight, 0,
    /* S.M.A.R.T. Attributes List */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "S.M.A.R.T. Attributes", LAYOUT_BevelStyle, BVS_GROUP,
    LAYOUT_ShrinkWrap, FALSE,
    LAYOUT_AddChild,
    (ui.health_list = ListBrowserObject, GA_ID, GID_HEALTH_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo, (uint32)health_cols,
     LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.health_labels, LISTBROWSER_AutoFit, TRUE,
     LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
    CHILD_WeightedHeight, 100, End, End;

    /* Page 5 (Log) — scrollable timestamped benchmark transcript */
    page5 = VLayoutObject, LAYOUT_SpaceOuter, TRUE,
    LAYOUT_AddChild, HLayoutObject, LAYOUT_ShrinkWrap, TRUE,
        LAYOUT_AddChild, ButtonObject, GA_ID, GID_LOG_CLEAR, GA_RelVerify, TRUE,
            GA_Text, "Clear Log", End,
        LAYOUT_AddChild, ButtonObject, GA_ID, GID_LOG_COPY, GA_RelVerify, TRUE,
            GA_Text, "Copy to Clipboard", End,
    End, CHILD_WeightedHeight, 0,
    LAYOUT_AddChild, HLayoutObject,
        LAYOUT_AddChild,
        (ui.log_editor = IIntuition->NewObject(ui.TextEditorClass, NULL,
            GA_ID, GID_LOG_EDITOR, GA_RelVerify, TRUE,
            GA_ReadOnly, TRUE,
            GA_TEXTEDITOR_FixedFont, TRUE,
            GA_TEXTEDITOR_Contents, (uint32)"",
            TAG_DONE)),
        LAYOUT_AddChild,
        (ui.log_vscroll = IIntuition->NewObject(ui.ScrollerClass, NULL,
            GA_ID, GID_LOG_VSCROLL, GA_RelVerify, TRUE,
            SCROLLER_Orientation, SORIENT_VERT,
            SCROLLER_Arrows, TRUE,
            TAG_DONE)),
        CHILD_WeightedWidth, 0,
    End, CHILD_WeightedHeight, 100,
    End;

    /* Build context menu for test chooser (right-click: Describe Test) */
    ui.test_context_menu = IIntuition->NewObject(
        NULL, "menuclass", MA_Type, T_ROOT, MA_AddChild,
        IIntuition->NewObject(NULL, "menuclass", MA_Type, T_MENU, MA_Label, "Info", MA_AddChild,
            IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ITEM,
                MA_Label, "Describe Test...", MA_ID, MID_TEST_DESCRIBE, TAG_DONE),
            TAG_DONE),
        TAG_DONE);

    if (ui.test_chooser && ui.test_context_menu) {
        IIntuition->SetAttrs(ui.test_chooser,
                             GA_ContextMenu, ui.test_context_menu,
                             TAG_DONE);
    }

    /* Build context menu for log editor (right-click: Select All, Copy) */
    ui.log_context_menu = IIntuition->NewObject(
        NULL, "menuclass", MA_Type, T_ROOT, MA_AddChild,
        IIntuition->NewObject(NULL, "menuclass", MA_Type, T_MENU, MA_Label, "Edit", MA_AddChild,
            IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ITEM,
                MA_Label, "Select All", MA_ID, MID_LOG_SELECTALL, MA_Key, "A", TAG_DONE),
            MA_AddChild,
            IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ITEM,
                MA_Label, "Copy", MA_ID, MID_LOG_COPY, MA_Key, "C", TAG_DONE),
            TAG_DONE),
        TAG_DONE);

    /* Link vertical scroller and context menu to log editor */
    if (ui.log_editor && ui.log_vscroll) {
        IIntuition->SetAttrs(ui.log_editor,
                             GA_TEXTEDITOR_VertScroller, ui.log_vscroll,
                             TAG_DONE);
    }
    if (ui.log_editor && ui.log_context_menu) {
        IIntuition->SetAttrs(ui.log_editor,
                             GA_ContextMenu, ui.log_context_menu,
                             TAG_DONE);
    }

    static struct NewMenu menu_data[] = {
        {NM_TITLE, (STRPTR) "Project", NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "About...", (STRPTR) "A", 0, 0, (APTR)MID_ABOUT},
        {NM_ITEM, (STRPTR) "Preferences...", (STRPTR) "P", 0, 0, (APTR)MID_PREFS},
        {NM_ITEM, (STRPTR) "Delete Preferences...", NULL, 0, 0, (APTR)MID_DELETE_PREFS},
        {NM_ITEM, (STRPTR) "Export to Text...", (STRPTR) "E", 0, 0, (APTR)MID_EXPORT_TEXT},
        {NM_ITEM, (STRPTR)NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "Quit", (STRPTR) "Q", 0, 0, (APTR)MID_QUIT},
        {NM_END, NULL, NULL, 0, 0, NULL}};

    /* Page DiskInfo */
    page_diskinfo = CreateDiskInfoPage();

    LOG_DEBUG("CreateMainLayout: page0=%p page_diskinfo=%p page1=%p page2=%p page3=%p page4=%p page5=%p",
              (void *)page0, (void *)page_diskinfo, (void *)page1, (void *)page2,
              (void *)page3, (void *)page4, (void *)page5);
    LOG_DEBUG("CreateMainLayout: PageAvailable=%d tab_list=%p (head=%p)",
              (int)ui.PageAvailable, (void *)tab_list, (void *)IExec->GetHead(tab_list));
    LOG_DEBUG("CreateMainLayout: TextEditorClass=%p log_editor=%p log_vscroll=%p",
              (void *)ui.TextEditorClass, (void *)ui.log_editor, (void *)ui.log_vscroll);
    LOG_DEBUG("CreateMainLayout: viz_chart_type=%p viz_canvas=%p viz_reload=%p viz_color_by=%p",
              (void *)ui.viz_chart_type, (void *)ui.viz_canvas,
              (void *)ui.viz_reload_button, (void *)ui.viz_color_by_display);

    Object *main_content = NULL;
    if (ui.PageAvailable && page0 && page_diskinfo && page1 && page2 && page3 && page4 && page5 && tab_list) {
        ui.page_obj = IIntuition->NewObject(NULL, "page.gadget", PAGE_Add, (uint32)page0, PAGE_Add,
                                            (uint32)page_diskinfo, PAGE_Add, (uint32)page1, PAGE_Add, (uint32)page2,
                                            PAGE_Add, (uint32)page4, /* Health before Bulk? Or same as tabs? */
                                            PAGE_Add, (uint32)page3, PAGE_Add, (uint32)page5, TAG_DONE);
        ui.tabs = ClickTabObject, GA_ID, GID_TABS, GA_RelVerify, TRUE, CLICKTAB_Labels, (uint32)tab_list,
        CLICKTAB_PageGroup, (uint32)ui.page_obj, End;
        main_content = ui.tabs;
        LOG_DEBUG("CreateMainLayout: page_obj=%p tabs=%p", (void *)ui.page_obj, (void *)ui.tabs);
    } else {
        LOG_DEBUG("CreateMainLayout: FALLBACK - Using vertical layout (components missing)");
        main_content = VLayoutObject, LAYOUT_AddChild, page0, LAYOUT_AddChild, page_diskinfo, LAYOUT_AddChild, page1,
        LAYOUT_AddChild, page2, LAYOUT_AddChild, page4, LAYOUT_AddChild, page3, LAYOUT_AddChild, page5, End;
        ui.tabs = NULL;
        ui.page_obj = NULL;
    }

    /* Wrap everything in a VLayout to add the Bottom Bar Traffic Light */
    Object *root_layout = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, main_content, CHILD_WeightedHeight,
           100,

           /* Bottom Bar with Traffic Light */
        LAYOUT_AddChild, HLayoutObject, LAYOUT_BevelStyle, BVS_GROUP, LAYOUT_SpaceInner, TRUE, LAYOUT_AddChild,
           SpaceObject, End, /* Spacer to push stuff to right */
        LAYOUT_AddChild,
           (ui.traffic_label = ButtonObject, GA_ID, GID_TRAFFIC_LABEL, GA_Text, "Ready!", GA_ReadOnly, TRUE,
            BUTTON_BevelStyle, BVS_NONE, BUTTON_Transparent, TRUE, End),
           CHILD_WeightedWidth, 0, CHILD_MinWidth, 100, LAYOUT_AddChild,
           (ui.traffic_light = SpaceObject, GA_ID, GID_TRAFFIC_LIGHT, SPACE_MinWidth, 20, SPACE_MinHeight, 20, End),
           CHILD_WeightedWidth, 0, End, CHILD_WeightedHeight, 0, End;

    LOG_DEBUG("CreateMainLayout: Traffic Label assigned to %p", ui.traffic_label);

    Object *win_obj = WindowObject, WA_Title, APP_VER_TITLE, WA_SizeGadget, TRUE, WA_CloseGadget, TRUE, WA_DepthGadget,
           TRUE, WA_DragBar, TRUE, WA_Activate, TRUE, WINDOW_GadgetHelp, TRUE, WA_ReportMouse, TRUE, WA_IDCMP,
           IDCMP_MOUSEMOVE | IDCMP_VANILLAKEY | IDCMP_RAWKEY,
           /* Report mouse moves for graph hover */ WA_InnerWidth, 600, WA_InnerHeight, 500, WINDOW_Application,
           (uint32)ui.app_id, WINDOW_SharedPort, (uint32)ui.gui_port, WINDOW_IconifyGadget, TRUE, WINDOW_Iconifiable,
           TRUE, WINDOW_Icon, (uint32)icon, WINDOW_IconNoDispose, TRUE, WINDOW_NewMenu, (uint32)menu_data,
           WINDOW_ParentGroup, root_layout, /* Changed from main_content to root_layout */
        End;

    return win_obj;
}
