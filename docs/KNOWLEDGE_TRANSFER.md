# Knowledge Transfer: AmigaDiskBench v2.5.2

This document captures detailed technical learnings and patterns discovered during the implementation of AmigaDiskBench through v2.5.2. It supplements the [AGENT_HANDOVER.md](AGENT_HANDOVER.md) with deep implementation details.

## 1. ReAction UI & Gadget State
- **Initial Selection**: When a ReAction `ChooserObject` (or similar) depends on a global state variable (like `ui.viz_chart_type_idx`), always include `CHOOSER_Selected, ui.variable` in the gadget definition within `CreateMainLayout`. Failing to do so can cause the UI to default to index 0 regardless of the variable's initial value.
- **Dynamic Label Refreshing**: Reattaching labels to a `ListBrowser` or `Chooser` requires detaching them first (`LISTBROWSER_Labels, (ULONG)-1`) to avoid internal OS corruption or missed updates.
- **Read-only display gadget pattern**: To surface a value in the main window without a live interactive gadget, use a `GA_ReadOnly, TRUE` `ButtonObject`. Store it in `GUIState`. Update via `IIntuition->SetGadgetAttrs()` when the underlying value changes (e.g., prefs save, profile switch).

## 2. CSV History Management & Sanitization
- **Sanitization Strategy**: Integrating sanitization directly into the `RefreshHistory` loading loop is efficient.
    - **Detection**: Check each record for validity (e.g., block size 0 for fixed-behavior tests).
    - **Persistence**: If any record is modified during loading, set a `needs_sanitization` flag and call `SaveHistoryToCSV` *after* the loop closes the file handle.
- **Timestamped Filenames**: Using `time.h` and `strftime` is fully supported. Format `bench_history_%Y-%m-%d-%H-%M-%S.csv` preserves sortability.

## 3. Workload Standardization
- **Block Size Sentinels**: For tests that do not utilize a configurable block size (e.g., `TEST_PROFILER`, `TEST_DAILY_GRIND`), the engine and GUI worker should strictly enforce a value of `0` (Mixed).
- **Centralized Enforcement**: Force these values in both `LaunchBenchmarkJob` (GUI side) and `RunBenchmark` (Engine side).
- **op_count must reflect actual I/O operations**: Sequential I/O workloads must return `bytes_processed / block_size` as `op_count`, not a hardcoded value. The engine computes IOPS as `total_ops / total_elapsed_seconds`. Hardcoding `op_count = 1` produces meaningless IOPS values.

## 4. Build Environment (WSL2 + Docker)
- **Volume Mapping**: Always use the `/mnt/[drive]/...` syntax for volume mounts in Docker when running under WSL2.
- **Distribution Consistency**: Ensure the WSL distribution matches path expectations.
- **Always `make clean` before `make all`**: The Makefile only recompiles changed `.c` files. Stale `.o` files can cause the binary to link against old object code, show an outdated version string, or exhibit phantom bugs.
- **Separate Docker invocations per target**: Run `make clean` and `make all` as separate `docker run` commands.

## 5. Versioning
- **Header Files**: `include/version.h` is the single source of truth. Updating `VERSION`, `REVISION`, `MINOR`, and `BUILD` here automatically propagates to the AppTitle, Version String, and internal metadata.
- **Rule**: Bug fixes increment `BUILD` by 1 only. New features or UI changes increment `MINOR` by 1. Major releases increment `REVISION`.

