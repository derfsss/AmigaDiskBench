# AmigaDiskBench

**AmigaDiskBench** is a modern, high-performance disk benchmarking utility specifically designed for **AmigaOS 4.1 Final Edition**. It provides a robust, ReAction-based GUI to measure, analyze, and visualize the performance of various storage devices, filesystems, and hardware configurations.

> This project was developed with the assistance of AI agents (Claude by Anthropic), which contributed to code design, implementation, and documentation throughout the development process.

## Key Features

### 1. Benchmark Profiles
Choose from a variety of tailored test scenarios:
- **Sprinter**: Fast, small file I/O and metadata performance test.
- **Marathon**: Sustained long-duration test to check for thermal throttling or buffer issues.
- **Heavy Lifter**: Large file sequential throughput with varying chunk sizes.
- **Daily Grind**: A pseudo-random mix of operations simulating real-world OS usage.
- **Profiler**: Detailed filesystem metadata performance analysis.
- **Standard Tests**: Sequential Read/Write, Random 4K Read/Write, and Mixed 70/30.

### 2. Flexible Pass Averaging
Choose how multi-pass results are combined, via **Preferences**:
- **All Passes**: Simple arithmetic mean across every pass.
- **Trimmed Mean**: Excludes the single best and worst pass, averages the rest (requires 3+ passes).
- **Median**: Sorts all passes and picks the single middle value — eliminates outliers without distorting the average.

### 3. Pluggable Visualization Profiles
Analyze your data with a powerful, profile-driven graphing engine. Chart definitions are loaded from `.viz` files in the `Visualizations/` folder — no recompilation needed to add or customize charts.

- **9 Built-in Profiles**:
  - **Scaling**: Performance vs. block size — see how chunk size affects throughput.
  - **Trend**: Track performance stability over time with linear trend lines.
  - **Battle**: Head-to-head comparison of multiple drives (collapsed to mean per block size).
  - **Workload**: Compare different test types (Read vs Write, Sequential vs Random).
  - **Hybrid**: Professional diagnostic view overlaying Throughput (MB/s) bars with IOPS lines.
  - **Peak Performance**: Maximum throughput per drive with SATA III and USB 2.0 reference lines.
  - **IOPS Smoothed**: Random I/O operations per second with moving average trend line.
  - **Scaling Curve**: Polynomial curve fit showing how throughput scales with block size.
  - **Filesystem**: Compare filesystem performance across all drives (grouped by filesystem type).
- **Profile Features**:
  - Chart type selection (line, bar, hybrid).
  - Configurable X/Y axes with custom labels and auto-scaling or fixed ranges.
  - Series grouping by drive, test type, block size, filesystem, hardware, vendor, app version, or averaging method.
  - Data collapse aggregation (mean, median, min, max) to reduce multiple runs to one data point.
  - Trend lines: linear regression, moving average, or polynomial curve fit.
  - Reference line annotations with labels.
  - Custom color palettes (up to 16 colors per profile).
  - Per-profile filters: include/exclude by test type, block size, volume, filesystem, hardware, vendor, product, averaging method, and app version.
  - Minimum pass count, MB/s range, and duration range filters.
- **Filtering**: On-screen filters for Volume, Test Type, Date Range, and App Version remain active on top of profile filters.
- **Reload Profiles**: Button to rescan the `Visualizations/` folder without restarting the application.
- **VALIDATE Mode**: Run with `VALIDATE` as a Shell argument or icon tooltype to check all `.viz` files for errors without launching the benchmark GUI. Reports line numbers and severity.
- **Interactive**: Hover over data points for precise values.

### 4. Drive Health Monitoring
Keep an eye on your hardware's physical status:
- **S.M.A.R.T. Analysis**: Reads raw attribute data directly from the drive via ATA PASS-THROUGH.
- **Health Indicators**: Real-time display of **Temperature**, **Power-On Hours**, and overall **Health Status**.
- **Assessment**: Automatic interpretation of critical attributes (Reallocated Sectors, Spin Retry Count, etc.).

### 5. Bulk Testing
Automate your benchmarking workflow:
- **Queue Jobs**: Select multiple drives and add them to a batch queue.
- **Automation**: Options to "Run All Test Types" and "Run All Block Sizes" (4K to 1M) automatically.
- **Progress Tracking**: Dedicated Fuel Gauge to track overall batch progress.

### 6. History & Data Management
- **Persistent Storage**: All results are automatically saved to `AmigaDiskBench_History.csv`.
- **Comparison**: Select any two results to generate a delta report (Speedup/Slowdown %).
- **Export**: Export specific datasets to CSV for external analysis.
- **Reports**: Generate global summary reports of all test activity.

