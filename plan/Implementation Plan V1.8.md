# Implementation Plan: AmigaDiskBench v1.8

Enhance robustness, add advanced benchmarking capabilities, and deepen OS integration through `application.library` and `locale.library`.

## Proposed Changes

### [Component] Hardware Information
Summary: Retrieve and display physical drive details in the drive selector.

#### [MODIFY] [engine.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/engine.c)
- **`GetFileSystemInfo`**: Extend to use `IDOS->GetDiskInfoTags` with `GDI_DeviceName` or SCSI Inquiry to retrieve Vendor and Product strings.
- **Goal**: Populate a more detailed "Drive Info" string.

#### [MODIFY] [gui.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/gui.c)
- **`RefreshDriveList`**: Update the chooser node labels to include the retrieved hardware information (e.g., "Samsung SSD 860 (sb600sata.device)").

---

### [Component] Advanced Benchmarking Engine
Summary: Implement multi-pass testing and variable block sizes for more accurate results.

#### [MODIFY] [engine.h](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/include/engine.h)
- Update `RunBenchmark` to support a `passes` parameter and `block_size` override.

#### [MODIFY] [engine.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/engine.c)
- **`RunBenchmark`**: Implement a loop for multiple passes.
- **Outlier Rejection**: Implement logic to discard the highest and lowest results from a set of runs before averaging.
- **`WriteDummyFile`**: Support variable block sizes.

---

### [Component] Data Integrity & OS Integration
Summary: Add CSV versioning, preference persistence, and scriptability.

#### [MODIFY] [main.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/main.c)
- **Unique Instance**: Use `application.library` to ensure only one instance is running.

#### [MODIFY] [gui.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/gui.c)
- **Preferences**: Implement loading/saving of benchmark options (Passes, Block Sizes) using `application.library` (Prefs Objects).
- **Locale**: Replace hardcoded strings with `locale.library` `GetCatalogStr` calls.
- **Sorting**: Implement `WMHI_GADGETUP` handling for `ListBrowser` column clicks to allow manual sorting.

#### [NEW] [arexx.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/arexx.c)
- Implement an ARexx host port to support commands: `RUN`, `GETRESULT`, `QUIT`, `SELECTDRIVE`.

---

### [Component] Assets & Polish
Summary: Update visual assets and add detailed stats view.

#### [MODIFY] [gui.c](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/src/gui.c)
- **Detailed View**: Add a double-click handler or separate "View Details" button to open a window showing full breakdown of multi-pass stats.

#### [MODIFY] [tool.info](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/dist/AmigaDiskBench.info)
- Replace with high-quality application icon.

## Verification Plan

### Automated Tests
- Cross-compile using the `/build` workflow.
- Verify `AREXX` port presence using `RXLIST` (or similar Amiga tool if available).
- Check `bench_history.csv` for the new "Version" column.

### Manual Verification
- Run a multi-pass benchmark and verify that the "Details" view shows the expected averages.
- Change a preference (e.g., default passes) and restart the app to ensure persistence.
- Test "Unique Instance" by trying to launch the app twice.
