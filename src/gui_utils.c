#include "gui_internal.h"
#include <stdio.h>

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

void FormatSize(uint64 bytes, char *out)
{
    if (bytes >= (1024ULL * 1024 * 1024 * 1024))
        sprintf(out, "%.1f TB", (double)bytes / (1024.0 * 1024 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024 * 1024))
        sprintf(out, "%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024))
        sprintf(out, "%.1f MB", (double)bytes / (1024.0 * 1024));
    else
        sprintf(out, "%llu KB", bytes / 1024);
}

CONST_STRPTR GetString(uint32 id, CONST_STRPTR default_str)
{
    return (ui.catalog && ui.ILoc) ? ui.ILoc->GetCatalogStr(ui.catalog, id, default_str) : default_str;
}