### 7. Session Log
Track exactly what AmigaDiskBench is doing in real time:
- **Timestamped Entries**: Every event is logged with an `[HH:MM:SS]` timestamp.
- **Live Updates**: New log lines appear automatically as benchmarks run.
- **Context Menu**: Right-click the log to access Select All and Copy.
- **Clear Log / Copy to Clipboard**: Dedicated buttons to manage the session transcript.

### 8. Detailed Disk Information
Inspect the physical and logical structure of your storage:
- **Tree View**: Hierarchical display organized as *Fixed Drives*, *USB Drives*, and *Optical Drives*. Each physical controller appears once (e.g., `a1ide.device`); partitions show their volume/device name and unit (e.g., `System (Unit 0)`, `DH3 (Unit 2)`).
- **Disk Details**: Click a drive node to see the disk's human-readable identity (vendor + product + revision from SCSI inquiry), bus type, capacity, geometry, and whether an RDB is present.
- **Partition Details**: Click a mounted partition to see Volume Name, Size, Used/Free space, Filesystem type, and Block count. Click an unmounted partition to see its DOS device name, size (from RDB geometry), and filesystem type — all without mounting.
- **Real-Time Updates**: Refresh button to rescan when devices are added or removed.

## Requirements

*   **AmigaOS 4.1 Final Edition** (or newer).
*   Reasonably fast storage device for meaningful results (SSD/NVMe recommended).

## Installation

No special installation is required.
1.  Extract the archive to a location of your choice (e.g., `Work:Utilities/AmigaDiskBench`).
2.  Ensure the `Visualizations/` folder (containing `.viz` profile files) is in the same directory as the executable. The application requires at least one valid `.viz` file to start.
3.  Launch `AmigaDiskBench` from the icon.

## Visualization Profile Format

Each `.viz` file is an INI-style text file with sections:

```ini
[Profile]
Name        = "Scaling"
Description = "Performance vs. block size"
ChartType   = line

[XAxis]
Source      = block_size
Label       = "Block Size"

[YAxis]
Source      = mb_per_sec
Label       = "MB/s"
AutoScale   = yes

[Series]
GroupBy     = drive
MaxSeries   = 16
SortX       = yes
Collapse    = none

[Filters]
ExcludeTest = profiler
MinPasses   = 1

[TrendLine]
Style       = linear
PerSeries   = yes

[Annotations]
ReferenceLine = 600, "SATA III"

[Colors]
Color = 0x00FF00
Color = 0xFF6600
```

See the included `.viz` files in `Visualizations/` for working examples. Add your own profiles by creating new `.viz` files — they are picked up automatically on next launch or when clicking "Reload Profiles".

## Comprehensive Guide

### 1. Running Benchmarks
The **Benchmark** tab is where performance testing happens.
- **Target Drive**: Select the volume or partition you wish to test. Note that depending on the filesystem, some tests require write access.
- **Test Type**:
  - *Standard Tests*: Choose Sequential Read/Write, Random 4K Read/Write, or a Mixed 70/30 (Read/Write) workload.
  - *Profiles*: Use preset profiles like "Sprinter" for quick I/O checks, "Marathon" for sustained thermal testing, or "Daily Grind" for everyday usage simulation.
- **Parameters**: Adjust the **Block Size** (e.g., 4K, 32K, 1M) and the number of **Passes**. Higher passes yield more reliable averages.
- **Execution**: Click **Run Benchmark**. Monitor the **Traffic Light** (green/yellow/red) for current status, and the **Fuel Gauge** for overall progress.
- **Quit Safety**: If you attempt to close the application while a benchmark is running, a confirmation dialog will ask whether you really want to quit.

### 2. Bulk Testing / Queue
For extensive testing sessions, use the Batch Queue.
- Click **Add to Queue** to stockpile tests.
- Alternatively, use **Add All Tests** or **Add All Block Sizes** to quickly generate a matrix of workloads for the selected drive.
- Click **Start Queue** to let AmigaDiskBench run them sequentially.

### 3. Visualizing Results
The **Visualization** tab brings your data to life.
- **Profile Chooser**: Select a visualization profile from the dropdown. Each profile defines its own chart type, axes, grouping, filters, trend lines, and colors.
- **Filters**: On-screen filters for Volume, Test Type, and Date Range further narrow the displayed data.
- **Color By**: Automatically set by the selected profile's GroupBy setting (e.g., Drive, Filesystem, Test Type). Shown as a read-only label.
- **Reload Profiles**: Click to rescan the `Visualizations/` folder for new or modified `.viz` files.
- **Hover**: Move your mouse over any data point on the graph to reveal precise MB/s and IOPS metrics.

