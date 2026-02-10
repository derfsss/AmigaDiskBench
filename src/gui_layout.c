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

#include "gui_internal.h"
#include <interfaces/intuition.h>
#include <reaction/reaction_macros.h>

static struct ColumnInfo bench_cols[] = {{100, "Date", CIF_FIXED | CIF_DRAGGABLE},
                                         {80, "Volume", CIF_FIXED | CIF_DRAGGABLE},
                                         {100, "Test Type", CIF_FIXED | CIF_DRAGGABLE},
                                         {80, "Block Size", CIF_FIXED | CIF_DRAGGABLE},
                                         {100, "No. of Passes", CIF_FIXED | CIF_DRAGGABLE},
                                         {120, "Average (MB/s)", CIF_FIXED | CIF_DRAGGABLE},
                                         {60, "IOPS", CIF_FIXED | CIF_DRAGGABLE},
                                         {80, "Device", CIF_FIXED | CIF_DRAGGABLE},
                                         {40, "Unit", CIF_FIXED | CIF_DRAGGABLE},
                                         {120, "App Version", CIF_FIXED | CIF_DRAGGABLE},
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
        /* Benchmark Actions Group */
        LAYOUT_AddChild, VLayoutObject, LAYOUT_Label, "Benchmark Actions", LAYOUT_BevelStyle, BVS_GROUP,
           LAYOUT_AddChild,
           (ui.run_button = ButtonObject, GA_ID, GID_RUN_ALL, GA_RelVerify, TRUE, GA_Text,
            GetString(8, "Run Benchmark"), End),
           CHILD_WeightedWidth, 0, CHILD_WeightedHeight, 0, CHILD_MinWidth, 160, CHILD_MinHeight, 40, End,
           CHILD_WeightedHeight, 0, /* End Group "Benchmark Actions" */
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
            LISTBROWSER_AutoFit, TRUE, LISTBROWSER_ShowSelected, TRUE, LISTBROWSER_HorizontalProp, TRUE, End),
           CHILD_WeightedHeight, 100, LAYOUT_AddChild, HLayoutObject, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_REFRESH_HISTORY, GA_Text, GetString(9, "Refresh History"), End, LAYOUT_AddChild, ButtonObject, GA_ID,
           GID_VIEW_REPORT, GA_Text, GetString(10, "Global Report"), End, End, CHILD_WeightedHeight, 0, End;

    static struct NewMenu menu_data[] = {
        {NM_TITLE, (STRPTR) "Project", NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "About...", (STRPTR) "A", 0, 0, (APTR)MID_ABOUT},
        {NM_ITEM, (STRPTR) "Preferences...", (STRPTR) "P", 0, 0, (APTR)MID_PREFS},
        {NM_ITEM, (STRPTR) "Delete Preferences...", NULL, 0, 0, (APTR)MID_DELETE_PREFS},
        {NM_ITEM, (STRPTR)NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, (STRPTR) "Quit", (STRPTR) "Q", 0, 0, (APTR)MID_QUIT},
        {NM_END, NULL, NULL, 0, 0, NULL}};

    Object *main_content = NULL;
    if (ui.PageAvailable && page0 && page1 && tab_list) {
        ui.page_obj =
            IIntuition->NewObject(NULL, "page.gadget", PAGE_Add, (uint32)page0, PAGE_Add, (uint32)page1, TAG_DONE);
        ui.tabs = ClickTabObject, GA_ID, GID_TABS, GA_RelVerify, TRUE, CLICKTAB_Labels, (uint32)tab_list,
        CLICKTAB_PageGroup, (uint32)ui.page_obj, End;
        main_content = ui.tabs;
    } else {
        LOG_DEBUG("CreateMainLayout: Using vertical fallback layout (components missing)");
        main_content = VLayoutObject, LAYOUT_AddChild, page0, LAYOUT_AddChild, page1, End;
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
