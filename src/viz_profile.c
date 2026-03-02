/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "viz_profile.h"
#include "debug.h"

VizProfile g_viz_profiles[MAX_VIZ_PROFILES];
uint32     g_viz_profile_count = 0;

/* --- String helpers --- */

static void TrimInPlace(char *str)
{
    char *src = str;
    char *end;

    while (*src == ' ' || *src == '\t') src++;
    if (src != str)
        memmove(str, src, strlen(src) + 1);

    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = '\0';
}

/* Strip surrounding quotes from a value string */
static void StripQuotes(char *str)
{
    uint32 len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"')
    {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

/* Case-insensitive string comparison using C stdlib */
static int StrCaseCmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Case-insensitive check if str ends with suffix */
static BOOL StrEndsWith(const char *str, const char *suffix)
{
    uint32 slen = strlen(str);
    uint32 xlen = strlen(suffix);
    if (xlen > slen) return FALSE;
    return StrCaseCmp(str + slen - xlen, suffix) == 0;
}

/* --- Enum parsers --- */

static VizChartType ParseChartType(const char *val)
{
    if (StrCaseCmp(val, "line") == 0)   return VIZ_CHART_LINE;
    if (StrCaseCmp(val, "bar") == 0)    return VIZ_CHART_BAR;
    if (StrCaseCmp(val, "hybrid") == 0) return VIZ_CHART_HYBRID;
    return VIZ_CHART_LINE; /* default */
}

static VizXSource ParseXSource(const char *val)
{
    if (StrCaseCmp(val, "block_size") == 0)  return VIZ_SRC_BLOCK_SIZE;
    if (StrCaseCmp(val, "timestamp") == 0)   return VIZ_SRC_TIMESTAMP;
    if (StrCaseCmp(val, "test_index") == 0)  return VIZ_SRC_TEST_INDEX;
    return VIZ_SRC_TEST_INDEX; /* default */
}

static VizYSource ParseYSource(const char *val)
{
    if (StrCaseCmp(val, "mb_per_sec") == 0)    return VIZ_SRC_MB_PER_SEC;
    if (StrCaseCmp(val, "iops") == 0)          return VIZ_SRC_IOPS;
    if (StrCaseCmp(val, "min_mbps") == 0)      return VIZ_SRC_MIN_MBPS;
    if (StrCaseCmp(val, "max_mbps") == 0)      return VIZ_SRC_MAX_MBPS;
    if (StrCaseCmp(val, "duration_secs") == 0) return VIZ_SRC_DURATION_SECS;
    if (StrCaseCmp(val, "total_bytes") == 0)   return VIZ_SRC_TOTAL_BYTES;
    return VIZ_SRC_MB_PER_SEC; /* default */
}

static VizGroupBy ParseGroupBy(const char *val)
{
    if (StrCaseCmp(val, "drive") == 0)            return VIZ_GROUP_DRIVE;
    if (StrCaseCmp(val, "test_type") == 0)        return VIZ_GROUP_TEST_TYPE;
    if (StrCaseCmp(val, "block_size") == 0)       return VIZ_GROUP_BLOCK_SIZE;
    if (StrCaseCmp(val, "filesystem") == 0)       return VIZ_GROUP_FILESYSTEM;
    if (StrCaseCmp(val, "hardware") == 0)         return VIZ_GROUP_HARDWARE;
    if (StrCaseCmp(val, "vendor") == 0)           return VIZ_GROUP_VENDOR;
    if (StrCaseCmp(val, "app_version") == 0)      return VIZ_GROUP_APP_VERSION;
    if (StrCaseCmp(val, "averaging_method") == 0) return VIZ_GROUP_AVERAGING;
    return VIZ_GROUP_DRIVE; /* default */
}

static VizCollapseMethod ParseCollapse(const char *val)
{
    if (StrCaseCmp(val, "mean") == 0)   return VIZ_COLLAPSE_MEAN;
    if (StrCaseCmp(val, "median") == 0) return VIZ_COLLAPSE_MEDIAN;
    if (StrCaseCmp(val, "min") == 0)    return VIZ_COLLAPSE_MIN;
    if (StrCaseCmp(val, "max") == 0)    return VIZ_COLLAPSE_MAX;
    return VIZ_COLLAPSE_NONE;
}

static VizDateRange ParseDateRange(const char *val)
{
    if (StrCaseCmp(val, "today") == 0) return VIZ_DATE_TODAY;
    if (StrCaseCmp(val, "week") == 0)  return VIZ_DATE_WEEK;
    if (StrCaseCmp(val, "month") == 0) return VIZ_DATE_MONTH;
    if (StrCaseCmp(val, "year") == 0)  return VIZ_DATE_YEAR;
    return VIZ_DATE_ALL;
}

static VizTrendStyle ParseTrendStyle(const char *val)
{
    if (StrCaseCmp(val, "linear") == 0)         return VIZ_TREND_LINEAR;
    if (StrCaseCmp(val, "moving_average") == 0) return VIZ_TREND_MOVING_AVERAGE;
    if (StrCaseCmp(val, "polynomial") == 0)     return VIZ_TREND_POLYNOMIAL;
    return VIZ_TREND_NONE;
}

static BOOL ParseBool(const char *val)
{
    if (StrCaseCmp(val, "yes") == 0 || StrCaseCmp(val, "true") == 0 || StrCaseCmp(val, "1") == 0)
        return TRUE;
    return FALSE;
}

/* --- Section identifiers --- */

typedef enum {
    SEC_NONE = 0,
    SEC_PROFILE,
    SEC_XAXIS,
    SEC_YAXIS,
    SEC_SERIES,
    SEC_FILTERS,
    SEC_OVERLAY,
    SEC_ANNOTATIONS,
    SEC_COLORS,
    SEC_TRENDLINE
} VizSection;

static VizSection ParseSectionHeader(const char *line)
{
    if (StrCaseCmp(line, "[profile]") == 0)     return SEC_PROFILE;
    if (StrCaseCmp(line, "[xaxis]") == 0)       return SEC_XAXIS;
    if (StrCaseCmp(line, "[yaxis]") == 0)       return SEC_YAXIS;
    if (StrCaseCmp(line, "[series]") == 0)      return SEC_SERIES;
    if (StrCaseCmp(line, "[filters]") == 0)     return SEC_FILTERS;
    if (StrCaseCmp(line, "[overlay]") == 0)     return SEC_OVERLAY;
    if (StrCaseCmp(line, "[annotations]") == 0) return SEC_ANNOTATIONS;
    if (StrCaseCmp(line, "[colors]") == 0)      return SEC_COLORS;
    if (StrCaseCmp(line, "[trendline]") == 0)   return SEC_TRENDLINE;
    return SEC_NONE;
}

/* --- Filter list helper --- */

static void AddFilterValue(VizFilterList *fl, VizFilterMode mode, const char *val)
{
    if (fl->count < VIZ_FILTER_LIST_MAX)
    {
        fl->mode = mode;
        strncpy(fl->values[fl->count], val, VIZ_FILTER_STR_LEN - 1);
        fl->values[fl->count][VIZ_FILTER_STR_LEN - 1] = '\0';
        fl->count++;
    }
}

/* --- Parse a single .viz file into a VizProfile --- */

static BOOL ParseVizFile(const char *path, VizProfile *profile)
{
    FILE *fp;
    char line[512];
    VizSection section = SEC_NONE;

    /* Set defaults */
    memset(profile, 0, sizeof(VizProfile));
    profile->y_autoscale = TRUE;
    profile->sort_x_by_value = TRUE; /* will be adjusted per x_source */
    profile->default_date_range = VIZ_DATE_ALL;
    profile->trend_window = 3;
    profile->trend_degree = 2;
    strncpy(profile->x_label, "X", sizeof(profile->x_label) - 1);
    strncpy(profile->y_label, "MB/s", sizeof(profile->y_label) - 1);
    strncpy(profile->y2_label, "IOPS", sizeof(profile->y2_label) - 1);
    snprintf(profile->filepath, VIZ_PROFILE_PATH_LEN, "%s", path);

    fp = fopen(path, "r");
    if (!fp) return FALSE;

    while (fgets(line, sizeof(line), fp))
    {
        char *key, *val, *eq;
        TrimInPlace(line);

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* Section header */
        if (line[0] == '[')
        {
            section = ParseSectionHeader(line);
            continue;
        }

        /* Key = value pair */
        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        key = line;
        val = eq + 1;
        TrimInPlace(key);
        TrimInPlace(val);
        StripQuotes(val);

        switch (section)
        {
        case SEC_PROFILE:
            if (StrCaseCmp(key, "Name") == 0)
                strncpy(profile->name, val, VIZ_PROFILE_NAME_LEN - 1);
            else if (StrCaseCmp(key, "Description") == 0)
                strncpy(profile->description, val, VIZ_PROFILE_DESC_LEN - 1);
            else if (StrCaseCmp(key, "ChartType") == 0)
            {
                profile->chart_type = ParseChartType(val);
                if (profile->chart_type == VIZ_CHART_HYBRID)
                    profile->enable_secondary_axis = TRUE;
            }
            break;

        case SEC_XAXIS:
            if (StrCaseCmp(key, "Source") == 0)
            {
                profile->x_source = ParseXSource(val);
                /* Adjust sort default based on source */
                if (profile->x_source != VIZ_SRC_BLOCK_SIZE)
                    profile->sort_x_by_value = FALSE;
            }
            else if (StrCaseCmp(key, "Label") == 0)
                strncpy(profile->x_label, val, sizeof(profile->x_label) - 1);
            break;

        case SEC_YAXIS:
            if (StrCaseCmp(key, "Source") == 0)
                profile->y_source = ParseYSource(val);
            else if (StrCaseCmp(key, "Label") == 0)
                strncpy(profile->y_label, val, sizeof(profile->y_label) - 1);
            else if (StrCaseCmp(key, "AutoScale") == 0)
                profile->y_autoscale = ParseBool(val);
            else if (StrCaseCmp(key, "Min") == 0)
                profile->y_fixed_min = (float)strtod(val, NULL);
            else if (StrCaseCmp(key, "Max") == 0)
                profile->y_fixed_max = (float)strtod(val, NULL);
            break;

        case SEC_SERIES:
            if (StrCaseCmp(key, "GroupBy") == 0)
                profile->group_by = ParseGroupBy(val);
            else if (StrCaseCmp(key, "SortX") == 0)
                profile->sort_x_by_value = ParseBool(val);
            else if (StrCaseCmp(key, "MaxSeries") == 0)
                profile->max_series = (uint32)strtoul(val, NULL, 10);
            else if (StrCaseCmp(key, "Collapse") == 0)
                profile->collapse_method = ParseCollapse(val);
            break;

        case SEC_FILTERS:
            if (StrCaseCmp(key, "DefaultDateRange") == 0)
                profile->default_date_range = ParseDateRange(val);
            else if (StrCaseCmp(key, "ExcludeTest") == 0)
                AddFilterValue(&profile->filter_test, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeTest") == 0)
                AddFilterValue(&profile->filter_test, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeBlockSize") == 0)
                AddFilterValue(&profile->filter_block_size, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeBlockSize") == 0)
                AddFilterValue(&profile->filter_block_size, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeVolume") == 0)
                AddFilterValue(&profile->filter_volume, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeVolume") == 0)
                AddFilterValue(&profile->filter_volume, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeFilesystem") == 0)
                AddFilterValue(&profile->filter_filesystem, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeFilesystem") == 0)
                AddFilterValue(&profile->filter_filesystem, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeHardware") == 0)
                AddFilterValue(&profile->filter_hardware, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeHardware") == 0)
                AddFilterValue(&profile->filter_hardware, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeVendor") == 0)
                AddFilterValue(&profile->filter_vendor, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeVendor") == 0)
                AddFilterValue(&profile->filter_vendor, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeProduct") == 0)
                AddFilterValue(&profile->filter_product, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeProduct") == 0)
                AddFilterValue(&profile->filter_product, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeAveraging") == 0)
                AddFilterValue(&profile->filter_averaging, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "IncludeAveraging") == 0)
                AddFilterValue(&profile->filter_averaging, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "ExcludeVersion") == 0)
                AddFilterValue(&profile->filter_version, VIZ_FILTER_EXCLUDE, val);
            else if (StrCaseCmp(key, "MinVersion") == 0)
                AddFilterValue(&profile->filter_version, VIZ_FILTER_INCLUDE, val);
            else if (StrCaseCmp(key, "MinPasses") == 0)
                profile->min_passes = (uint32)strtoul(val, NULL, 10);
            else if (StrCaseCmp(key, "MinMBs") == 0)
                profile->min_mbs = (float)strtod(val, NULL);
            else if (StrCaseCmp(key, "MaxMBs") == 0)
                profile->max_mbs = (float)strtod(val, NULL);
            else if (StrCaseCmp(key, "MinDurationSecs") == 0)
                profile->min_duration_secs = (float)strtod(val, NULL);
            else if (StrCaseCmp(key, "MaxDurationSecs") == 0)
                profile->max_duration_secs = (float)strtod(val, NULL);
            break;

        case SEC_OVERLAY:
            if (StrCaseCmp(key, "SecondarySource") == 0)
            {
                /* Currently only used by hybrid for IOPS overlay */
                profile->enable_secondary_axis = TRUE;
            }
            else if (StrCaseCmp(key, "SecondaryLabel") == 0)
                strncpy(profile->y2_label, val, sizeof(profile->y2_label) - 1);
            break;

        case SEC_ANNOTATIONS:
            if (StrCaseCmp(key, "ReferenceLine") == 0 && profile->ref_line_count < 8)
            {
                /* Format: value, "Label" */
                char *comma = strchr(val, ',');
                if (comma)
                {
                    *comma = '\0';
                    char *label = comma + 1;
                    TrimInPlace(val);
                    TrimInPlace(label);
                    StripQuotes(label);
                    profile->ref_line_values[profile->ref_line_count] = (float)strtod(val, NULL);
                    strncpy(profile->ref_line_labels[profile->ref_line_count], label, 63);
                    profile->ref_line_labels[profile->ref_line_count][63] = '\0';
                    profile->ref_line_count++;
                }
            }
            break;

        case SEC_COLORS:
            if (StrCaseCmp(key, "Color") == 0 && profile->color_count < 16)
            {
                profile->colors[profile->color_count] = (uint32)strtoul(val, NULL, 16);
                profile->color_count++;
            }
            break;

        case SEC_TRENDLINE:
            if (StrCaseCmp(key, "Style") == 0)
                profile->trend_style = ParseTrendStyle(val);
            else if (StrCaseCmp(key, "Window") == 0)
                profile->trend_window = (uint32)strtoul(val, NULL, 10);
            else if (StrCaseCmp(key, "Degree") == 0)
            {
                profile->trend_degree = (uint32)strtoul(val, NULL, 10);
                if (profile->trend_degree > 3) profile->trend_degree = 3;
                if (profile->trend_degree < 2) profile->trend_degree = 2;
            }
            else if (StrCaseCmp(key, "PerSeries") == 0)
                profile->trend_per_series = ParseBool(val);
            break;

        default:
            break;
        }
    }

    fclose(fp);

    /* Profile must have a name to be valid */
    return (profile->name[0] != '\0');
}

/* --- qsort comparator for profiles by name --- */

static int CompareProfiles(const void *a, const void *b)
{
    const VizProfile *pa = (const VizProfile *)a;
    const VizProfile *pb = (const VizProfile *)b;
    return StrCaseCmp(pa->name, pb->name);
}

/* --- Public API --- */

BOOL LoadVizProfiles(void)
{
    APTR context;
    struct ExamineData *data;
    BPTR lock;
    char fullpath[VIZ_PROFILE_PATH_LEN];

    g_viz_profile_count = 0;

    lock = IDOS->Lock("PROGDIR:Visualizations", ACCESS_READ);
    if (!lock)
    {
        LOG_DEBUG("LoadVizProfiles: Visualizations folder not found\n");
        return FALSE;
    }

    context = IDOS->ObtainDirContextTags(EX_LockInput, lock,
                                          EX_DataFields, EXF_NAME | EXF_TYPE,
                                          TAG_DONE);
    if (context)
    {
        while ((data = IDOS->ExamineDir(context)) != NULL)
        {
            if (g_viz_profile_count >= MAX_VIZ_PROFILES) break;

            if (!EXD_IS_FILE(data)) continue;
            if (!StrEndsWith(data->Name, ".viz")) continue;
            if (strlen(data->Name) >= sizeof(fullpath) - 24) continue; /* name too long */

            snprintf(fullpath, sizeof(fullpath), "PROGDIR:Visualizations/%.230s", data->Name);

            if (ParseVizFile(fullpath, &g_viz_profiles[g_viz_profile_count]))
            {
                LOG_DEBUG("Loaded profile: %s (%s)\n",
                        g_viz_profiles[g_viz_profile_count].name, data->Name);
                g_viz_profile_count++;
            }
            else
            {
                LOG_DEBUG("Skipped invalid profile: %s\n", data->Name);
            }
        }
        IDOS->ReleaseDirContext(context);
    }

    IDOS->UnLock(lock);

    if (g_viz_profile_count == 0)
    {
        LOG_DEBUG("LoadVizProfiles: No valid .viz files found\n");
        return FALSE;
    }

    /* Sort alphabetically by name */
    qsort(g_viz_profiles, g_viz_profile_count, sizeof(VizProfile), CompareProfiles);

    LOG_DEBUG("LoadVizProfiles: %lu profiles loaded\n", (unsigned long)g_viz_profile_count);
    return TRUE;
}

void FreeVizProfiles(void)
{
    g_viz_profile_count = 0;
}

/* --- Collapse --- */

static int FloatCompare(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

void CollapseSeriesPoints(float *x_vals, float *y_vals, uint32 *count, VizCollapseMethod method)
{
    uint32 n = *count;
    uint32 out = 0;
    uint32 i, j;
    float bucket_y[200];

    if (n == 0 || method == VIZ_COLLAPSE_NONE) return;

    i = 0;
    while (i < n)
    {
        float x_cur = x_vals[i];
        uint32 bucket_count = 0;

        /* Gather all points with same X value */
        for (j = i; j < n && x_vals[j] == x_cur; j++)
        {
            if (bucket_count < 200)
                bucket_y[bucket_count++] = y_vals[j];
        }

        /* Compute collapsed Y */
        float result_y = 0.0f;
        switch (method)
        {
        case VIZ_COLLAPSE_MEAN:
        {
            float sum = 0.0f;
            uint32 k;
            for (k = 0; k < bucket_count; k++) sum += bucket_y[k];
            result_y = sum / (float)bucket_count;
            break;
        }
        case VIZ_COLLAPSE_MEDIAN:
        {
            qsort(bucket_y, bucket_count, sizeof(float), FloatCompare);
            if (bucket_count % 2 == 1)
                result_y = bucket_y[bucket_count / 2];
            else
                result_y = (bucket_y[bucket_count / 2 - 1] + bucket_y[bucket_count / 2]) / 2.0f;
            break;
        }
        case VIZ_COLLAPSE_MIN:
        {
            uint32 k;
            result_y = bucket_y[0];
            for (k = 1; k < bucket_count; k++)
                if (bucket_y[k] < result_y) result_y = bucket_y[k];
            break;
        }
        case VIZ_COLLAPSE_MAX:
        {
            uint32 k;
            result_y = bucket_y[0];
            for (k = 1; k < bucket_count; k++)
                if (bucket_y[k] > result_y) result_y = bucket_y[k];
            break;
        }
        default:
            result_y = bucket_y[0];
            break;
        }

        x_vals[out] = x_cur;
        y_vals[out] = result_y;
        out++;

        i = j;
    }

    *count = out;
}

/* --- Trend line computation --- */

void ComputeLinearFit(float *x, float *y, uint32 count, float *y_fit)
{
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    double n = (double)count;
    double slope, intercept, denom;
    uint32 i;

    if (count < 2)
    {
        for (i = 0; i < count; i++) y_fit[i] = y[i];
        return;
    }

    for (i = 0; i < count; i++)
    {
        sum_x  += (double)x[i];
        sum_y  += (double)y[i];
        sum_xy += (double)x[i] * (double)y[i];
        sum_x2 += (double)x[i] * (double)x[i];
    }

    denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-12)
    {
        /* Degenerate — all same X */
        double avg = sum_y / n;
        for (i = 0; i < count; i++) y_fit[i] = (float)avg;
        return;
    }

    slope = (n * sum_xy - sum_x * sum_y) / denom;
    intercept = (sum_y - slope * sum_x) / n;

    for (i = 0; i < count; i++)
        y_fit[i] = (float)(slope * (double)x[i] + intercept);
}

void ComputeMovingAverage(float *y, uint32 count, uint32 window, float *y_fit)
{
    uint32 i;
    int32 half = (int32)(window / 2);

    if (count == 0) return;
    if (window < 2) window = 2;

    for (i = 0; i < count; i++)
    {
        int32 lo = (int32)i - half;
        int32 hi = (int32)i + half;
        int32 j;
        float sum = 0.0f;
        uint32 n = 0;

        if (lo < 0) lo = 0;
        if (hi >= (int32)count) hi = (int32)count - 1;

        for (j = lo; j <= hi; j++)
        {
            sum += y[j];
            n++;
        }
        y_fit[i] = sum / (float)n;
    }
}

void ComputePolynomialFit(float *x, float *y, uint32 count, uint32 degree, float *y_fit)
{
    uint32 i, j, k;
    uint32 n = degree + 1;
    double x_min, x_max, x_range;
    double matrix[5][6]; /* max degree 4 → 5x6 augmented matrix */
    double x_norm[200];
    double coeffs[5];

    /* Guard: insufficient points → fall back to linear */
    if (count < n)
    {
        ComputeLinearFit(x, y, count, y_fit);
        return;
    }

    if (degree > 3) degree = 3;
    n = degree + 1;

    /* Normalize X to [0,1] */
    x_min = (double)x[0];
    x_max = (double)x[0];
    for (i = 1; i < count; i++)
    {
        if ((double)x[i] < x_min) x_min = (double)x[i];
        if ((double)x[i] > x_max) x_max = (double)x[i];
    }
    x_range = x_max - x_min;
    if (x_range < 1e-12) x_range = 1.0;

    for (i = 0; i < count && i < 200; i++)
        x_norm[i] = ((double)x[i] - x_min) / x_range;

    /* Build normal equations: M * coeffs = rhs */
    memset(matrix, 0, sizeof(matrix));

    for (i = 0; i < count && i < 200; i++)
    {
        double xi_pow[8]; /* powers of x_norm[i] up to 2*degree */
        xi_pow[0] = 1.0;
        for (j = 1; j <= 2 * degree; j++)
            xi_pow[j] = xi_pow[j - 1] * x_norm[i];

        for (j = 0; j < n; j++)
        {
            for (k = 0; k < n; k++)
                matrix[j][k] += xi_pow[j + k];
            matrix[j][n] += xi_pow[j] * (double)y[i];
        }
    }

    /* Gaussian elimination with partial pivoting */
    for (i = 0; i < n; i++)
    {
        /* Find pivot */
        uint32 max_row = i;
        double max_val = fabs(matrix[i][i]);
        for (j = i + 1; j < n; j++)
        {
            if (fabs(matrix[j][i]) > max_val)
            {
                max_val = fabs(matrix[j][i]);
                max_row = j;
            }
        }

        /* Singular matrix guard */
        if (max_val < 1e-12)
        {
            ComputeLinearFit(x, y, count, y_fit);
            return;
        }

        /* Swap rows */
        if (max_row != i)
        {
            for (k = 0; k <= n; k++)
            {
                double tmp = matrix[i][k];
                matrix[i][k] = matrix[max_row][k];
                matrix[max_row][k] = tmp;
            }
        }

        /* Eliminate */
        for (j = i + 1; j < n; j++)
        {
            double factor = matrix[j][i] / matrix[i][i];
            for (k = i; k <= n; k++)
                matrix[j][k] -= factor * matrix[i][k];
        }
    }

    /* Back substitution */
    for (i = n; i > 0; i--)
    {
        uint32 row = i - 1;
        coeffs[row] = matrix[row][n];
        for (j = row + 1; j < n; j++)
            coeffs[row] -= matrix[row][j] * coeffs[j];
        coeffs[row] /= matrix[row][row];
    }

    /* Evaluate polynomial at each x_norm */
    for (i = 0; i < count && i < 200; i++)
    {
        double val = 0.0;
        double xp = 1.0;
        for (j = 0; j < n; j++)
        {
            val += coeffs[j] * xp;
            xp *= x_norm[i];
        }
        y_fit[i] = (float)val;
    }
}
