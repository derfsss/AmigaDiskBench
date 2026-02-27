# Knowledge Transfer: AmigaDiskBench v2.3.3 Implementation

This document captures detailed technical learnings and patterns discovered during the implementation of version 2.2.11.

## 1. ReAction UI & Gadget State
- **Initial Selection**: When a ReAction `ChooserObject` (or similar) depends on a global state variable (like `ui.viz_date_range_idx`), always include `CHOOSER_Selected, ui.variable` in the gadget definition within `CreateMainLayout`. Failing to do so can cause the UI to default to index 0 regardless of the variable's initial value.
- **Dynamic Label Refreshing**: Reattaching labels to a `ListBrowser` or `Chooser` requires detaching them first (`LISTBROWSER_Labels, (ULONG)-1`) to avoid internal OS corruption or missed updates.

## 2. CSV History Management & Sanitization
- **Sanitization Strategy**: Integrating sanitization directly into the `RefreshHistory` loading loop is efficient. 
    - **Detection**: Check each record for validity (e.g., block size 0 for fixed-behavior tests).
    - **Persistence**: If any record is modified during loading, set a `needs_sanitization` flag and call `SaveHistoryToCSV` *after* the loop closes the file handle. This keeps the CSV file as a "clean" source of truth.
- **Timestamped Filenames**: Using `time.h` and `strftime` is fully supported in the `walkero/amigagccondocker:os4-gcc11` environment. The format `bench_history_%Y-%m-%d-%H-%M-%S.csv` preserves sortability and prevents data loss via overwrite.

## 3. Workload Standardization
- **Block Size Sentinels**: For tests that do not utilize a configurable block size (e.g., `TEST_PROFILER`, `TEST_DAILY_GRIND`), the engine and GUI worker should strictly enforce a value of `0` (Mixed). This prevents the UI from accidentally reporting a leftover block size from a previous run or gadget interaction.
- **Centralized Enforcement**: Force these values in both `LaunchBenchmarkJob` (GUI side) and `RunBenchmark` (Engine side) to ensure consistency regardless of the trigger (UI, Bulk, or CLI-like calls).

## 4. Build Environment (WSL2 + Docker)
- **Volume Mapping**: Always use the `/mnt/[drive]/...` syntax for volume mounts in Docker when running under WSL2.
- **Distribution Consistency**: Ensure the WSL distribution (`-d Ubuntu`) matches the path expectations (e.g., `wsl -d Ubuntu -e sh -c "..."`).

## 5. Versioning
- **Header Files**: `include/version.h` is the single source of truth. Updating `VERSION_REV` and `VERSION_BUILD` here automatically propagates to the AppTitle, Version String, and internal metadata. 

## 6. Preferences Window — ReAction Chooser Pattern (v2.3.1 / v2.3.2)
- **CHOOSER_Labels requires a struct List \***: The `CHOOSER_Labels` tag expects a pointer to an AmigaOS Exec `struct List` populated with chooser nodes, **not** a plain C string array. Passing a `const char *[]` compiles silently but crashes at runtime as the chooser walks the string pointer as a linked list head.
- **chooser.gadget walks the list live** — it does **not** deep-copy the list at `NewObject` time. The list must remain allocated for the entire lifetime of the window. Store it in `GUIState` and free it only after `DisposeObject` on the window.
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
- **INTEGER_MaxChars must come first**: In an `IntegerObject` tag list, always place `INTEGER_MaxChars` before `INTEGER_Minimum`, `INTEGER_Maximum`, and `INTEGER_Number`. AmigaOS processes tags sequentially and `MaxChars` affects value clamping — placing it after the value can silently clamp the displayed number.

## 8. Chooser Label Text & Underscore Escaping (v2.3.2 / v2.3.3)
- **`_X` in chooser text underlines `X` as a keyboard shortcut**. Any `_` in a volume name (e.g., `SFS_DH7_2`) will cause the following character to be underlined in the chooser popup.
- **Fix**: Replace `_` with ` ` (space) when building chooser node text. Do **not** use `__` (double underscore) — that displays two underscores visually.
- **Apply consistently** to every site that builds chooser nodes from user-data strings (volume names, etc.):
    - `gui_system.c` — drive selector chooser (volume name → display string)
    - `gui_viz.c` — volume filter chooser (history `volume_name` → chooser label)
