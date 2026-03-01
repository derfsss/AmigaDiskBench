# Agent Handover: AmigaDiskBench

This document provides essential context for any AI agent or developer picking up work on the AmigaDiskBench project.

## Project Overview
AmigaDiskBench is a modern, ReAction-based disk benchmarking utility for AmigaOS 4.x. It supports various test types (Sequential, Random, Mixed, Meta-profiling), S.M.A.R.T. health monitoring, hierarchical disk information, pluggable visualization profiles, a real-time session log, and multi-series graphing with trend lines, annotations, and collapse aggregation.

## Build Environment
- **OS**: Windows (host) / WSL2 (build environment).
- **Toolchain**: AmigaOS 4.1 GCC 11 cross-compiler.
- **Docker Image**: `walkero/amigagccondocker:os4-gcc11`.
- **SDK Version**: 54.16.

### Build Commands (WSL2)
```bash
# From WSL shell inside the project directory:
cd /mnt/w/Code/amiga/antigravity/projects/AmigaDiskBench

# Clean
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make clean

# Build
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make all
```

When invoking from Windows (e.g., VS Code terminal via `wsl -e sh -c "..."`), `$(pwd)` expands correctly inside the quoted string. Each `make` target should be run as a separate Docker invocation. **Always run `make clean` before `make all`** — the Makefile only recompiles changed `.c` files, so stale `.o` files can cause the binary to show an old version string or link against outdated object code.

## Current Version
**v2.5.2** (version.h: VERSION=2, REVISION=5, MINOR=2, BUILD=1136, date 01.03.2026)

## Architecture & Key Components

### GUI Layer
- `src/main.c`: Entry point, initializes the GUI.
- `src/gui.c`: Core ReAction GUI logic and event loop. Handles startup (library init, icon loading, ReadArgs for VALIDATE mode, `LoadVizProfiles()`), main event loop (WaitPort, signal handling), and shutdown (worker drain, cleanup).
- `src/gui_layout.c`: GUI layout definition using ReAction objects. Defines all tab pages (Benchmark, Bulk, Visualization, Disk Info, Health, History, Log).
- `src/gui_events.c`: IDCMP / gadget event dispatch. Handles all `GID_*` gadget events, `WMHI_CLOSEWINDOW`, `APPLIBMT_Quit`, and quit confirmation dialog.
- `src/gui_worker.c`: Worker process for benchmark execution; posts progress/completion messages via Exec message passing.
- `src/gui_prefs.c`: Preferences window — open, update, load/save via application.library PrefsObjects.
- `src/gui_bulk.c`: Bulk queue management and sequential job dispatch.
- `src/gui_viz.c`: Visualization tab state — filter lists, data collection, profile-driven series grouping, collapse aggregation, `ApplyFilterList()` for per-profile filtering, `ReloadVizProfiles()`.
- `src/gui_viz_render.c`: Custom Intuition rendering hook for the multi-series graph. Renders line, bar, and hybrid charts. Draws grid/axes, legends, trend lines, annotations, X-axis labels with deduplication.
- `src/gui_info.c`: Disk Information tab UI (Master-Detail tree view).
- `src/gui_health.c`: Drive Health tab (S.M.A.R.T. display).
- `src/gui_history.c`: History tab — CSV load, ListBrowser population, delete/export.
- `src/gui_logging.c`: Session Log tab — timestamped log display, clear/copy buttons, incremental text insertion.
- `src/gui_system.c`: System resource init (drive enumeration, chooser label building).
- `src/gui_utils.c`: Shared GUI utility functions.
- `src/gui_compare_window.c`: Comparison delta report window.
- `src/gui_details_window.c`: Detailed result view window.
- `src/gui_export.c`: CSV export functionality.
- `src/gui_report.c`: Summary report generation.