## 6. Preferences Window — ReAction Chooser Pattern (v2.3.1 / v2.3.2)
- **CHOOSER_Labels requires a struct List \***: The `CHOOSER_Labels` tag expects a pointer to an AmigaOS Exec `struct List` populated with chooser nodes, **not** a plain C string array. Passing a `const char *[]` compiles silently but crashes at runtime.
- **chooser.gadget walks the list live** — it does **not** deep-copy the list at `NewObject` time. The list must remain allocated for the entire lifetime of the window.
- **Correct pattern** — store list in `GUIState`, free on all close paths:
    ```c
    // At window open time:
    IExec->NewList(&ui.prefs_avg_list);
    for (uint32 i = 0; i < NUM_LABELS; i++) {
        struct Node *n = IChooser->AllocChooserNode(CNA_Text, labels[i], CNA_CopyText, TRUE, TAG_DONE);
        if (n) IExec->AddTail(&ui.prefs_avg_list, n);
    }
    // ... WindowObject ... CHOOSER_Labels, (uint32)&ui.prefs_avg_list ...

    // On every close path (Save, Cancel, WM_CLOSEWINDOW):
    struct Node *n, *nx;
    for (n = IExec->GetHead(&ui.prefs_avg_list); n; n = nx) {
        nx = IExec->GetSucc(n);
        IChooser->FreeChooserNode(n);
    }
    ```
- **CHOOSER_Labels ownership**: Autodoc says the list is "freed automatically when the object is disposed." Do NOT free manually if the chooser owns it — double-free crash inside `bevel.image/chooser.gadget`.
- **INTEGER_MaxChars must come first**: Place `INTEGER_MaxChars` before `INTEGER_Minimum`, `INTEGER_Maximum`, and `INTEGER_Number`. Tags are processed sequentially and `MaxChars` affects value clamping.

## 7. Averaging Methods & IOPS (v2.3.1 / v2.5.2)
- **Three-option AveragingMethod enum** in `engine.h`:
    - `AVERAGE_ALL_PASSES` (0): Simple arithmetic mean of all pass results.
    - `AVERAGE_TRIMMED_MEAN` (1): Finds the single min and max, excludes them, averages the rest. Falls back to All Passes if fewer than 3 passes.
    - `AVERAGE_MEDIAN` (2): `qsort()` all results; index = `round(passes / 2.0) - 1`. `effective_passes = 1`.
- **Data flow**: `ui.averaging_method` (GUIState) → `BenchJob.averaging_method` → `RunBenchmark()` → `engine_tests.c` switch → `BenchResult.averaging_method` (persisted to CSV).
- **IOPS data flow**: Each workload's `Run()` returns `op_count`. Engine sums `sum_iops += op_count` across passes. Final `out_result->iops = (uint32)((float)sum_iops / total_duration)` where `total_duration` is the sum of all pass durations in seconds.
- **Historical bug**: Before v2.5.2, four workloads (Sequential Write/Read, Heavy Lifter, Legacy) hardcoded `*op_count = 1`, and the engine computed `sum_iops / valid_passes` (average ops per pass, not per second). Both bugs compounded to produce meaningless IOPS.

## 8. Chooser Label Text & Underscore Escaping (v2.3.2 / v2.3.3)
- **`_X` in chooser text underlines `X` as a keyboard shortcut**. Any `_` in a volume name (e.g., `SFS_DH7_2`) will cause the following character to be underlined.
- **Fix**: Replace `_` with ` ` (space) when building chooser node text. Do **not** use `__` (double underscore).
- **Apply consistently** to every site that builds chooser nodes from user-data strings.
- **Filter comparisons**: If the chooser text is sanitized and filter comparison reads it back via `CNA_Text` to compare against a raw string, the strings won't match. Sanitize both sides.

## 9. Pluggable Visualization Profiles (v2.5.2)

### Profile Loading
- **Startup**: `LoadVizProfiles()` is called in `gui.c` after `InitVizFilterLabels()`. It locks `PROGDIR:Visualizations/` via `IDOS->Lock()`, iterates with `IDOS->ExamineDir()` filtering for `*.viz`, calls `ParseVizFile()` for each, sorts by name, stores in `g_viz_profiles[]`.
- **Fatal on failure**: If `LoadVizProfiles()` returns FALSE (no folder or no valid files), an `EasyRequest` shows the error and the application exits. There are no built-in fallback charts.
- **Globals**: `VizProfile g_viz_profiles[MAX_VIZ_PROFILES]` and `uint32 g_viz_profile_count` in `viz_profile.c`.

