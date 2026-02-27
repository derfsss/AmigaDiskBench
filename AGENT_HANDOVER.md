# Agent Handover: AmigaDiskBench

This document provides essential context for any AI agent or developer picking up work on the AmigaDiskBench project.

## Project Overview
AmigaDiskBench is a modern, ReAction-based disk benchmarking utility for AmigaOS 4.x. It supports various test types (Sequential, Random, Mixed, Meta-profiling), S.M.A.R.T. health monitoring, hierarchical disk information, and real-time multi-series visualization.

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

When invoking from Windows (e.g., VS Code terminal via `wsl -e sh -c "..."`), `$(pwd)` expands correctly inside the quoted string. Each `make` target should be run as a separate Docker invocation.

## Current Version
**v2.3.7** (version.h: VERSION=2, REVISION=3, MINOR=7, BUILD=1116, date 27.02.2026)

## Architecture & Key Components
- `src/main.c`: Entry point, initializes the GUI.
- `src/gui.c`: Core ReAction GUI logic and event loop.
- `src/gui_layout.c`: GUI layout definition using ReAction objects.
- `src/gui_events.c`: IDCMP / gadget event dispatch.
- `src/gui_worker.c`: Worker process for benchmark execution; posts progress/completion messages.
- `src/gui_prefs.c`: Preferences window — open, update, load/save via application.library PrefsObjects.
- `src/gui_bulk.c`: Bulk queue management and sequential job dispatch.
- `src/gui_viz.c`: Visualization tab state — filter lists, data collection.
- `src/gui_viz_render.c`: Custom Intuition rendering hook for the multi-series graph.
- `src/gui_info.c`: Disk Information tab UI (Master-Detail tree view).
- `src/gui_health.c`: Drive Health tab (S.M.A.R.T. display).
- `src/gui_history.c`: History tab — CSV load, ListBrowser population, delete/export.
- `src/engine.c`: Benchmarking engine core — runs in a separate process.
- `src/engine_tests.c`: Per-test dispatch, multi-pass loop, averaging calculation.
- `src/engine_persistence.c`: CSV saving and loading for history.
- `src/engine_diskinfo.c`: Disk enumeration and hardware information retrieval.
- `src/engine_smart.c`: S.M.A.R.T. data retrieval via ATA PASS-THROUGH.
- `src/workloads/`: One file per workload strategy (sequential, random, mixed, legacy profiles, profiler).
- `include/version.h`: Single source of truth for version string.
- `include/gui.h`: GUIState struct, GID_* enum, BenchJob/BenchStatus message types.
- `include/gui_internal.h`: Shared internal header — all ReAction/OS includes, internal prototypes.
- `include/engine.h`: BenchResult, AveragingMethod enum, engine API.

## Version History

### v2.3.7 (Current — 27.02.2026)
- **S.M.A.R.T. column auto-fit**: Root cause was a timing problem — `LISTBROWSER_AutoFit` sized columns when the list was empty (health data loaded on demand, not at window creation). Fixed by detaching labels (`LISTBROWSER_Labels, (uint32)-1`) then reattaching with `LISTBROWSER_AutoFit, TRUE` in a single `RefreshSetGadgetAttrs` call at the end of `RefreshHealthTab()`. All `health_cols[]` use default (weighted, no `CIF_FIXED`) with Attribute Name weight=200, others 30/50/50/50/80/50 — matches the working History tab pattern.
- **Average method display**: Replaced the two-gadget layout (static "Average:" button + separate `avg_method_label`) with a single transparent `ButtonObject`. Text is the full combined string, e.g. `"Average: Median (Middle Value Only)"`. Updated `avg_label_strs[]` in `gui_prefs.c` to include the prefix. Eliminates the spacing inconsistency that could not be resolved with the two-gadget approach.
- **Bulk tab Settings text**: `UpdateBulkTabInfo()` includes the averaging method name: `Settings: Sprinter / 10 Passes (Median) / 4K`.
- **Bug fixes**: Disk Info categorization (fixed drives appearing in Optical section), Not Mounted partition sorting, S.M.A.R.T. tab crash on first open (caused by `WM_RETHINK` on anonymous ButtonObject — replaced with gadget-scoped AutoFit re-trigger).