### Visualization Profile System (v2.5.2)
- `src/viz_profile.c`: `.viz` file parser and profile management. Loads INI-style profiles from `PROGDIR:Visualizations/`, parses sections (`[Profile]`, `[XAxis]`, `[YAxis]`, `[Series]`, `[Filters]`, `[Overlay]`, `[Annotations]`, `[Colors]`, `[TrendLine]`). Implements `CollapseSeriesPoints()`, `ComputeLinearFit()`, `ComputeMovingAverage()`, `ComputePolynomialFit()`.
- `src/gui_validate.c`: VALIDATE mode — validates all `.viz` files and reports errors with line numbers. Shell mode prints to stdout; Workbench mode opens a standalone ReAction window with monospace ListBrowser.
- `include/viz_profile.h`: All enums (`VizChartType`, `VizTrendStyle`, `VizXSource`, `VizYSource`, `VizGroupBy`, `VizCollapseMethod`, `VizFilterMode`), `VizFilterList` struct, `VizProfile` struct, globals (`g_viz_profiles[]`, `g_viz_profile_count`), prototypes.
- `include/gui_validate.h`: `VizFinding` struct, `RunValidation()` prototype.
- `Visualizations/*.viz`: 9 built-in profile files (scaling, trend, battle, workload, hybrid, peak, smoothed, curve, filesystem).

### Engine Layer
- `src/engine.c`: Benchmarking engine core — runs in a separate process. Computes final results including IOPS (total ops / total elapsed time).
- `src/engine_tests.c`: Per-test dispatch, multi-pass loop, averaging calculation.
- `src/engine_persistence.c`: CSV saving and loading for history.
- `src/engine_diskinfo.c`: Disk enumeration and hardware information retrieval.
- `src/engine_smart.c`: S.M.A.R.T. data retrieval via ATA PASS-THROUGH.
- `src/engine_system.c`: System-level engine utilities.
- `src/engine_utils.c`: Shared engine helpers.
- `src/engine_warmup.c`: Pre-benchmark warmup passes.
- `src/engine_workloads.c`: Workload dispatch.
- `src/benchmark_queue.c`: Benchmark job queue management.

### Workloads
- `src/workloads/workload_sequential.c`: Sequential write test.
- `src/workloads/workload_sequential_read.c`: Sequential read test.
- `src/workloads/workload_random_4k.c`: Random 4K write test.
- `src/workloads/workload_random_4k_read.c`: Random 4K read test.
- `src/workloads/workload_mixed_rw.c`: Mixed 70/30 read/write test.
- `src/workloads/workload_profiler.c`: Filesystem metadata profiling.
- `src/workloads/workload_legacy_sprinter.c`: Quick I/O profile.
- `src/workloads/workload_legacy_grind.c`: Daily Grind profile.
- `src/workloads/workload_legacy_heavy.c`: Heavy Lifter profile.
- `src/workloads/workload_legacy_legacy.c`: Legacy 512B-block test.

Each workload implements the `BenchWorkload` interface: `Setup()`, `Run()`, `Cleanup()`, `GetDefaultSettings()`. The `Run()` function returns `bytes_processed` and `op_count` (= bytes / block_size for sequential I/O workloads).

### Headers
- `include/version.h`: Single source of truth for version string.
- `include/gui.h`: GUIState struct, GID_* enum, BenchJob/BenchStatus/BenchLogMsg message types.
- `include/gui_internal.h`: Shared internal header — all ReAction/OS includes, internal prototypes.
- `include/engine.h`: BenchResult, AveragingMethod enum, engine API.
- `include/engine_internal.h`: Engine-internal helpers shared across engine source files.
- `include/engine_diskinfo.h`: Disk enumeration types and API.
- `include/engine_warmup.h`: Warmup API.
- `include/engine_workloads.h`: Workload dispatch API.
- `include/workload_interface.h`: BenchWorkload struct definition.
- `include/benchmark_queue.h`: Queue types and API.
- `include/debug.h`: Debug logging toggle.
- `include/gui_details_window.h`: Details window API.

## Version History