### .viz File Format
- INI-style: `[Section]` headers, `Key = Value` pairs, `#` comments.
- Repeated keys allowed (e.g., multiple `ExcludeTest` or `Color` entries append to arrays).
- String values may be quoted (`"Label"`) or unquoted. Parser trims whitespace.
- Case-insensitive enum matching via `strcasecmp`.
- Color values: `0xRRGGBB` hex, parsed with `strtoul(value, NULL, 16)`.

### Sections
| Section | Key Fields |
|---------|-----------|
| `[Profile]` | `Name`, `Description`, `ChartType` (line/bar/hybrid) |
| `[XAxis]` | `Source` (block_size/timestamp/test_index), `Label`, `Format` |
| `[YAxis]` | `Source` (mb_per_sec/iops/min_mbps/max_mbps/duration_secs/total_bytes), `Label`, `AutoScale` (yes/no), `Min`, `Max` |
| `[Series]` | `GroupBy` (drive/test_type/block_size/filesystem/hardware/vendor/app_version/averaging_method), `MaxSeries`, `SortX` (yes/no), `Collapse` (none/mean/median/min/max) |
| `[Filters]` | `ExcludeTest`/`IncludeTest`, `ExcludeBlockSize`/`IncludeBlockSize`, `ExcludeVolume`/`IncludeVolume`, `ExcludeFilesystem`/`IncludeFilesystem`, `ExcludeHardware`/`IncludeHardware`, `ExcludeVendor`/`IncludeVendor`, `ExcludeProduct`/`IncludeProduct`, `ExcludeAveraging`/`IncludeAveraging`, `ExcludeVersion`/`IncludeVersion`, `MinPasses`, `MinMBs`, `MaxMBs`, `MinDurationSecs`, `MaxDurationSecs` |
| `[TrendLine]` | `Style` (none/linear/moving_avg/polynomial), `Window` (for moving_avg), `Degree` (for polynomial), `PerSeries` (yes/no) |
| `[Annotations]` | `ReferenceLine = value, "Label"` (up to 8) |
| `[Colors]` | `Color = 0xRRGGBB` (up to 16) |

### Data Collection (gui_viz.c)
- `CollectVizData()` iterates loaded CSV results, applying:
  1. On-screen UI filters (volume, test type, date range, app version) — existing logic.
  2. Per-profile `VizFilterList` filters via `ApplyFilterList()` — exclude/include matching.
  3. Numeric range filters (min_passes, min/max MB/s, min/max duration).
- Y value selected by `profile->y_source`. X value by `profile->x_source`.
- Series grouping by `profile->group_by` (maps to result fields: volume_name, test type display name, block size label, fs_type, device_name, vendor, app_version, averaging method string).
- After sorting, if `profile->collapse_method != VIZ_COLLAPSE_NONE`, `CollapseSeriesPoints()` deduplicates.

### ApplyFilterList Semantics
```
Exclude mode: return FALSE if value matches any entry (skip this result)
Include mode: return FALSE if value matches NONE of the entries (skip this result)
Empty list: return TRUE (pass all)
Hardware/vendor/product: substring match (strstr)
All others: exact case-insensitive match (Stricmp)
```

### Collapse Aggregation
- **block_size X**: Groups results with the same block size within each series, applies mean/median/min/max to Y values, keeps one representative result per group.
- **Non-block_size X** (test_index, timestamp): Collapses the entire series to a single point (all Y values aggregated to one).
- **Median**: Copies Y values to temp array, `qsort`, picks middle value.

### Trend Line Mathematics
- **Linear (OLS)**: 5 accumulators (n, Σx, Σy, Σxy, Σx²). `slope = (nΣxy - ΣxΣy) / (nΣx² - (Σx)²)`.
- **Moving Average**: Centred window, clamped at edges. `y_fit[i] = mean(y[max(0,i-w/2) .. min(count-1,i+w/2)])`.
- **Polynomial**: Normalizes X to [0,1], builds (degree+1)×(degree+2) augmented matrix of normal equations, Gaussian elimination with partial pivoting using `fabs()`. Guard: if `count < degree + 1`, falls back to linear.
- **Rendering**: Polyline with lighter shade of series color (`(comp + 255) / 2` per RGB channel). All coordinates clamped to chart bounds.

