# AmigaDiskBench

**AmigaDiskBench** is a modern, high-performance disk benchmarking utility specifically designed for **AmigaOS 4.1 Final Edition**. It provides a robust, ReAction-based GUI to measure, analyze, and visualize the performance of various storage devices, filesystems, and hardware configurations.

## Key Features

### 1. Benchmark Profiles
Choose from a variety of tailored test scenarios:
- **Sprinter**: Fast, small file I/O and metadata performance test.
- **Legacy**: Small-block (512B) sequential write to stress worst-case throughput.
- **Heavy Lifter**: Large file sequential throughput with varying chunk sizes.
- **Daily Grind**: A pseudo-random mix of operations simulating real-world OS usage.
- **Profiler**: Detailed filesystem metadata performance analysis.
- **Standard Tests**: Sequential Read/Write, Random 4K Read/Write, and Mixed 70/30.

Right-click the **Test Type** chooser and select **Describe Test...** for a detailed explanation of what the selected test measures, including file sizes, block sizes, operation counts, and real-world equivalents.

### 2. Flexible Pass Averaging
Choose how multi-pass results are combined, via **Preferences**:
- **All Passes**: Simple arithmetic mean across every pass.
- **Trimmed Mean**: Excludes the single best and worst pass, averages the rest (requires 3+ passes).
- **Median**: Sorts all passes and picks the single middle value — eliminates outliers without distorting the average.

### 3. Pluggable Visualization Profiles
Analyze your data with a powerful, profile-driven graphing engine. Chart definitions are loaded from `.viz` files in the `Visualizations/` folder — no recompilation needed to add or customize charts.

- **Nine Built-in Profiles**:
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
- **S.M.A.R.T. Analysis**: Three-tier query strategy for maximum hardware compatibility:
  1. **Direct ATA** via CMD_IDE (works on a1ide, sb600sata, sii3114 drivers).
  2. **ATA PASS-THROUGH** via HD_SCSICMD (SAT-compliant drivers).
  3. **External smartctl** fallback (bundled with AmigaOS 4.1 FE).
- **Health Indicators**: Real-time display of **Temperature**, **Power-On Hours**, and overall **Health Status**. Query method shown in status display.
- **Assessment**: Automatic interpretation of critical attributes (Reallocated Sectors, Spin Retry Count, etc.) with threshold comparison.
- **Threshold Data**: Reads both SMART attribute values and failure thresholds from the drive for accurate health assessment.

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

1.  Extract `AmigaDiskBench.lha` to a location of your choice (e.g., `Work:`). This creates an `AmigaDiskBench` drawer containing the executable, icons, readme, visualization profiles, and an AutoInstall script.
2.  **AutoInstall** (optional): Double-click or `Execute AutoInstall` from a Shell to copy the application and visualization profiles to `SYS:Utilities/`.
3.  The application requires at least one valid `.viz` file in the `Visualizations/` folder to start.
4.  Launch `AmigaDiskBench` from the icon.

## Visualization Profile Format

Each `.viz` file is an INI-style text file placed in the `Visualizations/` folder. Files are loaded alphabetically on startup. Lines starting with `#` are comments. All key names and enum values are case-insensitive. String values may be quoted (`"like this"`) or unquoted. Filters use case-insensitive substring matching.

### Quick Example

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

### Complete Section Reference

#### `[Profile]` (required)

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `Name` | Any string (required) | - | Display name in the profile chooser. Profile is skipped if missing. |
| `Description` | Any string | - | Tooltip / description text. |
| `ChartType` | `line`, `bar`, `hybrid` | `line` | Chart rendering mode. `hybrid` automatically enables the secondary Y-axis. |

#### `[XAxis]`

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `Source` | `block_size`, `timestamp`, `test_index` | `test_index` | What drives the X-axis. `block_size` sorts numerically; `timestamp` and `test_index` plot chronologically. |
| `Label` | Any string | `"X"` | X-axis title displayed below the chart. |

