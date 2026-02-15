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

#include "gui_internal.h"

static struct ColumnInfo bench_cols[] = {{100, "Date", CIF_SORTABLE | CIF_DRAGGABLE},
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

static struct ColumnInfo bulk_cols[] = {{20, "", CIF_FIXED}, /* Checkbox */
                                        {150, "Volume", CIF_FIXED | CIF_DRAGGABLE},
                                        {100, "FileSystem", CIF_FIXED | CIF_DRAGGABLE},
                                        {1, "", CIF_FIXED},
                                        {-1, NULL, 0}};

Object *CreateMainLayout(struct DiskObject *icon, struct List *tab_list)
{
    /* Refactored Page 0 (Benchmark) using standard ReAction macros */
    Object *page0 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, VLayoutObject, LAYOUT_Label,
           GetString(3, "Benchmark Control"), LAYOUT_BevelStyle, BVS_GROUP,
           /* Drive Selector */
        LAYOUT_AddChild,
           (ui.target_chooser = ChooserObject, GA_ID, GID_VOL_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Labels,
            (uint32)&ui.drive_list, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(4, "Drive:"), End, CHILD_WeightedHeight, 0,
           /* Test Type Selector */
        LAYOUT_AddChild,
           (ui.test_chooser = ChooserObject, GA_ID, GID_TEST_CHOOSER, GA_RelVerify, TRUE, CHOOSER_Selected, 0,
            CHOOSER_Labels, (uint32)&ui.test_labels, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(5, "Test Type:"), End, CHILD_WeightedHeight, 0,
           /* Passes */
        LAYOUT_AddChild,
           (ui.pass_gad = IntegerObject, GA_ID, GID_NUM_PASSES, GA_RelVerify, TRUE, INTEGER_Minimum, 3, INTEGER_Maximum,
            20, INTEGER_Number, 3, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(6, "Passes:"), End, CHILD_WeightedHeight, 0,
           /* Block Size */
        LAYOUT_AddChild,
           (ui.block_chooser = ChooserObject, GA_ID, GID_BLOCK_SIZE, GA_RelVerify, TRUE, CHOOSER_Selected, 0,
            CHOOSER_Labels, (uint32)&ui.block_list, End),
           CHILD_Label, LabelObject, LABEL_Text, GetString(7, "Block Size:"), End, CHILD_WeightedHeight, 0, End,
           CHILD_WeightedHeight, 0, /* End Group "Benchmark Control" */

        /* Volume Information Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Volume Information", LAYOUT_BevelStyle, BVS_GROUP,
           LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, LabelObject, LABEL_Text, "Size:", End, LAYOUT_AddChild,
           (ui.vol_size_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", End), CHILD_WeightedWidth, 100, End,
           LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, LabelObject, LABEL_Text, "Free:", End, LAYOUT_AddChild,
           (ui.vol_free_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", End), CHILD_WeightedWidth, 100, End,
           LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, LabelObject, LABEL_Text, "Filesystem:", End,
           LAYOUT_AddChild, (ui.vol_fs_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", End),
           CHILD_WeightedWidth, 100, End, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, LabelObject, LABEL_Text,
           "Device:", End, LAYOUT_AddChild,
           (ui.vol_device_label = ButtonObject, GA_ReadOnly, TRUE, GA_Text, "N/A", End), CHILD_WeightedWidth, 100, End,
           End, CHILD_WeightedHeight, 0,

           /* Benchmark Actions Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Benchmark Actions", LAYOUT_BevelStyle, BVS_GROUP,
           LAYOUT_ShrinkWrap, TRUE, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild,
           (ui.run_button = ButtonObject, GA_ID, GID_RUN_ALL, GA_RelVerify, TRUE, GA_Text,
            GetString(8, "Run Benchmark"), End),
           LAYOUT_AddChild, ButtonObject, GA_ID, GID_REFRESH_DRIVES, GA_RelVerify, TRUE, GA_Text, "Refresh Drives", End,
           End, CHILD_WeightedHeight, 0, End, /* End Group "Benchmark Actions" */
        CHILD_WeightedHeight, 0,              /* Fix: Prevent group from expanding vertically in main page */
        /* Benchmark List */
        LAYOUT_AddChild,
           (ui.bench_list = ListBrowserObject, GA_ID, GID_CURRENT_RESULTS, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
            (uint32)bench_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.bench_labels,
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
           CHILD_WeightedHeight, 100, End;

    /* Refactored Page 1 (History) using standard ReAction macros */
    Object *page1 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild,
           (ui.history_list = ListBrowserObject, GA_ID, GID_HISTORY_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
            (uint32)bench_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.history_labels,
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE,
            LISTBROWSER_MultiSelect, TRUE, End),
           CHILD_WeightedHeight, 100, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_REFRESH_HISTORY, GA_Text, GetString(9, "Refresh History"), End, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_VIEW_REPORT, GA_Text, GetString(10, "Global Report"), End, LAYOUT_AddChild,
           (ui.compare_button = ButtonObject, GA_ID, GID_HISTORY_COMPARE, GA_Text, "Compare Selected", GA_Disabled,
            TRUE, End),
           LAYOUT_AddChild, ButtonObject, GA_ID, GID_HISTORY_DELETE, GA_Text, "Delete Selected", End, LAYOUT_AddChild,
           ButtonObject, GA_ID, GID_HISTORY_CLEAR_ALL, GA_Text, "Clear All", End, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_HISTORY_EXPORT, GA_Text, "Export", End, End, CHILD_WeightedHeight, 0, End;

    /* Page 2 (Visualization) - History Trend Graph */
    Object *page2 = VLayoutObject, LAYOUT_SpaceOuter, TRUE,

           /* Filter Controls */
        LAYOUT_AddChild, HLayoutObject, LAYOUT_Label, "Filters", LAYOUT_BevelStyle, BVS_GROUP,

           LAYOUT_AddChild,
           (ui.viz_filter_volume = ChooserObject, GA_ID, GID_VIZ_FILTER_VOLUME, GA_RelVerify, TRUE, CHOOSER_Labels,
            (uint32)&ui.viz_volume_labels, End),
           CHILD_Label, LabelObject, LABEL_Text, "Volume:", End,

           LAYOUT_AddChild,
           (ui.viz_filter_test = ChooserObject, GA_ID, GID_VIZ_FILTER_TEST, GA_RelVerify, TRUE, CHOOSER_Labels,
            (uint32)&ui.viz_test_labels, End),
           CHILD_Label, LabelObject, LABEL_Text, "Test:", End,

           LAYOUT_AddChild,
           (ui.viz_filter_metric = ChooserObject, GA_ID, GID_VIZ_FILTER_METRIC, GA_RelVerify, TRUE, CHOOSER_Labels,
            (uint32)&ui.viz_metric_labels, End),
           CHILD_Label, LabelObject, LABEL_Text, "Metric:", End, End, CHILD_WeightedHeight, 0,

           /* Graph Canvas */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Performance Trend", LAYOUT_BevelStyle, BVS_GROUP,

           LAYOUT_AddChild,
           (ui.viz_canvas = SpaceObject, GA_ID, GID_VIZ_CANVAS, SPACE_MinWidth, 400, SPACE_MinHeight, 250,
            SPACE_BevelStyle, BVS_FIELD, SPACE_Transparent, TRUE, SpaceEnd),
           CHILD_WeightedHeight, 100, End, CHILD_WeightedHeight, 100, End;

    /* Page 3 (Bulk Testing) */
    Object *page3 = VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild,
           (ui.bulk_list = ListBrowserObject, GA_ID, GID_BULK_LIST, GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo,
            (uint32)bulk_cols, LISTBROWSER_ColumnTitles, TRUE, LISTBROWSER_Labels, (uint32)&ui.bulk_labels,
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
           CHILD_WeightedHeight, 100,
           /* Queued Job Settings & Options */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Queued Job Settings", LAYOUT_BevelStyle, BVS_GROUP,
           LAYOUT_AddChild,
           (ui.bulk_info_label = ButtonObject, GA_ID, GID_BULK_INFO, GA_ReadOnly, TRUE, GA_Text, "Init...", End),
           LAYOUT_AddChild,
           (ui.bulk_all_tests_check = CheckBoxObject, GA_ID, GID_BULK_ALL_TESTS, GA_RelVerify, TRUE, GA_Text,
            "Run All Test Types (Sprinter..Profiler)", CHECKBOX_Checked, FALSE, End),
           LAYOUT_AddChild,
           (ui.bulk_all_blocks_check = CheckBoxObject, GA_ID, GID_BULK_ALL_BLOCKS, GA_RelVerify, TRUE, GA_Text,
            "Run All Block Sizes (4K..1M)", CHECKBOX_Checked, FALSE, End),
           End, CHILD_WeightedHeight, 0, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_BULK_RUN, GA_Text, "Run Bulk Benchmark on Selected", End, End, CHILD_WeightedHeight, 0, End;

    static struct NewMenu menu_data[] = {
        {NM_TITLE, (STRPTR) "Project", NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "About...", (STRPTR) "A", 0, 0, (APTR)MID_ABOUT},
        {NM_ITEM, (STRPTR) "Preferences...", (STRPTR) "P", 0, 0, (APTR)MID_PREFS},
        {NM_ITEM, (STRPTR) "Delete Preferences...", NULL, 0, 0, (APTR)MID_DELETE_PREFS},
        {NM_ITEM, (STRPTR) "Export to Text...", (STRPTR) "E", 0, 0, (APTR)MID_EXPORT_TEXT},
        {NM_ITEM, (STRPTR)NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "Quit", (STRPTR) "Q", 0, 0, (APTR)MID_QUIT},
        {NM_END, NULL, NULL, 0, 0, NULL}};

    Object *main_content = NULL;
    if (ui.PageAvailable && page0 && page1 && page2 && page3 && tab_list) {
        ui.page_obj = IIntuition->NewObject(NULL, "page.gadget", PAGE_Add, (uint32)page0, PAGE_Add, (uint32)page1,
                                            PAGE_Add, (uint32)page2, PAGE_Add, (uint32)page3, TAG_DONE);
        ui.tabs = ClickTabObject, GA_ID, GID_TABS, GA_RelVerify, TRUE, CLICKTAB_Labels, (uint32)tab_list,
        CLICKTAB_PageGroup, (uint32)ui.page_obj, End;
        main_content = ui.tabs;
    } else {
        LOG_DEBUG("CreateMainLayout: Using vertical fallback layout (components missing)");
        main_content = VLayoutObject, LAYOUT_AddChild, page0, LAYOUT_AddChild, page1, LAYOUT_AddChild, page2,
        LAYOUT_AddChild, page3, End;
        ui.tabs = NULL;
        ui.page_obj = NULL;
    }

    Object *win_obj = WindowObject, WA_Title, APP_VER_TITLE, WA_SizeGadget, TRUE, WA_CloseGadget, TRUE, WA_DepthGadget,
           TRUE, WA_DragBar, TRUE, WA_Activate, TRUE, WA_InnerWidth, 600, WA_InnerHeight, 500, WINDOW_Application,
           (uint32)ui.app_id, WINDOW_SharedPort, (uint32)ui.gui_port, WINDOW_IconifyGadget, TRUE, WINDOW_Iconifiable,
           TRUE, WINDOW_Icon, (uint32)icon, WINDOW_IconNoDispose, TRUE, WINDOW_NewMenu, (uint32)menu_data,
           WINDOW_ParentGroup, VLayoutObject, LAYOUT_SpaceOuter, TRUE, LAYOUT_AddChild, (uint32)main_content,
           CHILD_WeightedHeight, 100, End, End;

    return win_obj;
}
