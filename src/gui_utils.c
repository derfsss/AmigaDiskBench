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

const char *FormatPresetBlockSize(uint32 bytes)
{
    if (bytes == 4096)
        return "4K";
    if (bytes == 16384)
        return "16K";
    if (bytes == 32768)
        return "32K";
    if (bytes == 65536)
        return "64K";
    if (bytes == 262144)
        return "256K";
    if (bytes == 1048576)
        return "1M";

    static char custom[32];
    if (bytes < 1024)
        snprintf(custom, sizeof(custom), "%uB", (unsigned int)bytes);
    else if (bytes < 1048576)
        snprintf(custom, sizeof(custom), "%uK", (unsigned int)(bytes / 1024));
    else
        snprintf(custom, sizeof(custom), "%uM", (unsigned int)(bytes / 1048576));

    return custom;
}

const char *FormatByteSize(uint64 bytes)
{
    static char buffer[64];
    if (bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1048576) {
        snprintf(buffer, sizeof(buffer), "%.2f KB", (double)bytes / 1024.0);
    } else if (bytes < 1073741824) {
        snprintf(buffer, sizeof(buffer), "%.2f MB", (double)bytes / 1048576.0);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f GB", (double)bytes / 1073741824.0);
    }
    return buffer;
}

void FormatSize(uint64 bytes, char *out, uint32 out_size)
{
    if (bytes >= (1024ULL * 1024 * 1024 * 1024))
        snprintf(out, out_size, "%.1f TB", (double)bytes / (1024.0 * 1024 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024 * 1024))
        snprintf(out, out_size, "%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024))
        snprintf(out, out_size, "%.1f MB", (double)bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        snprintf(out, out_size, "%llu KB", bytes / 1024);
    else
        snprintf(out, out_size, "%llu B", bytes);
}

CONST_STRPTR GetString(uint32 id, CONST_STRPTR default_str)
{
    return (ui.catalog && ui.ILoc) ? ui.ILoc->GetCatalogStr(ui.catalog, id, default_str) : default_str;
}

/**
 * ShowMessage
 *
 * Displays a standard ReAction EasyRequest message box.
 */
void ShowMessage(const char *title, const char *body, const char *gadgets)
{
    if (ui.window) {
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, (STRPTR)title, (STRPTR)body, (STRPTR)gadgets};
        IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE);
    }
}

/**
 * SetGadgetState
 *
 * Enables or disables a gadget by its GID.
 */
void SetGadgetState(uint16 gid, BOOL disabled)
{
    if (ui.window) {
        /* This helper assumes the pointer to the gadget is stored in ui under its standard name.
         * For more complex mapping, we might search the window's gadget list.
         * For now, we handle the most common ones.
         */
        Object *obj = NULL;
        switch (gid) {
        case GID_RUN_ALL:
            obj = ui.run_button;
            break;
        case GID_VOL_CHOOSER:
            obj = ui.target_chooser;
            break;
        case GID_TEST_CHOOSER:
            obj = ui.test_chooser;
            break;
        case GID_NUM_PASSES:
            obj = ui.pass_gad;
            break;
        case GID_BLOCK_SIZE:
            obj = ui.block_chooser;
            break;
        case GID_BULK_RUN:
            obj = ui.run_button; /* Reused for bulk */
            break;
        case GID_HISTORY_COMPARE:
            obj = ui.compare_button;
            break;
        }

        if (obj) {
            IIntuition->SetGadgetAttrs((struct Gadget *)obj, ui.window, NULL, GA_Disabled, (uint32)disabled, TAG_DONE);
        }
    }
}

/**
 * ShowConfirm
 *
 * Displays a standard ReAction EasyRequest message box and returns TRUE
 * if the primary (first) gadget was pressed.
 */
BOOL ShowConfirm(const char *title, const char *body, const char *gadgets)
{
    if (ui.window) {
        struct EasyStruct es = {sizeof(struct EasyStruct), 0, (STRPTR)title, (STRPTR)body, (STRPTR)gadgets};
        return (IIntuition->EasyRequest(ui.window, &es, NULL, TAG_DONE) == 1);
    }
    return FALSE;
}