#### `[YAxis]`

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `Source` | `mb_per_sec`, `iops`, `min_mbps`, `max_mbps`, `duration_secs`, `total_bytes` | `mb_per_sec` | Which result field to plot on the Y-axis. |
| `Label` | Any string | `"MB/s"` | Y-axis title displayed on the left side. |
| `AutoScale` | `yes` / `no` | `yes` | When `no`, uses `Min` and `Max` for fixed Y range. |
| `Min` | Decimal number | `0` | Fixed Y-axis minimum (only when `AutoScale = no`). |
| `Max` | Decimal number | `0` | Fixed Y-axis maximum (only when `AutoScale = no`). |

#### `[Series]`

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `GroupBy` | `drive`, `test_type`, `block_size`, `filesystem`, `hardware`, `vendor`, `app_version`, `averaging_method` | `drive` | How data points are grouped into separate colored series. |
| `SortX` | `yes` / `no` | `yes` for `block_size`, `no` otherwise | Sort data points by X value within each series. |
| `MaxSeries` | Integer (0 = unlimited) | `0` | Cap the number of series shown. Extra series are silently dropped. |
| `Collapse` | `none`, `mean`, `median`, `min`, `max` | `none` | When multiple data points share the same X value, reduce them to a single point using the chosen method. |

#### `[Filters]`

Filters control which benchmark results are included in the chart. Each `Exclude*` / `Include*` key can appear multiple times to add values to the filter list. Matching is case-insensitive substring.

**Test type filters** (matched against canonical CSV names):

| Key | Description |
|-----|-------------|
| `ExcludeTest` | Exclude results matching this test type. |
| `IncludeTest` | Include only results matching these test types. |

Valid test type names: `Sprinter`, `HeavyLifter`, `Legacy`, `DailyGrind`, `Sequential`, `Random4K`, `Profiler`, `SequentialRead`, `Random4KRead`, `MixedRW70/30`

**Block size filters** (matched against display strings):

| Key | Description |
|-----|-------------|
| `ExcludeBlockSize` | Exclude results with this block size. |
| `IncludeBlockSize` | Include only results with these block sizes. |

Valid block size names: `4K`, `16K`, `32K`, `64K`, `256K`, `1M`, `Mixed`

**Other data filters:**

| Key | Matched against | Description |
|-----|-----------------|-------------|
| `ExcludeVolume` / `IncludeVolume` | Volume name (e.g., `System`, `DH0`) | Filter by partition. |
| `ExcludeFilesystem` / `IncludeFilesystem` | Filesystem type (e.g., `SFS/00`, `NGF/00`) | Filter by filesystem. |
| `ExcludeHardware` / `IncludeHardware` | Device name (e.g., `ahci.device`) | Filter by device driver. |
| `ExcludeVendor` / `IncludeVendor` | Drive vendor string | Filter by manufacturer. |
| `ExcludeProduct` / `IncludeProduct` | Drive product string | Filter by drive model. |
| `ExcludeAveraging` / `IncludeAveraging` | Averaging method | Filter by pass averaging. |
| `ExcludeVersion` / `MinVersion` | App version string | Filter by AmigaDiskBench version. |

Valid averaging method names: `AllPasses`, `TrimmedMean`, `Median`

**Numeric threshold filters:**

| Key | Type | Description |
|-----|------|-------------|
| `MinPasses` | Integer | Minimum number of passes a result must have. |
| `MinMBs` | Decimal | Minimum MB/s to include. |
| `MaxMBs` | Decimal | Maximum MB/s to include. |
| `MinDurationSecs` | Decimal | Minimum test duration in seconds. |
| `MaxDurationSecs` | Decimal | Maximum test duration in seconds. |
| `DefaultDateRange` | `today`, `week`, `month`, `year`, `all` | Initial date range filter when this profile is selected. Default: `all`. |