### v2.3.4 – v2.3.6 (27.02.2026)
- **Flexible Pass Averaging**: `AVERAGE_MEDIAN` added to `AveragingMethod` enum. Engine sorts pass results and picks the middle value. Three options: All Passes, Trimmed Mean, Median.
- **Average Method on Benchmark tab**: `ui.avg_method_label` ButtonObject in the Passes row shows the active method at all times; updated by `UpdateAvgMethodLabel()` on prefs save.
- **Passes row alignment**: Passes+Average row uses `CHILD_Label` on the enclosing `VLayout` entry so it aligns with Drive/Test/Block rows.
- **Disk Info tree restructured**: Drive nodes show device name only. Multiple units merged under one node. Partition nodes show `VolumeName (Unit N)`.
- **Bug fixes**: Underscore in volume names (ReAction shortcut trigger), Preferences chooser lifetime crash, `INTEGER_MaxChars` tag ordering.

### v2.3.3 (27.02.2026)
- **S.M.A.R.T. Attribute Name column auto-width**: Changed from fixed `250px` to `CIF_WEIGHTED` in `health_cols[]` (`gui_layout.c`). Column now fills available space, preventing text truncation.
- **Underscore replacement in chooser labels**: Volume names containing `_` (e.g., `SFS_DH7_2`) were triggering ReAction's keyboard-shortcut underline mechanism (`_X` underlines `X`). Changed escape from `__` (double underscore) to a plain space in `gui_system.c`. Also applied to the Visualization tab's volume filter chooser (`gui_viz.c`) with a `SanitizeNameForChooser()` helper. Filter comparison logic updated to sanitize both sides so filtering still works correctly.
- **Average Method display on Benchmark tab**: A read-only `ButtonObject` (`ui.avg_method_label`) now appears in the same row as the Passes IntegerObject in Benchmark Control. It shows the currently active averaging method (e.g., "All Passes") and updates whenever Preferences are saved. Initialized at startup via `UpdateAvgMethodLabel()` after `LoadPrefs()`.
- **Disk Info tree restructured**: Drive nodes now show only the device name (e.g., `a1ide.device`) — unit number removed from this level. Partition nodes now show `VolumeName (Unit N)` (e.g., `DH3 (Unit 2)`), making it clear which physical unit owns each partition when a controller has multiple drives.

### v2.3.2 (27.02.2026)
- **Preferences chooser list lifetime fix**: The Averaging Method chooser list was freed immediately after the `WindowObject` macro. `chooser.gadget` walks the list live each time the popup opens — it does not copy at creation time. Fixed by storing the list in `ui.prefs_avg_list` (GUIState) and freeing it in all three close paths (Save, Cancel, WM_CLOSEWINDOW).
- **Volume name underscore escaping**: Added `_` → `__` escaping in `gui_system.c` when building drive chooser display strings (later improved to space in v2.3.3).

### v2.3.1 (26.02.2026)
- **Averaging Method — Median**: Added `AVERAGE_MEDIAN` (index 2) to the `AveragingMethod` enum in `engine.h`. The engine sorts pass results and picks the single middle value. This completes a 3-option set: All Passes, Trimmed Mean, Median.
- **Preferences crash fix**: `OpenPrefsWindow()` in `gui_prefs.c` was passing a raw `const char *[]` C array to `CHOOSER_Labels`. That tag requires a `struct List *` (Exec linked list of chooser nodes). Fixed by building a proper `avg_list` via `IChooser->AllocChooserNode()` / `IExec->AddTail()`.
- **INTEGER_MaxChars fix**: Added `INTEGER_MaxChars, 2` before `INTEGER_Minimum`/`INTEGER_Maximum`/`INTEGER_Number` on the Passes `IntegerObject` in Preferences. MaxChars must precede bounds tags (AmigaOS processes tags sequentially; MaxChars affects value clamping).

