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

### 3. Advanced Visualization
Analyze your data with a powerful, interactive graphing engine:
- **Chart Types**:
  - **Scaling**: Visualize performance vs. Block Size.
  - **Trend**: Track performance stability over time.
  - **Workload**: Compare different Test Types (Read vs Write, Seq vs Rand).
  - **Hybrid**: Professional diagnostic view overlaying **Throughput (MB/s)** bars with **IOPS** lines to identify bottlenecks.
  - **Battle**: Head-to-head comparison of multiple drives.
- **Filtering**: Drill down into data by **Volume**, **Test Type**, **Date Range**, and **App Version**.
- **Grouping**: Color-code results by **Drive**, **Test Type**, or **Block Size** (up to 16 distinct series).
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

### 7. Detailed Disk Information
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
2.  Launch `AmigaDiskBench` from the icon.

## Comprehensive Guide

### 1. Running Benchmarks
The **Benchmark** tab is where performance testing happens.
- **Target Drive**: Select the volume or partition you wish to test. Note that depending on the filesystem, some tests require write access.
- **Test Type**:
  - *Standard Tests*: Choose Sequential Read/Write, Random 4K Read/Write, or a Mixed 70/30 (Read/Write) workload.
  - *Profiles*: Use preset profiles like "Sprinter" for quick I/O checks, "Marathon" for sustained thermal testing, or "Daily Grind" for everyday usage simulation.
- **Parameters**: Adjust the **Block Size** (e.g., 4K, 32K, 1M) and the number of **Passes**. Higher passes yield more reliable averages.
- **Execution**: Click **Run Benchmark**. Monitor the **Traffic Light** (green/yellow/red) for current status, and the **Fuel Gauge** for overall progress.

### 2. Bulk Testing / Queue
For extensive testing sessions, use the Batch Queue.
- Click **Add to Queue** to stockpile tests.
- Alternatively, use **Add All Tests** or **Add All Block Sizes** to quickly generate a matrix of workloads for the selected drive.
- Click **Start Queue** to let AmigaDiskBench run them sequentially.

### 3. Visualizing Results
The **Visualization** tab brings your data to life.
- **Filters**: Located at the top. Use them to narrow down results by Volume, Test Type, or Date.
- **Chart Modes**:
  - *Hybrid*: Shows throughput (MB/s) as bars and IOPS as an overlaid line graph. Ideal for performance tuning.
  - *Scaling*: Demonstrates how block size impacts performance.
  - *Trend*: Shows historical performance decay or improvement over time.
  - *Battle*: Select datasets to compare different drives head-to-head.
- **Color Coding**: Group data series by Drive, Test Type, or Block Size for immediate visual clarity.
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

### v2.3.7 (Current)
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