#### `[Overlay]`

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `SecondarySource` | Any value | - | Enables the secondary Y-axis (right side). Currently used by `hybrid` charts to overlay IOPS. |
| `SecondaryLabel` | Any string | `"IOPS"` | Label for the secondary (right) Y-axis. |

#### `[TrendLine]`

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `Style` | `none`, `linear`, `moving_average`, `polynomial` | `none` | Trend line algorithm. |
| `Window` | Integer | `3` | Window size for moving average (number of points on each side). |
| `Degree` | `2` or `3` | `2` | Polynomial degree (clamped to 2-3). Uses Gaussian elimination. |
| `PerSeries` | `yes` / `no` | `no` | Draw a separate trend line for each series, or one global trend. |

#### `[Annotations]`

| Key | Format | Description |
|-----|--------|-------------|
| `ReferenceLine` | `value, "Label"` | Draws a horizontal dashed line at the given Y value with a text label. Up to 8 reference lines per profile. |

Example: `ReferenceLine = 600, "SATA III Max"` draws a dashed line at 600 MB/s.

#### `[Colors]`

| Key | Format | Description |
|-----|--------|-------------|
| `Color` | `0xRRGGBB` | Hex color for series (in order). Up to 16 per profile. When omitted, the built-in 8-color palette is used. |

### Notes

- A profile **must** have a `[Profile]` section with a `Name` key to be loaded.
- Boolean values accept `yes`/`true`/`1` for true; anything else is false.
- The `Exclude*` / `Include*` filter modes are mutually exclusive per category. If you use `IncludeTest`, only those tests are shown. If you use `ExcludeTest`, everything except those tests is shown. Do not mix both for the same category.
- On-screen GUI filters (Volume, Test Type, Date Range, App Version) are applied on top of profile filters.
- Use the `VALIDATE` mode (Shell argument or icon tooltype) to check your `.viz` files for errors before launching.
- See the 9 included `.viz` files in `Visualizations/` for working examples covering all features.

## Comprehensive Guide

### 1. Running Benchmarks
The **Benchmark** tab is where performance testing happens.
- **Target Drive**: Select the volume or partition you wish to test. Note that depending on the filesystem, some tests require write access.
- **Test Type**:
  - *Standard Tests*: Choose Sequential Read/Write, Random 4K Read/Write, or a Mixed 70/30 (Read/Write) workload.
  - *Profiles*: Use preset profiles like "Sprinter" for quick I/O checks, "Heavy Lifter" for sustained throughput testing, or "Daily Grind" for everyday usage simulation.
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
- **Status Check**: Click **Refresh Health Data** to retrieve the latest vital statistics. The query method used (Direct ATA, ATA Pass-Through, or smartctl) is shown in the status display.
- **Interpretation**: The tool automatically interprets raw attributes (Reallocated Sectors, Power-On Hours, etc.) and provides a human-readable health assessment with threshold comparison.

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
# Clean rebuild and create distribution directory
make clean && make all