### 4. Preferences
Open **Preferences** from the menu to configure defaults applied to all new benchmarks:
- **Default Drive**: The volume pre-selected when the application starts.
- **Default Test / Block Size / Passes**: Starting values for the benchmark controls.
- **Average Method**: How pass results are combined:
  - *All Passes* — Simple mean.
  - *Trimmed Mean* — Excludes the fastest and slowest pass before averaging.
  - *Median* — Uses only the middle pass value from a sorted set.
- **CSV Path**: Location of the persistent history file.

The **currently active Average Method** is always visible on the Benchmark tab in the "Benchmark Control" group, next to the Passes count — no need to open Preferences to check.

### 5. Disk Information
The **Disk Info** tab provides deep hardware enumeration.
- **Navigation**: Use the left-side tree view. Drives are categorized logically into *Fixed Drives*, *USB Drives*, and *Optical Drives*.
- **Disk Details**: Click a root drive node to view the disk's full identity (vendor, product, revision), Bus Type, logical Geometry, Capacity, and whether an RDB is present.
- **Mounted Partitions**: Click a mounted partition to see Volume Name, Used/Free Space, total Size, and the DOS Type Identifier (e.g., `SFS/00 (0x53465300)`).
- **Unmounted Partitions**: Click an unmounted partition to see its DOS device name (e.g., `DH3`), size derived from RDB geometry, and filesystem type — no mount required.

### 6. Drive Health
The **Drive Health** tab communicates directly with S.M.A.R.T.-enabled drives.
- **Status Check**: Click **Refresh Health Data** to retrieve the latest vital statistics.
- **Interpretation**: The tool automatically interprets raw hexadecimal attributes (Reallocated Sectors, Power-On Hours, etc.) and provides a human-readable health assessment.

### 7. History and Exporting
- **History View**: Review all past benchmarks in a tabular format.
- **Comparison**: Select any two rows and click **Compare Selected** to generate a delta report showing percentage improvements or regressions.
- **Exporting**: Use **Export to CSV** to save the raw data for analysis in external spreadsheet software.

### 8. VALIDATE Mode
Run AmigaDiskBench with the `VALIDATE` argument (from Shell) or add `VALIDATE` as an icon tooltype to check all `.viz` profile files for syntax errors, unknown keys, invalid values, and missing required fields. The benchmark GUI does not open — only a validation report is shown.
- **Shell**: Outputs a formatted report with line numbers and severity to stdout.
- **Workbench**: Opens a standalone window with a scrollable list of findings.

## Building from Source

AmigaDiskBench is open source and can be cross-compiled using a Docker-based toolchain (GCC 11).

### Prerequisites
*   **WSL2** (Windows) or **Linux/macOS**.
*   **Docker** installed and running.
*   **Walkero's Amiga GCC 11 Image** (`walkero/amigagccondocker:os4-gcc11`).

### Build Commands
From the project root (WSL2):

```bash
cd /mnt/w/Code/amiga/antigravity/projects/AmigaDiskBench

# Clean previous build
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make clean

# Build
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make all
```

This will produce the `AmigaDiskBench` executable in the `build/` folder.

## Version History

### v2.5.2 (Current)
- **Pluggable Visualization Profiles**: Chart definitions are now loaded from `.viz` files in the `Visualizations/` folder. Nine built-in profiles ship with the application: Scaling, Trend, Battle, Workload, Hybrid, Peak Performance, IOPS Smoothed, Scaling Curve, and Filesystem.
- **Profile-driven rendering**: Each profile defines chart type (line/bar/hybrid), X/Y axes, series grouping, data filters, trend lines, reference line annotations, custom color palettes, and collapse aggregation — all without recompilation.
- **Trend lines**: Linear regression, moving average, and polynomial curve fitting, rendered per-series or globally. Coordinate clamping prevents drawing outside the chart area.
- **Collapse aggregation**: Reduce multiple runs at the same X value to a single data point using mean, median, min, or max.
- **Reference line annotations**: Horizontal dashed lines with labels at user-defined Y values (e.g., SATA III @ 600 MB/s).
- **Custom color palettes**: Up to 16 hex colors per profile.
- **Reload Profiles button**: Rescan the `Visualizations/` folder for new or modified profiles without restarting.
- **VALIDATE mode**: Shell argument or icon tooltype to validate all `.viz` files and report errors with line numbers and severity.
- **Quit confirmation dialog**: Attempting to close the application while a benchmark is running now shows a Yes/No confirmation prompt.
- **IOPS calculation corrected**: IOPS is now computed as total I/O operations divided by total elapsed time (true ops/second). Previously, workloads reported a fixed operation count of 1, and the engine averaged ops per pass rather than per second.
- **Bug fixes**:
  - Crash on exit while benchmarks are running (pending worker messages not drained before freeing reply port).
  - Blank window on Workbench launch (`ReadArgs` called unconditionally, failing when launched from icon).
  - Crash in `CollapseSeriesPoints` when series had no data points.
  - Chart lines drawn outside the graph area (missing coordinate clamping in line/hybrid renderers).
  - Duplicate X-axis labels when all data points share the same block size.
  - Color By dropdown removed — now a read-only label driven by the active profile's GroupBy setting.
  - Unused filter dropdowns removed from the visualization tab.

