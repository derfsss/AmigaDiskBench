# Knowledge Transfer: AmigaDiskBench v2.2.11 Implementation

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

## 6. Low-Level Disk Access & Quirks (v2.2.16)
- **SCSI Inquiry Freezes**: Many older ATAPI CD-ROM drives (and some emulated ones like QEMU) hang indefinately if queried for VPD Page 0x80 (Serial Number). 
    - **Solution**: Always check the "Peripheral Device Type" (Byte 0) from the Standard Inquiry first. If it is `0x05` (CD-ROM), **SKIP** all VPD page queries.
- **System Requesters on Empty Drives**: Calling `IDOS->Lock("CD0:", ...)` on an empty drive triggers a "No Disk present" system requester, blocking the UI.
    - **Solution**: Wrap the lock call with `APTR old = IDOS->SetProcWindow((APTR)-1); ... IDOS->SetProcWindow(old);` to suppress these popups.
- **Device Locking**: To retrieve filesystem info for a partition, you must lock the *Root Volume*. Ensure you append a colon to the device name (e.g., `DH0:` not `DH0`). Locking `DH0` might fail or return a lock to the device node itself, which doesn't support `Packet_Info`.
- **Filesystem String Formatting**: Standard Amiga practice for DOS Types (e.g., `SFS\0`, `DOS\3`) is to print the last byte as hex if it's non-printable or version-significant. Use format `%c%c%c/%02X`.
