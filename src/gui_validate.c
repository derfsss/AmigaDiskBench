/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include "gui_internal.h"
#include "gui_validate.h"
#include "viz_profile.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Known section names */
static const char *known_sections[] = {
    "Profile", "XAxis", "YAxis", "Series", "Filters",
    "Overlay", "Annotations", "Colors", "TrendLine", NULL
};

/* Known keys per section */
static const char *profile_keys[] = {"Name", "Description", "ChartType", NULL};
static const char *xaxis_keys[] = {"Source", "Label", "Format", NULL};
static const char *yaxis_keys[] = {"Source", "Label", "AutoScale", "Min", "Max", NULL};
static const char *series_keys[] = {"GroupBy", "MaxSeries", "DefaultDateRange", "SortX", "Collapse", NULL};
static const char *filters_keys[] = {
    "DefaultDateRange",
    "ExcludeTest", "IncludeTest", "ExcludeBlockSize", "IncludeBlockSize",
    "ExcludeVolume", "IncludeVolume", "ExcludeFilesystem", "IncludeFilesystem",
    "ExcludeHardware", "IncludeHardware", "ExcludeVendor", "IncludeVendor",
    "ExcludeProduct", "IncludeProduct", "ExcludeAveraging", "IncludeAveraging",
    "ExcludeVersion", "IncludeVersion", "MinVersion",
    "MinPasses", "MinMBs", "MaxMBs", "MinDurationSecs", "MaxDurationSecs", NULL
};
static const char *overlay_keys[] = {"SecondaryAxis", "SecondarySource", "SecondaryLabel", NULL};
static const char *annotations_keys[] = {"ReferenceLine", NULL};
static const char *colors_keys[] = {"Color", NULL};
static const char *trendline_keys[] = {"Style", "Window", "Degree", "PerSeries", NULL};