### v2.4.1
- **Session Log tab**: A new scrollable, timestamped log panel records all benchmark activity in real time — start, per-pass progress, results, and failures — with `[HH:MM:SS]` timestamps.
- **Live log updates**: Cross-process Exec message passing delivers log entries from the worker process to the GUI without polling or unsafe gadget access.
- **Log context menu**: Right-click for Select All and Copy.
- **Copy to Clipboard button**: Writes the entire log to the system clipboard via IFFParse (FORM FTXT/CHRS format).
- **Clear Log button**: Wipes the transcript and re-inserts the session header.
- **Bug fix**: Classic menu selections were silently swallowed when a context menu was also present. Fixed by checking `WINDOW_MenuType` before routing the `WMHI_MENUPICK` event.

### v2.3.7
- **S.M.A.R.T. column auto-fit**: The Attribute Name column in the Drive Health tab now correctly auto-expands to show full attribute names without truncation.
- **Average method display**: The Benchmark Control row now shows a single combined label (e.g. `Average: Median (Middle Value Only)`) next to the Passes count, removing the previous two-gadget layout that caused spacing issues.
- **Bulk tab Settings text**: The Queued Job Settings line now includes the averaging method name, e.g. `Settings: Sprinter / 10 Passes (Median) / 4K`.
- **Disk Info — improved naming**: The right-hand detail panel now uses precise terminology: *Disk Name* (SCSI vendor/product/revision), *Partition Name* (DOS device name, shown for unmounted partitions), and *Volume Name* (filesystem label, shown for mounted partitions). The section header is *Disk Details* for drives and *Partition Details* for partitions.
- **Disk Info — unmounted partition details**: Clicking an unmounted partition now shows its DOS device name, size (derived from RDB/DosEnvec geometry without mounting), and filesystem type. Previously showed a blank panel.
- **Disk Info — underscore sanitization**: Volume and partition names containing underscores (filesystem artifact) are displayed with spaces for readability.
- **Disk Info — DOS type display**: The 4th byte of a DOS type identifier is now shown as a printable character when it is ASCII (e.g. `SWAP`, `CD01`) rather than hex notation (e.g. `SWA/50`, `CD0/31`).
- **Bug fixes**: Disk Info drive categorization, Not Mounted partition sorting, S.M.A.R.T. tab crash on first open, DSI crash on Disk Info tab page switching.

### v2.3.4 – v2.3.6
- **Flexible Pass Averaging**: Choose how multi-pass results are combined via **Preferences**: All Passes (mean), Trimmed Mean (excludes best/worst), or Median (middle value — eliminates outliers).
- **Average Method on Benchmark tab**: The currently active averaging method is always visible next to the Passes count in the Benchmark Control group — no need to open Preferences to check.
- **Disk Info tree restructured**: Physical drive nodes show the device name only (e.g. `a1ide.device`). Multiple units of the same controller are merged into one node. Partition nodes show `VolumeName (Unit N)`.
- **Bug fixes**: Underscore in volume names rendering as keyboard shortcuts, Preferences chooser lifetime crash, integer gadget tag ordering.

### v2.2.16
- **Disk Information Center**: Added a brand new, highly detailed hierarchical view organizing all physical storage devices into Fixed, USB, and Optical categories.
- **True Hardware Scanning**: The engine now directly probes the system to map logical partitions precisely to their physical driver units.
- **Enhanced CD/DVD Detection**: Added robust safeguards to prevent system freezes when querying optical drives (skips VPD Page 0x80 for CD-ROMs).
- **Filesystem Display**: Standardized DOS type formatting to `ABC/XX` hex notation.
- **Bug fixes**: DSI exception on startup (IOExtTD buffer size), layout stuttering, crashes on deep UI selection.

### v2.2.14
- **Release Optimization**: Disabled internal debug logging for maximum performance.
- **Final Release Polish**: Versioning bumps, code comment cleanup, open-source preparation.

### v2.2.11
- **Architectural Foundation**: Established the multi-threaded benchmark engine and core CSV history persistence.
- **UX Foundation**: Solidified the ReAction-based windowing interface.

### v2.2.10
- **Advanced Graphing**: Overhauled the visualization engine; X-axis changed to Block Size; auto-refresh on tab switch.
- **Variable Workloads**: Upgraded Random I/O tests to support dynamic user-selected block sizes.
- **Visual Feedback**: Introduced Traffic Light status indicator and Fuel Gauge progress bar.
- **Bug fixes**: CSV export crashes (alignment/varargs), color allocation issues.
