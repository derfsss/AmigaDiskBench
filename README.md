# AmigaDiskBench

**AmigaDiskBench** is a modern, high-performance disk benchmarking utility specifically designed for **AmigaOS 4.1 Final Edition**. It provides a robust, ReAction-based GUI to measure, analyze, and visualize the performance of various storage devices, filesystems, and hardware configurations.

![AmigaDiskBench Icon](AmigaDiskBench.info)

## Key Features

### 1. Benchmark Profiles
Choose from a variety of tailored test scenarios:
- **Sprinter**: Fast, small file I/O and metadata performance test.
- **Marathon**: Sustained long-duration test to check for thermal throttling or buffer issues.
- **Heavy Lifter**: Large file sequential throughput with varying chunk sizes.
- **Daily Grind**: A pseudo-random mix of operations simulating real-world OS usage.
- **Profiler**: Detailed filesystem metadata performance analysis.
- **Standard Tests**: Sequential Read/Write, Random 4K Read/Write, and Mixed 70/30.

### 2. Advanced Visualization (Updated v2.2.14)
Analyze your data with a powerful, interactive graphing engine:
- **Chart Types**:
  - **Scaling**: Visualize performance vs. Block Size.
  - **Trend**: Track performance stability over time.
  - **Workload**: Compare different Test Types (Read vs Write, Seq vs Rand).
  - **Hybrid**: Professional diagnostic view overlaying **Throughput (MB/s)** bars with **IOPS** lines to identify bottlenecks.
  - **Battle**: Head-to-head comparison of multiple drives.
- **Filtering**: Drill down into data by **Volume**, **Test Type**, **Date Range**, and **App Version**.
- **Grouping**: Color-code results by **Drive**, **Test Type**, or **Block Size**.
- **Interactive**: Hover over data points for precise values.

### 3. Drive Health Monitoring (New!)
Keep an eye on your hardware's physical status:
- **S.M.A.R.T. Analysis**: Reads raw attribute data directly from the drive.
- **Health Indicators**: Real-time display of **Temperature**, **Power-On Hours**, and overall **Health Status**.
- **Assessment**: Automatic interpretation of critical attributes (Reallocated Sectors, Spin Retry Count, etc.).

### 4. Bulk Testing
Automate your benchmarking workflow:
- **Queue Jobs**: Select multiple drives and add them to a batch queue.
- **Automation**: Options to "Run All Test Types" and "Run All Block Sizes" (4K to 1M) automatically.
- **Progress Tracking**: dedicated "Fuel Gauge" to track overall batch progress.

### 5. History & Data Management
- **Persistent Storage**: All results are automatically saved to `AmigaDiskBench_History.csv`.
- **Comparison**: Select any two results to generate a delta report (Speedup/Slowdown %).
- **Export**: Export specific datasets to CSV for external analysis (Excel, Sheets).
- **Reports**: Generate global summary reports of all text activities.

## Requirements

*   **AmigaOS 4.1 Final Edition** (or newer).
*   Reasonably fast storage device for meaningful results (SSD/NVMe recommended).

## Installation

No special installation is required.
1.  Extract the archive to a location of your choice (e.g., `Work:Utilities/AmigaDiskBench`).
2.  Launch `AmigaDiskBench` from the icon.

## How to Use

### Basic Benchmarking
1.  Go to the **Benchmark** tab.
2.  Select a **Target Drive** and **Test Type**.
3.  Set the **Block Size** and **Passes** (default is 3).
4.  Click **Run Benchmark**.

### Using Visualization
1.  Switch to the **Visualization** tab.
2.  Use the top filters to isolate the data you want (e.g., "System", "Random 4K Read").
3.  Select a **Chart Type** (e.g., "Hybrid").
4.  Select a **Color By** mode (e.g., "Test Type" to see Read vs Write colors).
5.  Hover over the graph to see exact MB/s and IOPS values.

### Checking Drive Health
1.  Switch to the **Drive Health** tab.
2.  Select a drive from the dropdown.
3.  Click **Refresh Health Data**.
4.  Review the S.M.A.R.T. attributes list and the status summary at the top.

## Building from Source

AmigaDiskBench is open source and can be cross-compiled using a Docker-based toolchain (GCC 11).

### Prerequisites
*   **WSL2** (Windows) or **Linux/macOS**.
*   **Docker** installed and running.
*   **Walkero's Amiga GCC 11 Image** (`walkero/amigagccondocker:os4-gcc11`).

### Build Command
From the project root:

```bash
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make all
```

This will produce the `AmigaDiskBench` executable in the `dist` folder.

## Authors

*   **Team Derfs** - *Core Development*
*   **Rich** - *Lead Developer*

## License

This project is licensed under the MIT License - see the LICENSE file for details.