### Chooser Integration
- Profile names populate the chart type chooser (replacing hardcoded array).
- On profile switch (`GID_VIZ_CHART_TYPE`): update Color By display text from `group_names[profile->group_by]`, update date range to `profile->default_date_range`, call `UpdateVisualization()`.
- `ReloadVizProfiles()`: detach chooser labels → free old nodes → `FreeVizProfiles()` + `LoadVizProfiles()` → rebuild nodes → reattach → reset index 0 → `UpdateVisualization()`.

### Rendering Dispatch
```c
switch (profile->chart_type) {
    case VIZ_CHART_BAR:    RenderBarChart(rp, box, vd, profile);    break;
    case VIZ_CHART_HYBRID: RenderHybridChart(rp, box, vd, profile); break;
    default:               RenderLineChart(rp, box, vd, profile);   break;
}
```
- All render functions receive `VizProfile *profile` for axis labels, colors, trend lines, annotations.
- `DrawGridAndAxes()` uses `profile->y_label` and `profile->x_label` instead of hardcoded strings.
- `GetSeriesColor(profile, idx)` returns `profile->colors[idx % color_count]` when custom colors are defined.

### X-Axis Label Deduplication
When all data points share the same X value (e.g., all tests at 1M block size), the label drawing code compares first/middle/last labels as strings and skips duplicates. This prevents "1M 1M 1M" showing on the X-axis.

## 10. VALIDATE Mode (v2.5.2)
- **Detection**: Shell = `ReadArgs("VALIDATE/S", ...)` when `IDOS->Output() != NULL`. Workbench = `IIcon->FindToolType(diskobj->do_ToolTypes, "VALIDATE")`.
- **Shell output**: Formatted text report to stdout with file name, line number, severity (ERROR/WARNING), and message. Exit with `RETURN_OK` (clean), `RETURN_WARN`, or `RETURN_ERROR`.
- **Workbench output**: Standalone ReAction window (`WindowObject`) with `WINDOW_CloseGadget/SizeGadget/DragBar/DepthGadget TRUE`. Single scrollable `ListBrowserObject` with monospace font. `WMHI_CLOSEWINDOW` is the only event. Mini event loop.
- **Validation checks**: Required keys present, unknown section/key detection, enum value validation, type/range checks, hex color format, duplicate key warnings, mutual exclusion (Include + Exclude for same filter type).
- **VizFinding struct**: `{ struct Node node; uint32 line_number; char severity; char message[256]; }` — linked list for findings per file.

## 11. Worker Process Communication (v2.4.1 / v2.5.2)
- **Message types**: `BenchJob` (GUI → worker, `ReplyMsg` on completion), `BenchStatus` (worker → GUI, `PutMsg` with progress), `BenchLogMsg` (worker → GUI, `PutMsg` with log text).
- **Two messages per job**: A submitted `BenchJob` produces a `BenchStatus` `PutMsg` AND the original job `ReplyMsg` (`mn_Node.ln_Type == NT_REPLYMSG`).
- **Shutdown protocol**: (1) `CleanupBenchmarkQueue()` frees all pending jobs. (2) Loop `WaitPort`/`GetMsg` until `NT_REPLYMSG` arrives. Free status messages along the way. (3) Set `worker_busy = FALSE`. (4) Then send quit message and free reply port.
- **ReadArgs guard**: `ReadArgs("VALIDATE/S", ...)` must only be called when `IDOS->Output() != NULL`. On Workbench launch, `Output()` returns NULL, causing `ReadArgs` to fail and potentially leave stale state.

## 12. Quit Confirmation Dialog (v2.5.2)
- Three quit paths: `WMHI_CLOSEWINDOW`, `APPLIBMT_Quit` (application.library quit message), `SIGBREAKF_CTRL_C`.
- Each path checks `ui.worker_busy`. If TRUE, shows `EasyRequest`:
    ```c
    struct EasyStruct easy = { sizeof(struct EasyStruct), 0, "AmigaDiskBench",
        "A benchmark is still running.\nAre you sure you want to quit?", "Yes|No" };
    if (IIntuition->EasyRequest(ui.window, &easy, NULL, NULL) == 1)
        running = FALSE;
    ```