/* Case-insensitive string compare */
static int ci_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static BOOL is_known_section(const char *name)
{
    for (int i = 0; known_sections[i]; i++) {
        if (ci_cmp(name, known_sections[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL is_known_key(const char *key, const char **key_list)
{
    if (!key_list) return TRUE;
    for (int i = 0; key_list[i]; i++) {
        if (ci_cmp(key, key_list[i]) == 0) return TRUE;
    }
    return FALSE;
}

static const char **keys_for_section(const char *section)
{
    if (ci_cmp(section, "Profile") == 0)     return profile_keys;
    if (ci_cmp(section, "XAxis") == 0)       return xaxis_keys;
    if (ci_cmp(section, "YAxis") == 0)       return yaxis_keys;
    if (ci_cmp(section, "Series") == 0)      return series_keys;
    if (ci_cmp(section, "Filters") == 0)     return filters_keys;
    if (ci_cmp(section, "Overlay") == 0)     return overlay_keys;
    if (ci_cmp(section, "Annotations") == 0) return annotations_keys;
    if (ci_cmp(section, "Colors") == 0)      return colors_keys;
    if (ci_cmp(section, "TrendLine") == 0)   return trendline_keys;
    return NULL;
}

static void AddFinding(struct List *list, uint32 line, char severity, const char *fmt, ...)
{
    VizFinding *f = (VizFinding *)IExec->AllocVecTags(sizeof(VizFinding),
        AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0, TAG_DONE);
    if (!f) return;
    f->line_number = line;
    f->severity = severity;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(f->message, sizeof(f->message), fmt, ap);
    va_end(ap);

    IExec->AddTail(list, &f->node);
}

/**
 * @brief Validate a single .viz file, appending findings to the list.
 */
static void ValidateVizFile(const char *path, struct List *findings,
                            uint32 *errors, uint32 *warnings)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        AddFinding(findings, 0, 'E', "%s: Cannot open file", path);
        (*errors)++;
        return;
    }

    char line[512];
    uint32 lineno = 0;
    char current_section[64] = "";
    BOOL has_profile = FALSE;
    BOOL has_name = FALSE;
    BOOL has_chart_type = FALSE;
    BOOL has_y_source = FALSE;
    BOOL has_x_source = FALSE;

    /* Extract filename from path for display */
    const char *fname = path;
    const char *p = path;
    while (*p) {
        if (*p == '/' || *p == ':') fname = p + 1;
        p++;
    }

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        /* Skip comments and blank lines */
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == ';' || *s == '\0')
            continue;

        /* Section header */
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Malformed section (missing ']')", fname, lineno);
                (*errors)++;
                continue;
            }
            *end = '\0';
            snprintf(current_section, sizeof(current_section), "%s", s + 1);
            if (!is_known_section(current_section)) {
                AddFinding(findings, lineno, 'W', "%s:%lu: Unknown section [%s]", fname, lineno, current_section);
                (*warnings)++;
            }
            if (ci_cmp(current_section, "Profile") == 0)
                has_profile = TRUE;
            continue;
        }

        /* Key = Value */
        char *eq = strchr(s, '=');
        if (!eq) {
            AddFinding(findings, lineno, 'W', "%s:%lu: Line has no '=' separator", fname, lineno);
            (*warnings)++;
            continue;
        }

        /* Extract key */
        char key[64];
        uint32 klen = (uint32)(eq - s);
        while (klen > 0 && (s[klen - 1] == ' ' || s[klen - 1] == '\t')) klen--;
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, s, klen);
        key[klen] = '\0';

        /* Extract value */
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        /* Strip trailing whitespace */
        uint32 vlen = strlen(val);
        while (vlen > 0 && (val[vlen - 1] == ' ' || val[vlen - 1] == '\t')) vlen--;
        val[vlen] = '\0';
        /* Strip quotes */
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val++;
            vlen -= 2;
            val[vlen] = '\0';
        }

        /* Check key is known for current section */
        const char **kl = keys_for_section(current_section);
        if (kl && !is_known_key(key, kl)) {
            AddFinding(findings, lineno, 'W', "%s:%lu: Unknown key '%s' in [%s]",
                       fname, lineno, key, current_section);
            (*warnings)++;
        }

        /* Specific value validation */
        if (ci_cmp(current_section, "Profile") == 0) {
            if (ci_cmp(key, "Name") == 0) {
                has_name = TRUE;
                if (vlen == 0) {
                    AddFinding(findings, lineno, 'E', "%s:%lu: Name is empty", fname, lineno);
                    (*errors)++;
                }
            }
            if (ci_cmp(key, "ChartType") == 0) {
                has_chart_type = TRUE;
                if (ci_cmp(val, "line") != 0 && ci_cmp(val, "bar") != 0 && ci_cmp(val, "hybrid") != 0) {
                    AddFinding(findings, lineno, 'E', "%s:%lu: Unknown ChartType '%s' (expected line/bar/hybrid)",
                               fname, lineno, val);
                    (*errors)++;
                }
            }
        }
        if (ci_cmp(current_section, "XAxis") == 0 && ci_cmp(key, "Source") == 0) {
            has_x_source = TRUE;
            if (ci_cmp(val, "block_size") != 0 && ci_cmp(val, "timestamp") != 0 && ci_cmp(val, "test_index") != 0) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Unknown XAxis Source '%s'", fname, lineno, val);
                (*errors)++;
            }
        }
        if (ci_cmp(current_section, "YAxis") == 0 && ci_cmp(key, "Source") == 0) {
            has_y_source = TRUE;
            if (ci_cmp(val, "mb_per_sec") != 0 && ci_cmp(val, "iops") != 0 &&
                ci_cmp(val, "min_mbps") != 0 && ci_cmp(val, "max_mbps") != 0 &&
                ci_cmp(val, "duration_secs") != 0 && ci_cmp(val, "total_bytes") != 0) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Unknown YAxis Source '%s'", fname, lineno, val);
                (*errors)++;
            }
        }
        if (ci_cmp(current_section, "Colors") == 0 && ci_cmp(key, "Color") == 0) {
            /* Validate hex color format */
            if (vlen != 6) {
                AddFinding(findings, lineno, 'W', "%s:%lu: Color '%s' should be 6 hex digits", fname, lineno, val);
                (*warnings)++;
            } else {
                for (uint32 i = 0; i < 6; i++) {
                    char c = val[i];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        AddFinding(findings, lineno, 'W', "%s:%lu: Color '%s' contains non-hex char", fname, lineno, val);
                        (*warnings)++;
                        break;
                    }
                }
            }
        }
        if (ci_cmp(current_section, "TrendLine") == 0 && ci_cmp(key, "Style") == 0) {
            if (ci_cmp(val, "none") != 0 && ci_cmp(val, "linear") != 0 &&
                ci_cmp(val, "moving_average") != 0 && ci_cmp(val, "polynomial") != 0) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Unknown TrendLine Style '%s'", fname, lineno, val);
                (*errors)++;
            }
        }
        if (ci_cmp(current_section, "Series") == 0 && ci_cmp(key, "GroupBy") == 0) {
            if (ci_cmp(val, "drive") != 0 && ci_cmp(val, "test_type") != 0 &&
                ci_cmp(val, "block_size") != 0 && ci_cmp(val, "filesystem") != 0 &&
                ci_cmp(val, "hardware") != 0 && ci_cmp(val, "vendor") != 0 &&
                ci_cmp(val, "app_version") != 0 && ci_cmp(val, "averaging_method") != 0) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Unknown GroupBy '%s'", fname, lineno, val);
                (*errors)++;
            }
        }
        if (ci_cmp(current_section, "Series") == 0 && ci_cmp(key, "Collapse") == 0) {
            if (ci_cmp(val, "none") != 0 && ci_cmp(val, "mean") != 0 &&
                ci_cmp(val, "median") != 0 && ci_cmp(val, "min") != 0 && ci_cmp(val, "max") != 0) {
                AddFinding(findings, lineno, 'E', "%s:%lu: Unknown Collapse '%s'", fname, lineno, val);
                (*errors)++;
            }
        }
    }

    fclose(fp);

    /* Check required fields */
    if (!has_profile) {
        AddFinding(findings, 0, 'E', "%s: Missing required [Profile] section", fname);
        (*errors)++;
    }
    if (!has_name) {
        AddFinding(findings, 0, 'E', "%s: Missing required Profile Name", fname);
        (*errors)++;
    }
    if (!has_chart_type) {
        AddFinding(findings, 0, 'W', "%s: No ChartType specified (defaults to 'line')", fname);
        (*warnings)++;
    }
    if (!has_x_source) {
        AddFinding(findings, 0, 'W', "%s: No XAxis Source specified (defaults to 'block_size')", fname);
        (*warnings)++;
    }
    if (!has_y_source) {
        AddFinding(findings, 0, 'W', "%s: No YAxis Source specified (defaults to 'mb_per_sec')", fname);
        (*warnings)++;
    }
}

