/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

#ifndef VIZ_PROFILE_H
#define VIZ_PROFILE_H

#include <exec/types.h>
#include <dos/dos.h>

/* Forward-declare VizDateRange from gui.h — reuse existing enum */
/* VIZ_DATE_TODAY=0, VIZ_DATE_WEEK, VIZ_DATE_MONTH, VIZ_DATE_YEAR, VIZ_DATE_ALL */
#include "gui.h"

#define VIZ_PROFILE_NAME_LEN   64
#define VIZ_PROFILE_DESC_LEN   128
#define VIZ_PROFILE_PATH_LEN   256
#define MAX_VIZ_PROFILES       32

typedef enum {
    VIZ_CHART_LINE = 0,
    VIZ_CHART_BAR,
    VIZ_CHART_HYBRID,
    VIZ_CHART_COUNT
} VizChartType;

typedef enum {
    VIZ_TREND_NONE = 0,
    VIZ_TREND_LINEAR,
    VIZ_TREND_MOVING_AVERAGE,
    VIZ_TREND_POLYNOMIAL,
} VizTrendStyle;

typedef enum {
    VIZ_SRC_BLOCK_SIZE = 0,
    VIZ_SRC_TIMESTAMP,
    VIZ_SRC_TEST_INDEX,
    VIZ_SRC_COUNT
} VizXSource;

typedef enum {
    VIZ_SRC_MB_PER_SEC = 0,
    VIZ_SRC_IOPS,
    VIZ_SRC_MIN_MBPS,
    VIZ_SRC_MAX_MBPS,
    VIZ_SRC_DURATION_SECS,
    VIZ_SRC_TOTAL_BYTES,
    VIZ_SRC_COUNT_Y
} VizYSource;

typedef enum {
    VIZ_GROUP_DRIVE = 0,
    VIZ_GROUP_TEST_TYPE,
    VIZ_GROUP_BLOCK_SIZE,
    VIZ_GROUP_FILESYSTEM,
    VIZ_GROUP_HARDWARE,
    VIZ_GROUP_VENDOR,
    VIZ_GROUP_APP_VERSION,
    VIZ_GROUP_AVERAGING,
    VIZ_GROUP_COUNT
} VizGroupBy;

typedef enum {
    VIZ_COLLAPSE_NONE = 0,
    VIZ_COLLAPSE_MEAN,
    VIZ_COLLAPSE_MEDIAN,
    VIZ_COLLAPSE_MIN,
    VIZ_COLLAPSE_MAX,
} VizCollapseMethod;

#define VIZ_FILTER_LIST_MAX  16
#define VIZ_FILTER_STR_LEN   64

typedef enum {
    VIZ_FILTER_EXCLUDE = 0,
    VIZ_FILTER_INCLUDE
} VizFilterMode;

typedef struct {
    VizFilterMode mode;
    char          values[VIZ_FILTER_LIST_MAX][VIZ_FILTER_STR_LEN];
    uint32        count;
} VizFilterList;

typedef struct {
    char         name[VIZ_PROFILE_NAME_LEN];
    char         description[VIZ_PROFILE_DESC_LEN];
    char         filepath[VIZ_PROFILE_PATH_LEN];
    VizChartType chart_type;
    VizXSource   x_source;
    char         x_label[64];
    VizYSource   y_source;
    char         y_label[64];
    char         y2_label[64];
    BOOL         y_autoscale;
    float        y_fixed_min;
    float        y_fixed_max;
    VizGroupBy   group_by;
    uint32       max_series;
    VizDateRange default_date_range;
    BOOL         enable_secondary_axis;
    BOOL         sort_x_by_value;
    VizCollapseMethod collapse_method;
    /* Profile-level filters */
    VizFilterList filter_test;
    VizFilterList filter_block_size;
    VizFilterList filter_volume;
    VizFilterList filter_filesystem;
    VizFilterList filter_hardware;
    VizFilterList filter_vendor;
    VizFilterList filter_product;
    VizFilterList filter_averaging;
    VizFilterList filter_version;
    uint32        min_passes;
    float         min_mbs;
    float         max_mbs;
    float         min_duration_secs;
    float         max_duration_secs;
    /* Annotations */
    float         ref_line_values[8];
    char          ref_line_labels[8][64];
    uint32        ref_line_count;
    /* Trend line */
    VizTrendStyle trend_style;
    uint32        trend_window;
    uint32        trend_degree;
    BOOL          trend_per_series;
    /* Color palette */
    uint32        color_count;
    uint32        colors[16];
} VizProfile;

/* Global profile registry */
extern VizProfile g_viz_profiles[MAX_VIZ_PROFILES];
extern uint32     g_viz_profile_count;

/* Load all .viz profiles from PROGDIR:Visualizations/ */
BOOL LoadVizProfiles(void);

/* Free profile data (resets count to 0) */
void FreeVizProfiles(void);

/* Collapse duplicate X points within a series */
void CollapseSeriesPoints(float *x_vals, float *y_vals, uint32 *count, VizCollapseMethod method);

/* Trend line computation */
void ComputeLinearFit(float *x, float *y, uint32 count, float *y_fit);
void ComputeMovingAverage(float *y, uint32 count, uint32 window, float *y_fit);
void ComputePolynomialFit(float *x, float *y, uint32 count, uint32 degree, float *y_fit);

#endif /* VIZ_PROFILE_H */