- **Filter comparisons break if you only change display text**: If the chooser `CNA_Text` is sanitized and the filter comparison reads it back via `CNA_Text` to compare against a raw internal string, the strings won't match. **Solution**: sanitize both sides — create a `SanitizeNameForChooser()` helper and apply it to both the stored chooser text and the candidate string in the filter comparison.
    ```c
    static void SanitizeNameForChooser(const char *src, char *dst, size_t dst_size)
    {
        size_t i = 0;
        while (src[i] && i < dst_size - 1) {
            dst[i] = (src[i] == '_') ? ' ' : src[i];
            i++;
        }
        dst[i] = '\0';
    }
    ```

## 7. Averaging Methods & Read-Only Display Labels (v2.3.1 / v2.3.3)
- **Three-option AveragingMethod enum** in `engine.h`:
    - `AVERAGE_ALL_PASSES` (0): Simple arithmetic mean of all pass results.
    - `AVERAGE_TRIMMED_MEAN` (1): Finds the single min and max, excludes them, averages the rest. Falls back to All Passes if fewer than 3 passes.
    - `AVERAGE_MEDIAN` (2): `qsort()` all results; index = `round(passes / 2.0) - 1` (picks lower-middle for even counts). `effective_passes = 1`.
- **Data flow**: `ui.averaging_method` (GUIState) → `BenchJob.averaging_method` → `RunBenchmark()` → `engine_tests.c` switch statement → `BenchResult.averaging_method` (persisted to CSV).
- **Prefs persistence**: stored/loaded as `"AveragingMethod"` key in the application.library PrefsObjects dictionary.
- **Read-only display gadget pattern**: To surface a preference value in the main window without adding a live gadget tied to the Prefs window, use a `GA_ReadOnly, TRUE` `ButtonObject`. Store it in `GUIState` (e.g., `ui.avg_method_label`). Update it:
    1. At startup: call `UpdateAvgMethodLabel()` immediately after `LoadPrefs()` in `gui.c`, once gadgets exist.
    2. After prefs save: call `UpdateAvgMethodLabel()` from `UpdatePreferences()` in `gui_prefs.c`.
    - The update function reads `ui.averaging_method` (already set before the call) and pushes the matching label string via `IIntuition->SetGadgetAttrs()`.
- **File-scope label string array**: Move label string arrays to file scope (not inside a function) when they are needed by more than one function in the same `.c` file (e.g., both `OpenPrefsWindow` and `UpdateAvgMethodLabel`). Pair with a `#define NUM_*` constant for the array length.

## 9. Low-Level Disk Access & Quirks (v2.2.16)
- **SCSI Inquiry Freezes**: Many older ATAPI CD-ROM drives (and some emulated ones like QEMU) hang indefinately if queried for VPD Page 0x80 (Serial Number). 
    - **Solution**: Always check the "Peripheral Device Type" (Byte 0) from the Standard Inquiry first. If it is `0x05` (CD-ROM), **SKIP** all VPD page queries.
- **System Requesters on Empty Drives**: Calling `IDOS->Lock("CD0:", ...)` on an empty drive triggers a "No Disk present" system requester, blocking the UI.
    - **Solution**: Wrap the lock call with `APTR old = IDOS->SetProcWindow((APTR)-1); ... IDOS->SetProcWindow(old);` to suppress these popups.
- **Device Locking**: To retrieve filesystem info for a partition, you must lock the *Root Volume*. Ensure you append a colon to the device name (e.g., `DH0:` not `DH0`). Locking `DH0` might fail or return a lock to the device node itself, which doesn't support `Packet_Info`.
- **Filesystem String Formatting**: Standard Amiga practice for DOS Types (e.g., `SFS\0`, `DOS\3`) is to print the last byte as hex if it's non-printable or version-significant. Use format `%c%c%c/%02X`.
- **Trackdisk IORequests**: `AllocSysObjectTags` for `ASOT_IOREQUEST` must allocate enough space for the target device's extended request structure (e.g. `sizeof(struct IOExtTD)` instead of `struct IOStdReq`). Failing to do so causes buffer overruns and DSI Exceptions on device response.
- **Unmounted / Foreign Partitions**: Partitions with bizarre handlers (like CDFileSystem mounted on a hard disk) may fail to return valid data via `IDOS->Lock`. Ensure the GUI gracefully falls back to displaying an "Unmounted" state rather than dereferencing null `InfoData` structures.
- **QEMU Emulated Drives & S.M.A.R.T.**: QEMU's block device emulation (e.g., `SCSIDiskState`) does not emulate raw ATA commands like ATA PASS-THROUGH. S.M.A.R.T. polling will fail gracefully. Only generic pass-through (`/dev/sg*`) supports ATA commands.
- **Root Node Tab Switch Glitches**: When switching back to a tree view layout from another tab, ensure the selected payload resets properly. It's often necessary to clear the detail panel layout manually if the user left a root node selected.