/**
 * @brief Scan the Visualizations directory and validate all .viz files.
 */
static void ValidateVizDirectory(struct List *findings, uint32 *total_files,
                                 uint32 *total_errors, uint32 *total_warnings)
{
    *total_files = 0;
    *total_errors = 0;
    *total_warnings = 0;

    BPTR lock = IDOS->Lock("PROGDIR:Visualizations", ACCESS_READ);
    if (!lock) {
        AddFinding(findings, 0, 'E', "Cannot access PROGDIR:Visualizations/ folder");
        (*total_errors)++;
        return;
    }

    APTR context = IDOS->ObtainDirContextTags(EX_FileLockInput, lock,
        EX_DataFields, EXF_NAME | EXF_TYPE, TAG_DONE);
    if (!context) {
        IDOS->UnLock(lock);
        AddFinding(findings, 0, 'E', "Cannot read Visualizations/ directory");
        (*total_errors)++;
        return;
    }

    struct ExamineData *data;
    while ((data = IDOS->ExamineDir(context)) != NULL) {
        if (EXD_IS_FILE(data)) {
            uint32 nlen = strlen(data->Name);
            if (nlen > 4 && nlen < 230 && strcmp(&data->Name[nlen - 4], ".viz") == 0) {
                char fullpath[256];
                snprintf(fullpath, sizeof(fullpath), "PROGDIR:Visualizations/%.230s", data->Name);
                (*total_files)++;
                ValidateVizFile(fullpath, findings, total_errors, total_warnings);
            }
        }
    }

    IDOS->ReleaseDirContext(context);
    IDOS->UnLock(lock);

    if (*total_files == 0) {
        AddFinding(findings, 0, 'W', "No .viz files found in Visualizations/ folder");
        (*total_warnings)++;
    }
}

/**
 * @brief Print validation report to stdout (Shell mode).
 */
static void PrintShellReport(struct List *findings, uint32 total_files,
                             uint32 total_errors, uint32 total_warnings)
{
    IDOS->Printf("AmigaDiskBench Visualization Profile Validator\n");
    IDOS->Printf("=============================================\n\n");
    IDOS->Printf("Scanned %lu .viz files\n\n", total_files);

    struct Node *node = IExec->GetHead(findings);
    while (node) {
        VizFinding *f = (VizFinding *)node;
        IDOS->Printf("[%s] %s\n",
            f->severity == 'E' ? "ERROR" : "WARN ",
            f->message);
        node = IExec->GetSucc(node);
    }

    IDOS->Printf("\n--- Summary: %lu errors, %lu warnings ---\n", total_errors, total_warnings);
}

/**
 * @brief Show validation report in a ReAction window (Workbench mode).
 */