### v2.5.2 (Current — 01.03.2026)
- **Pluggable Visualization Profiles**: Replaced 5 hardcoded chart types with a file-driven `.viz` profile system. Nine INI-style profiles in `Visualizations/` define chart type, axes, grouping, filters, trend lines, annotations, colors, and collapse aggregation.
- **Profile-driven rendering**: `gui_viz.c` reads the active `VizProfile` to determine data collection (X/Y sources, GroupBy, filters, collapse), and `gui_viz_render.c` dispatches to the correct chart renderer based on `profile->chart_type`.
- **Trend lines**: `ComputeLinearFit()` (OLS), `ComputeMovingAverage()` (centred window), `ComputePolynomialFit()` (Gaussian elimination with partial pivoting). Rendered as lighter-shade polylines with coordinate clamping to stay within chart bounds.
- **Collapse aggregation**: `CollapseSeriesPoints()` deduplicates data at the same X value using mean, median, min, or max. For non-block_size X sources, collapses entire series to one point.
- **Reference line annotations**: `DrawAnnotations()` renders horizontal dashed white lines at specified Y values with right-aligned labels.
- **Custom color palettes**: `GetSeriesColor()` returns profile colors when available, falling back to the default palette.
- **Reload Profiles button** (`GID_VIZ_RELOAD`): Detaches chooser labels, frees old profiles, re-scans folder, rebuilds chooser, resets to index 0.
- **VALIDATE mode**: `RunValidation()` detects Shell vs Workbench context. Shell prints to stdout. Workbench opens a standalone ReAction window with a monospace ListBrowser showing findings.
- **Quit confirmation dialog**: `EasyRequest` prompts "A benchmark is still running. Are you sure you want to quit?" at `WMHI_CLOSEWINDOW`, `APPLIBMT_Quit`, and `SIGBREAKF_CTRL_C`.
- **IOPS calculation fix**: Engine now computes `total_ops / total_duration` (true ops/second). Four workloads (Sequential Write/Read, Heavy Lifter, Legacy) fixed to return `bytes / block_size` as `op_count` instead of hardcoded `1`.
- **Exit crash fix**: `CleanupBenchmarkQueue()` drains pending queue, then a loop waits for `NT_REPLYMSG` from the active worker before freeing the reply port.
- **Blank window on Workbench launch fix**: `ReadArgs("VALIDATE/S", ...)` guarded to only run when `IDOS->Output() != NULL` (Shell context).
- **Crash in CollapseSeriesPoints**: NULL pointer guard when series has no data points.
- **Coordinate clamping**: All line/polyline drawing in `gui_viz_render.c` clamps pixel coordinates to chart bounds.
- **Duplicate X-axis labels**: `DrawXAxisLabels()` now compares first/middle/last label text and skips duplicates.
- **Color By removed as interactive control**: Now a read-only `ButtonObject` showing the active profile's GroupBy text.
- **Unused filter dropdowns removed** from visualization tab.

### v2.4.1 (01.03.2026)
- **Session Log tab**: Scrollable, timestamped log panel. Cross-process Exec message passing (`BenchLogMsg` with `MSG_TYPE_LOG`) delivers log entries from worker to GUI.
- **Incremental log display**: `RefreshLogDisplay()` inserts only new text via `GM_TEXTEDITOR_InsertText` at `GV_TEXTEDITOR_InsertText_Bottom`, then `GM_TEXTEDITOR_ARexxCmd "GOTOBOTTOM"` for auto-scroll.
- **IFF clipboard**: `FORM FTXT { CHRS }` wrapper for Copy to Clipboard. `PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)` → `PushChunk(0, ID_CHRS, ...)` → data → PopChunk × 2.
- **Context menu**: Select All + Copy on texteditor gadget. `GM_TEXTEDITOR_MarkText` with `start=(0,0)` and `stop=(0xFFFFFFFF,0xFFFFFFFF)` for read-only Select All.
- **Bug fix**: `WINDOW_MenuType` must be checked before `WINDOW_MenuAddress` to prevent classic menu events from being swallowed by context menu handler.