# Create LHA archive (requires Docker)
make dist-lha
```

`make all` compiles all sources, links the binary, and assembles the `dist/AmigaDiskBench/` distribution directory. `make dist-lha` runs `lha` inside Docker to produce `dist/AmigaDiskBench.lha`.

## Version History

### v2.7 (Current)
- **S.M.A.R.T. rewrite**: Three-tier query strategy for broad hardware support:
  1. **CMD_IDE/CMDIDE_DIRECTATA**: Direct ATA register passthrough (reverse-engineered from AmigaOS smartctl binary). Works on `a1ide.device`, `sb600sata.device`, `sii3112ide.device`, `sii3114ide.device`, and other drivers implementing the CMD_IDE interface.
  2. **HD_SCSICMD with ATA PASS-THROUGH**: SAT-compliant CDBs (16-byte with 12-byte fallback) for drivers supporting SCSI-ATA Translation.
  3. **External smartctl**: Falls back to the system `smartctl` command bundled with AmigaOS 4.1 Final Edition, parsing its text output.
- S.M.A.R.T. now reads **threshold data** (ATA Feature 0xD1) in addition to attribute values, enabling accurate failure threshold comparison.
- S.M.A.R.T. status display shows which query method succeeded for diagnostic transparency.
- **Fixed**: `SCSICmd` struct not properly reinitialized between 16-byte and 12-byte ATA PASS-THROUGH fallback attempts.
- **Fixed**: SCSI sense buffer now properly allocated for S.M.A.R.T. commands.
- Expanded S.M.A.R.T. attribute name table from 18 to 35 known attributes (adds SSD-specific attributes).

### v2.6
- **Version numbering**: Adopted Amiga-style major.minor versioning.
- **AutoInstall script**: Distribution now includes an AmigaDOS AutoInstall script for one-click installation to `SYS:Utilities/`.
- **Distribution packaging**: New `make dist` and `make dist-lha` targets produce a ready-to-ship LHA archive with all required files.
- **Code quality audit**: Comprehensive code review and bug fix pass across all 48 source files.
- **Critical bug fixes**:
  - Mounted partitions were never added to the drive scan results (memory leak and missing partition data in Disk Info).
  - DosList device name resolution was unconditionally overwritten with "Generic Disk", ignoring successfully resolved names.
  - UtilityBase closed while IUtility interface still in use (potential crash in date functions).
  - ReAction class pointers (TextEditor, Scroller, FuelGauge) not NULLed after cleanup, leaving dangling references.
  - `ChangeFilePosition` return value misused as boolean in Random 4K, Random 4K Read, and Mixed R/W workloads — seeks to offset 0 were incorrectly treated as failures.
  - Unsigned integer underflow in random workloads when block_size >= file_size.
  - IOPS comparison percentage wrong when result2 < result1 (unsigned subtraction wrap).
  - Visualization "Last Week" date filter broken across month boundaries.
  - Hover detection picked the last matching point instead of the closest one.
  - Variable shadowing in hybrid chart renderer caused wrong pen colors and leaked resources.
  - Polynomial curve fit silently truncated data beyond 200 points, leaving uninitialized output.
  - CSV report parser used undersized buffer (512 vs 2048 bytes) and unbounded sscanf patterns.
  - Test type fuzzy matching returned false positives (e.g. "Random" matched before "RandomRead").
  - History legacy field shifting used wrong sizeof, corrupting parsed data.
- **Other fixes**: NULL dereference guard in GetDuration, localtime NULL checks, free_bytes underflow guard, missing path separator in CSV export, uninitialized variable in health query, Daily Grind chunk size range mismatch with documentation.
- **Code cleanup**: Dead code removed, stale developer comments cleaned up, professional file headers and function documentation added across all modules.

### v2.5
- Pluggable visualization profiles loaded from `.viz` files (9 built-in profiles).
- Trend lines: linear regression, moving average, polynomial curve fitting.
- VALIDATE mode for checking `.viz` files from Shell or Workbench.
- Test description popup (right-click Test Type chooser).
- S.M.A.R.T. drive health monitoring tab.
- Quit confirmation dialog when benchmarks are running.
- IOPS calculation corrected to true ops/second.
- Numerous crash fixes (X1000 drive scanning, Workbench launch, exit during benchmarks).

### v2.4
- Session Log tab with timestamped, cross-process log entries.
- Copy to Clipboard and Clear Log buttons.
- Context menu fix for classic menu selections.

### v2.3
- Flexible pass averaging (All Passes, Trimmed Mean, Median) via Preferences.
- Disk Information Center with hierarchical hardware tree view.
- S.M.A.R.T. column auto-fit, improved drive/partition naming and categorization.
- Unmounted partition details from RDB geometry.

### v2.2
- Multi-threaded benchmark engine with CSV history persistence.
- ReAction-based GUI with Traffic Light and Fuel Gauge.
- Advanced graphing engine with block size X-axis.
- Random I/O tests with dynamic block sizes.