static void ShowValidateWindow(struct List *findings, uint32 total_files,
                               uint32 total_errors, uint32 total_warnings)
{
    /* Build ListBrowser label list */
    struct List lb_list;
    IExec->NewList(&lb_list);

    /* Header */
    char header[128];
    snprintf(header, sizeof(header), "Scanned %lu files: %lu errors, %lu warnings",
             (unsigned long)total_files, (unsigned long)total_errors, (unsigned long)total_warnings);
    struct Node *hdr_node = IListBrowser->AllocListBrowserNode(2,
        LBNA_Flags, LBFLG_READONLY,
        LBNCA_CopyText, TRUE, LBNCA_Text, "INFO",
        LBNCA_CopyText, TRUE, LBNCA_Text, header,
        TAG_DONE);
    if (hdr_node) IExec->AddTail(&lb_list, hdr_node);

    /* Findings */
    struct Node *fnode = IExec->GetHead(findings);
    while (fnode) {
        VizFinding *f = (VizFinding *)fnode;
        const char *sev = f->severity == 'E' ? "ERROR" : "WARN";
        struct Node *n = IListBrowser->AllocListBrowserNode(2,
            LBNA_Flags, LBFLG_READONLY,
            LBNCA_CopyText, TRUE, LBNCA_Text, sev,
            LBNCA_CopyText, TRUE, LBNCA_Text, f->message,
            TAG_DONE);
        if (n) IExec->AddTail(&lb_list, n);
        fnode = IExec->GetSucc(fnode);
    }

    /* Create column info — proportional widths */
    struct ColumnInfo ci[] = {
        {10, "Type", 0},
        {90, "Message", 0},
        {-1, NULL, 0}
    };

    /* Create window */
    Object *win_obj = WindowObject,
        WA_Title, "AmigaDiskBench - Profile Validation",
        WA_Width, 600,
        WA_Height, 400,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_CloseGadget, TRUE,
        WA_Activate, TRUE,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_Layout, VLayoutObject,
            LAYOUT_SpaceOuter, TRUE,
            LAYOUT_DeferLayout, TRUE,
            LAYOUT_AddChild, ListBrowserObject,
                GA_ReadOnly, TRUE,
                LISTBROWSER_Labels, &lb_list,
                LISTBROWSER_ColumnInfo, ci,
                LISTBROWSER_ColumnTitles, TRUE,
                LISTBROWSER_Striping, LBS_ROWS,
                LISTBROWSER_AutoFit, TRUE,
                LISTBROWSER_HorizontalProp, TRUE,
            End,
        End,
    End;

    if (!win_obj) {
        IListBrowser->FreeListBrowserList(&lb_list);
        return;
    }

    struct Window *win = (struct Window *)IIntuition->IDoMethod(win_obj, WM_OPEN);
    if (!win) {
        IIntuition->DisposeObject(win_obj);
        IListBrowser->FreeListBrowserList(&lb_list);
        return;
    }

    /* Simple event loop — only handle close */
    BOOL running = TRUE;
    uint32 wsig = 0;
    IIntuition->GetAttr(WINDOW_SigMask, win_obj, &wsig);

    while (running) {
        uint32 sigs = IExec->Wait(wsig | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;

        uint32 result = WMHI_LASTMSG;
        while ((result = IIntuition->IDoMethod(win_obj, WM_HANDLEINPUT, &result)) != WMHI_LASTMSG) {
            if ((result & WMHI_CLASSMASK) == WMHI_CLOSEWINDOW)
                running = FALSE;
        }
    }

    IIntuition->IDoMethod(win_obj, WM_CLOSE);
    IIntuition->DisposeObject(win_obj);
    IListBrowser->FreeListBrowserList(&lb_list);
}

void RunValidation(void)
{
    struct List findings;
    IExec->NewList(&findings);
    uint32 total_files = 0, total_errors = 0, total_warnings = 0;

    ValidateVizDirectory(&findings, &total_files, &total_errors, &total_warnings);

    /* Detect context: Shell vs Workbench (Cli() is reliable; Output() can be
     * non-NULL from Workbench if a default console is configured) */
    if (IDOS->Cli()) {
        /* Shell mode */
        PrintShellReport(&findings, total_files, total_errors, total_warnings);
    } else {
        /* Workbench mode */
        ShowValidateWindow(&findings, total_files, total_errors, total_warnings);
    }

    /* Free findings */
    struct Node *node;
    while ((node = IExec->RemHead(&findings)) != NULL) {
        IExec->FreeVec(node);
    }
}