### v2.2.16 (Disk Information & Hardware Scanning)
- **Structure**: Master-Detail view with hierarchical ListBrowser categorized by Drive Type (Fixed, USB, Optical). Partitions are direct children of the drives.
- **UI**: Automatic layout adjustment using `WM_RETHINK`; `CHILD_MinWidth` for consistent labeling; strict vertical shrink-wrapping.
- **Real Scanning**: Replaced dummy data with actual `DosList` iteration (LDF_DEVICES), filtering strictly for storage devices (SCSI, IDE, USB, NVMe) and excluding serial/parallel ports.
- **SCSI Inquiry**: Identifies CD/DVD drives by Peripheral Device Type (0x05). Skips VPD Page 0x80 queries for CD-ROMs to prevent system freezes.
- **Stability**: Wrapped `IDOS->Lock` calls with `SetProcWindow((APTR)-1)` to suppress "No Disk" system requesters on empty drives.
- **Filesystem Display**: Standardized `GetDosTypeString` to format DOS types as `ABC/XX` (Hex).
- **UI Hotfixes**: Resolved a DSI Exception by migrating `GA_Text` string pointers from the stack to static memory. Added hide logic for unmounted tree partitions and a Refresh button.
- **Memory Fix**: `ASOT_IOREQUEST` allocation uses `sizeof(struct IOExtTD)` instead of `sizeof(struct IOStdReq)` for trackdisk.device and SCSI devices — prevents buffer overrun / DSI Exception on device response.

### v2.2.14 (Release Finalisation)
- Disabled debug logging for release builds.
- Versioning bumps and code documentation cleanup.

### v2.2.11 (CSV Export & UI Enhancements)
- Multi-threaded benchmark engine, CSV history persistence.
- Refer to [KNOWLEDGE_TRANSFER.md](KNOWLEDGE_TRANSFER.md) for detailed technical patterns.

### v2.2.10 (Visualization & Usability)
- Visualization engine overhauled; X-axis = Block Size; auto-refresh on tab switch.
- Variable block sizes for Random I/O tests.
- Traffic Light status indicator and Fuel Gauge progress bar added.
- Bubble Help tooltips (`GA_HintInfo`) added to most controls.