### v2.3.7 (27.02.2026)
- S.M.A.R.T. column auto-fit (detach/reattach with `LISTBROWSER_AutoFit`).
- Average method display as combined label.
- Bulk tab Settings text includes averaging method.
- Disk Info: improved naming, unmounted partition details, underscore sanitization, DOS type ASCII display.
- Bug fixes: DSI crash on tab page switch, drive categorization, Not Mounted sorting.

### v2.3.4 – v2.3.6 (27.02.2026)
- Flexible Pass Averaging (All Passes, Trimmed Mean, Median).
- Average Method visible on Benchmark tab.
- Disk Info tree restructured.
- Bug fixes: underscore keyboard shortcuts, Preferences chooser lifetime, INTEGER_MaxChars ordering.

### v2.2.16
- Disk Information Center: hierarchical tree view (Fixed, USB, Optical).
- True hardware scanning via DosList + SCSI inquiry.
- CD/DVD safeguards (skip VPD Page 0x80).
- Bug fixes: DSI on startup, layout stuttering.

### v2.2.14
- Release optimization (debug logging disabled).

### v2.2.11
- Multi-threaded benchmark engine, CSV history persistence.

### v2.2.10
- Visualization engine, variable block sizes, Traffic Light + Fuel Gauge.

## Known Quirks & Gotchas

### ReAction / GUI
- **CHOOSER_Labels requires struct List \***: Always pass `(uint32)&my_list` (an Exec `struct List *`), never a plain C string array. Build nodes via `IChooser->AllocChooserNode()`. See v2.3.1 crash fix.
- **chooser.gadget list lifetime**: The chooser walks its node list **live** every time the popup opens. It does NOT copy the list at `NewObject` time. The list must remain allocated for the window's entire lifetime.
- **INTEGER_MaxChars must come first**: Always place `INTEGER_MaxChars` before `INTEGER_Minimum`, `INTEGER_Maximum`, and `INTEGER_Number`. Tags are processed sequentially; MaxChars affects value clamping.
- **Underscores in chooser label text**: `_X` underlines `X` as a keyboard shortcut. Replace `_` with ` ` (space) in display text. If used for filter matching, sanitize both sides.
- **CIF_WEIGHTED for auto-fit columns**: Fixed-width columns won't auto-expand with `LISTBROWSER_AutoFit`. Use weighted columns with proportional `ci_Width` values.
- **Inline labels**: Use read-only transparent `ButtonObject` instead of bare `LabelObject` in `HLayoutObject` for visible text labels.
- **WM_RETHINK must NOT be called from inside WM_HANDLEINPUT**: Causes re-entrant layout pass → DSI crash. Use a deferred flag: set in handler, call `WM_RETHINK` after the event loop iteration.
- **LM_REMOVECHILD auto-disposes**: Never call `DisposeObject` after `LM_REMOVECHILD` — double-free crash.
- **ListBrowser AutoFit re-trigger**: For on-demand data, detach labels (`-1`), then reattach with `LISTBROWSER_AutoFit, TRUE` in one `RefreshSetGadgetAttrs` call.
- **LBNCA_CopyText MUST come BEFORE LBNCA_Text**: Tags processed sequentially; CopyText allocates a buffer that Text then fills.
- **DoGadgetMethodA vs IDoMethodA for texteditor**: `IDoMethodA` skips render pass. Use `DoGadgetMethodA(gadget, window, NULL, msg)` for methods that must cause immediate visual update.
- **WINDOW_MenuType before WINDOW_MenuAddress**: `WINDOW_MenuAddress` returns non-NULL for classic menu events (returns menu strip root). Without type check, context-menu code fires on every classic menu pick.
- **CHOOSER_Labels ownership**: Autodoc says list is "freed automatically when the object is disposed." Do NOT free manually — double-free crash.

