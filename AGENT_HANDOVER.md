# Agent Handover: AmigaDiskBench

This document provides essential context for any AI agent or developer picking up work on the AmigaDiskBench project.

## Project Overview
AmigaDiskBench is a modern, ReAction-based disk benchmarking utility for AmigaOS 4.x. It supports various test types (Sequential, Random, Mixed, Meta-profiling) and provides real-time visualization.

## Build Environment
- **OS**: Windows (host) / WSL2 (build environment).
- **Toolchain**: AmigaOS 4.1 GCC 11 cross-compiler.
- **Docker Image**: `walkero/amigagccondocker:os4-gcc11`.
- **SDK Version**: 54.16.

### Build Commands
- `make clean all`: Build the project.
- `make install`: Copy binary to `dist/`.

## Architecture & Key Components
- `src/main.c`: Entry point, initializes the GUI.
- `src/gui.c`: Core ReAction GUI logic and event loop.
- `src/gui_layout.c`: GUI layout definition using ReAction objects.
- `src/engine.c`: Benchmarking engine (runs in a separate process).
- `src/engine_persistence.c`: CSV saving/loading for history.
- `src/gui_viz_render.c`: Custom intuition rendering for the trend graph.
- `src/workloads/`: Implementation of different test strategies.
- `src/engine_diskinfo.c`: Disk enumeration and information retrieval (IDosMethod, etc.).
- `src/gui_info.c`: Disk Information tab UI (Master-Detail with Tree View).

## Version History

### v2.2.16: Disk Information & Hardware Scanning
- **Structure**: Master-Detailed view with hierarchical ListBrowser categorized by Drive Type (Fixed, USB, Optical). Partitions are direct children of the drives.
- **UI**: Automatic layout adjustment using `WM_RETHINK`; consistent labeling with `CHILD_MinWidth` and strict vertical shrink-wrapping.
- **Real Scanning**: Replaced dummy data with actual `DosList` iteration (LDF_DEVICES), strictly filtering for storage devices (SCSI, IDE, USB, NVMe) while excluding serial/parallel ports.
- **SCSI Inquiry**: Enhanced to identify CD/DVD drives by Peripheral Device Type (0x05). Implemented logic to SKIP VPD Page 0x80 queries for CD-ROMs to prevent system freezes.
- **Stability**: Wrapped `IDOS->Lock` calls with `SetProcWindow((APTR)-1)` to suppress "No Disk" system requesters on empty drives.
- **Filesystem Display**: Standardized `GetDosTypeString` to format DOS types as `ABC/XX` (Hex).
- **UI Hotfixes**: Resolved a DSI Exception by migrating string pointers for `GA_Text` attributes from the stack to static memory.

### v2.2.14: Finalizing Release
- **Build**: Bumped build number to release target.
- **Configuration**: Disabled debug switch for release builds.
- **Documentation**: Formatted and updated core code comments.

### v2.2.11: CSV Export & UI Enhancements
- **Architecture Highlights**: ReAction-based GUI, multi-threaded benchmark engine, CSV history persistence.
- **Knowledge Transfer**: Refer to [KNOWLEDGE_TRANSFER.md](file:///w:/Code/amiga/antigravity/projects/AmigaDiskBench/KNOWLEDGE_TRANSFER.md) for detailed technical patterns and learnings.

### v2.2.10: Visualization & Usability Update
- **Visualization Enhancements**: X-axis changed to Block Size; auto-refresh on tab switch.
- **Bubble Help**: Comprehensive tooltips (`GA_HintInfo`) added to most controls.
- **Variable Block Sizes**: Random tests now support user-selected block sizes (fixed-4KB behavior removed).
- **Stability Fixes**: Resolved multiple DSI crashes related to CSV export (alignment/varargs issues) and color allocation.
- **Visual Indicators**: Added Traffic Light (status) and Fuel Gauge (progress).

## Known Quirks & Gotchas
- **AllocListBrowserNode**: In the current SDK (54.16), explicitly passing the `IListBrowser` pointer as the first argument to `AllocListBrowserNode` can cause type-mismatch warnings or errors depending on how the prototypes are resolved. Stick to the format: `AllocListBrowserNode(column_count, TAGS...)`.
- **PowerPC Alignment**: Be extremely careful with `snprintf` and mixed-type varargs (e.g., mixing `double` and `uint32`). Large or complex `snprintf` calls have caused stack corruption/misalignment issues. Use incremental string construction if possible.
- **Custom Rendering**: The trend graph uses a custom `gpRender` hook. Ensure `WA_IDCMP` includes `IDCMP_MOUSEBUTTONS` for hover detection.
- **ListBrowser Hierarchy**: To show expansion handles (`+`/`-`), parent nodes MUST have `LBNA_Flags` set to `LBFLG_HASCHILDREN | LBFLG_SHOWCHILDREN`. Merely nesting nodes is not enough.
- **Layout Refresh**: When switching pages in a `page.gadget` layout on the fly, use `IIntuition->IDoMethod(window_obj, WM_RETHINK);` to force a complete re-layout. `RefreshGList` is often insufficient for complex layout changes.

## Debugging
- **Debug Port**: Logs are sent to the serial port (can be viewed via `KDebug` or similar on AmigaOS).
- **`debug.h`**: Toggle `ENABLE_DEBUG_LOGGING` to enable/disable detailed traces.

## Future Roadmap

### v2.2.12: S.M.A.R.T. Integration
- **Retrieval**: Investigate S.M.A.R.T. information retrieval mechanisms for AmigaOS 4.1.
- **Research**: Specific research into `IDiskinfo` interface or direct SCSI/ATA commands via `ExecDeviceIO`.
- **UI**: Design new "Drive Health" tab for S.M.A.R.T. data display.

### General
- Native AmigaOS icons for the `dist/` binary.