## Known Quirks & Gotchas
- **CHOOSER_Labels requires struct List \***: Always pass `(uint32)&my_list` (an Exec `struct List *`), never a plain C string array. Build nodes via `IChooser->AllocChooserNode()`. See v2.3.1 crash fix for a concrete example.
- **chooser.gadget list lifetime**: The chooser walks its node list **live** every time the popup opens. It does NOT copy the list at `NewObject` time. The list must remain allocated for the entire window lifetime — store it in `GUIState`, free it on all close paths (Save, Cancel, WM_CLOSEWINDOW).
- **INTEGER_MaxChars must come first**: Always place `INTEGER_MaxChars` before `INTEGER_Minimum`, `INTEGER_Maximum`, and `INTEGER_Number` in an IntegerObject tag list. Tags are processed sequentially; MaxChars affects value clamping.
- **Underscores in chooser label text**: `_X` underlines `X` as a keyboard shortcut. Volume names or any user-data strings with `_` must be sanitized — replace `_` with ` ` (space). Do **not** use `__` (visible as two underscores). If the chooser text is also used for filter matching, sanitize both sides of the comparison.
- **CIF_WEIGHTED for auto-fit columns**: A column with a fixed pixel width (e.g., `{250, "Name", CIF_SORTABLE}`) will NOT auto-expand even when `LISTBROWSER_AutoFit, TRUE` is set on the gadget. Use `{200, "Name", CIF_WEIGHTED | CIF_SORTABLE}` to allow proportional auto-sizing. **Important**: use a non-zero weight value — `{0, ..., CIF_WEIGHTED}` means zero proportional share and the column receives no space at all.
- **Inline label in HLayoutObject**: A bare `LabelObject` added directly as a `LAYOUT_AddChild` inside an `HLayoutObject` (without `CHILD_Label` context) may not render as visible text. Use a read-only transparent `ButtonObject` instead: `ButtonObject, GA_ReadOnly, TRUE, GA_Text, "Label:", BUTTON_BevelStyle, BVS_NONE, BUTTON_Transparent, TRUE, BUTTON_Justification, BCJ_LEFT, End`.
- **Disk Info category/unit dedup**: When merging PhysicalDrive units under one device node, the inner partition loop must check BOTH `device_name == match` AND `media_type`/`bus_type` matches the current category. Without the category check, a device like `a1ide.device` with both fixed and optical units will contaminate all categories with all its partitions.
- **AllocListBrowserNode**: Do not pass `IListBrowser` as the first argument — use `AllocListBrowserNode(column_count, TAGS...)` directly.
- **PowerPC Alignment**: Be careful with `snprintf` and mixed-type varargs (mixing `double` and `uint32`). Large or complex calls can cause stack corruption. Use incremental string construction where possible.
- **64-bit Formatting**: Use `%llu` and cast to `unsigned long long` — `PRIu64` may not be defined correctly in the SDK.
- **Custom Rendering**: The visualization graph uses a custom `gpRender` hook. `WA_IDCMP` must include `IDCMP_MOUSEBUTTONS` for hover detection.
- **ListBrowser Hierarchy**: Parent nodes must have `LBNA_Flags` set to `LBFLG_HASCHILDREN | LBFLG_SHOWCHILDREN` — nesting nodes alone does not show expand handles.
- **Layout Refresh**: Use `IIntuition->IDoMethod(window_obj, WM_RETHINK)` to force re-layout after page switches. `RefreshGList` is insufficient for complex nested layouts. **WARNING**: `WM_RETHINK` re-layouts the entire window tree including all hidden pages. If any anonymous `ButtonObject` (no `GA_ID`, not stored in `GUIState`) is present in any page of the layout, `WM_RETHINK` will crash with a DSI exception inside `button.gadget`. Avoid calling `WM_RETHINK` from on-demand data refresh functions.
- **ListBrowser AutoFit re-trigger**: When a ListBrowser is populated on demand (not at window creation), `LISTBROWSER_AutoFit` has already sized columns against an empty list. To recalculate: call `SetGadgetAttrs` with `LISTBROWSER_Labels, (uint32)-1` (detach), then `RefreshSetGadgetAttrs` with `LISTBROWSER_Labels, (uint32)&list, LISTBROWSER_AutoFit, TRUE` (reattach with re-fit). This is scoped only to the ListBrowser gadget and is safe to call from any refresh function.
- **CIF_WEIGHTED interaction with CIF_FIXED**: When mixing `CIF_FIXED` columns with default (weighted) columns, `LISTBROWSER_AutoFit` may not distribute space as expected. The reliable pattern is for ALL data columns to be weighted (default, no `CIF_FIXED`) with proportional `ci_Width` values. This matches the pattern used by all working listers in the app (History, Drive Health).
- **Versioning rule**: Bug fixes increment `BUILD` by 1 only (MINOR unchanged). New features or UI changes increment `MINOR` by 1 and reset BUILD to the next round number.
- **SCSI_INQUIRY & AUTOSENSE**: If `SCSIF_AUTOSENSE` is set on an `HD_SCSICMD`, a valid `scsi_SenseData` buffer and `scsi_SenseLength` must be provided. A NULL pointer causes a kernel DSI Exception, especially on empty CD-ROMs via IDE.

## Debugging
- **Debug Port**: Logs are sent to the serial port (view via `KDebug` or serial terminal).
- **`debug.h`**: Toggle `ENABLE_DEBUG_LOGGING` to enable/disable detailed traces.

## Future Roadmap
- **User Progress Log** (v2.3.x): Scrollable log tab showing real-time plain-English benchmark progress, skip reasons for bulk queue items, and a Copy to Clipboard function. Design plan: `docs/plans/user_log_window.md`.
- **Persistent Log Files**: Save session logs to `PROGDIR:logs/session_YYYYMMDD.log` (Phase 2 of log feature).
- **AmigaOS native icons**: Proper `.info` icons for the `dist/` binary.