### Visualization System
- **Missing Visualizations/ folder is fatal**: `LoadVizProfiles()` returns FALSE → EasyRequest error → exit. No built-in fallback charts.
- **Profile chooser index = array index**: `g_viz_profiles[ui.viz_chart_type_idx]` is the active profile. Profiles are sorted alphabetically by name.
- **Collapse for non-block_size X**: When X source is `test_index` or `timestamp`, collapse reduces the entire series to a single aggregated point rather than deduplicating per-X-value.
- **Trend line coordinate clamping**: All trend line pixel coordinates are clamped to `[chart_left, chart_right]` × `[chart_top, chart_bottom]` to prevent drawing outside the graph area.
- **ApplyFilterList semantics**: Exclude mode = skip if value matches any entry. Include mode = skip if value matches NONE of the entries. Empty filter list = pass all.
- **Filter string matching**: Hardware/vendor/product filters use substring matching (`strstr`). Test/filesystem/volume use exact match (`Stricmp`).

### Engine / Worker
- **IUtility is NULL in CreateNewProc workers**: Worker processes bypass `crt0` startup so library globals are not initialised. Use C stdlib `vsnprintf`/`snprintf`/`strncpy` instead of `IUtility->VSNPrintf`/`SNPrintf`/`Strlcpy`.
- **IOPS = total_ops / total_elapsed_time**: Not ops per pass. Each workload's `Run()` must return `bytes / block_size` as `op_count`, not `1`.
- **Worker shutdown protocol**: Each submitted `BenchJob` produces TWO messages: a `BenchStatus` `PutMsg` and the original job `ReplyMsg` (with `mn_Node.ln_Type == NT_REPLYMSG`). On quit, drain the pending queue (`CleanupBenchmarkQueue()`), then loop `WaitPort`/`GetMsg` until `NT_REPLYMSG` arrives to confirm the worker is done.
- **ReadArgs guard**: `ReadArgs("VALIDATE/S", ...)` must only be called when `IDOS->Output() != NULL` (Shell context). On Workbench launch, `Output()` is NULL and `ReadArgs` fails, leaving a stale result that can cause a blank window.

### Low-Level Disk / Hardware
- **SCSI Inquiry freezes on CD-ROMs**: Skip VPD Page 0x80 for Peripheral Device Type 0x05.
- **System Requesters on empty drives**: Wrap `IDOS->Lock` with `SetProcWindow((APTR)-1)`.
- **Device name needs trailing colon**: Lock `DH0:` not `DH0`.
- **IOExtTD allocation**: Use `sizeof(struct IOExtTD)` for trackdisk.device IORequests.
- **SCSI_INQUIRY & AUTOSENSE**: If `SCSIF_AUTOSENSE` is set, `scsi_SenseData` buffer must be non-NULL.

### Build / Versioning
- **Versioning rule**: Bug fixes increment `BUILD` by 1 only. New features or UI changes increment `MINOR` by 1 and reset BUILD.
- **PowerPC Alignment**: Be careful with `snprintf` and mixed-type varargs. Use incremental string construction.
- **64-bit Formatting**: Use `%llu` and cast to `unsigned long long` — `PRIu64` may not be defined correctly.
- **`IUtility` does NOT have**: `Strstr`, `Strchr`, `Strlen`, `AToLong` — use C stdlib.
- **`IUtility` DOES have**: `Stricmp`, `Strnicmp`, `Strlcpy`, `SNPrintf`, `VSNPrintf`.

## Debugging
- **Debug Port**: Logs are sent to the serial port (view via `KDebug` or serial terminal).
- **`debug.h`**: Toggle `ENABLE_DEBUG_LOGGING` to enable/disable detailed traces.

## Future Roadmap
- **Persistent Log Files**: Save session logs to `PROGDIR:logs/session_YYYYMMDD.log`.
- **AmigaOS native icons**: Proper `.info` icons for the `dist/` binary.
- **Additional .viz profiles**: Users can create their own without recompilation.