- Return value 1 = "Yes" (first button), 0 = "No" (second button).

## 13. IFF Clipboard Format (v2.4.1)
- A bare `CHRS` chunk without a `FORM FTXT` container is not valid IFF and is rejected by applications.
- Correct pattern:
    ```
    PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)
    PushChunk(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN)
    WriteChunkBytes(iff, text, len)
    PopChunk(iff)  // CHRS
    PopChunk(iff)  // FORM
    ```
- `ID_FTXT`/`ID_CHRS` defined in `<datatypes/textclass.h>`. `PRIMARY_CLIP` defined in `<devices/clipboard.h>`.

## 14. Low-Level Disk Access & Quirks (v2.2.16)
- **SCSI Inquiry Freezes**: Many older ATAPI CD-ROM drives hang if queried for VPD Page 0x80. Check Peripheral Device Type first; skip VPD for type 0x05.
- **System Requesters on Empty Drives**: Wrap `IDOS->Lock` with `SetProcWindow((APTR)-1)` to suppress "No Disk" popups.
- **Device Locking**: Append colon to device name (`DH0:` not `DH0`).
- **IOExtTD allocation**: Use `sizeof(struct IOExtTD)` for trackdisk.device, not `sizeof(struct IOStdReq)`.
- **SCSI_INQUIRY & AUTOSENSE**: If `SCSIF_AUTOSENSE` is set, `scsi_SenseData` buffer must be non-NULL. NULL causes kernel DSI.
- **Unmounted Partitions**: May fail `IDOS->Lock`. Gracefully fall back to "Unmounted" display.
- **QEMU & S.M.A.R.T.**: QEMU block device emulation does not support raw ATA commands. S.M.A.R.T. fails gracefully.

## 15. Rendering Patterns (gui_viz_render.c)
- **Custom gpRender hook**: The visualization graph uses a custom Intuition rendering hook. `WA_IDCMP` must include `IDCMP_MOUSEBUTTONS` for hover detection.
- **Color pen management**: `ObtainColorPen()` / `ReleaseColorPen()` for each series color. Allocated at render time, released after drawing.
- **Dashed lines**: `DrawDashedHLine()` draws alternating 4px segments. Reused for grid lines and annotation reference lines.
- **Coordinate clamping**: All line/polyline drawing clamps pixel coordinates to chart bounds (`px`, `py`, `px+pw`, `py+ph`). Without this, trend lines and data lines can draw outside the chart area, overwriting axis labels or other UI elements.
- **Legend**: Drawn below the chart area. Color swatches + series names. Uses `GetSeriesColor()` for profile-aware colors.
- **Hybrid chart**: Bars for MB/s on left Y-axis, line for IOPS on right Y-axis. Both rendered in the same coordinate space with independent Y scaling.

## 16. AmigaOS 4 SDK Patterns
- **`-lauto`**: Auto-opens/closes libraries, interfaces, and class pointers. Use with `<proto/foo.h>`.
- **Do NOT define `__USE_INLINE__`**: Use `IFace->Method()` syntax.
- **`IUtility` limitations**: Does NOT have `Strstr`, `Strchr`, `Strlen`, `AToLong`. Use C stdlib.
- **`IUtility` capabilities**: `Stricmp`, `Strnicmp`, `Strlcpy`, `SNPrintf`, `VSNPrintf`.
- **Proto headers don't always include tag defs**: E.g., `<proto/filler.h>` provides class pointer but NOT `FILLER_BackgroundColor`. Include the specific image/gadget header too.
- **TAG_IGNORE pattern**: `condition ? REAL_TAG : TAG_IGNORE, value` for conditional tags. TAG_IGNORE skips both tag+data.
- **Deprecated functions**: `AllocMem`/`AllocVec` → `AllocVecTags`. See wiki for full list.
