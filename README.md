# AmigaDiskBench

**AmigaDiskBench** is a modern, high-performance disk benchmarking utility specifically designed for **AmigaOS 4.1 Final Edition**. It provides a robust, ReAction-based GUI to measure and analyze the performance of various storage devices, filesystems, and hardware configurations.

![AmigaDiskBench Icon](AmigaDiskBench.info)

## Key Features

- **Comprehensive Benchmark Profiles**:
  - **Sprinter**: Small file I/O and metadata performance.
  - **Heavy Lifter**: Large file sequential throughput with big chunks.
  - **Legacy**: Large file performance with standard 512-byte blocks.
  - **Daily Grind**: A pseudo-random mix of operations representing real-world usage.
  - **Sequential Read/Write**: Professional-grade pure sequential throughput tests.
  - **Random 4K Read/Write**: High-stress IOPS testing for SSDs and fast media.
  - **Mixed 70/30**: Real-world read/write mix simulation.
  - **Profiler**: Filesystem metadata performance analysis.

- **Precision & Reliability**:
  - **Microsecond Precision**: Uses `timer.device` for high-accuracy timing.
  - **Trimmed Mean**: Optional outlier filtering to remove OS background noise from results.
  - **Cache Flushing**: Automatic buffer flushing attempts to ensure measuring disk speed, not RAM speed.

- **Data Management**:
  - **Current Session vs. History**: Separate tabs for active benchmarking and historical analysis.
  - **CSV Persistence**: Results are automatically saved to a standardized CSV format (`AmigaDiskBench_History.csv`).
  - **Smart Sync**: Changing the CSV path automatically keeps your session context clean.

- **Advanced Analysis & Visualization (New in v2.2.13.1022)**:
  - **Multi-Type Visualizations**: Choose between **Scaling Profiles**, **Trend Lines**, **Battle Bar Charts**, and **Hybrid Diagnostic Views**.
  - **Flexible Grouping**: Group and color data by **Drive**, **Test Type**, or **Block Size** to analyze complex datasets in a single view.
  - **16-Color Palette**: Professional high-contrast palette for clear differentiation of multiple data series.
  - **Smart Legends**: Automatically wrapping legend labels to ensure fit within the window.
  - **Hover Interaction**: Real-time display of detailed point metadata on mouse-over.
  - **Side-by-Side Comparison**: Compare any two results with a dedicated diff-view engine.
  - **Global Aggregate Reports**: Statistical summaries of all benchmarking activity.

## Installation

No special installation is required. Just extract the archive to a location of your choice.
Requires **AmigaOS 4.1 Final Edition** or newer.

## How to Use

### Running a Benchmark
1.  **Select a Target**: Use the "Benchmark Control" tab to select a volume or partition.
2.  **Configure Tests**: Choose a Test Type, Block Size, and number of Passes (3-5 recommended).
3.  **Run**: Click the "Run Benchmark" button.
    *   *Tip*: Using the "Bulk" tab allows you to queue multiple drives and test types for automated testing.

### Visualization & Analysis
Switch to the **Visualization** tab to perform deep-dives into your benchmark data:
1.  **Select Chart Type**: 
    - *Scaling Line*: Best for seeing how performance changes with block size.
    - *Trend Line*: Best for tracking drive stability over long sessions.
    - *Battle Bars*: Best for comparing multiple physcial drives head-to-head.
    - *Hybrid*: Professional view overlaying **Throughput (MB/s)** and **IOPS** for a comprehensive performance profile.
2.  **Filter Data**: Narrow down results by Volume, Test Type, or Date Range (Today/Month/Year).
3.  **Color By**: Change the grouping logic to visualize comparisons between different hardware or access patterns.
4.  **Analyze**: Hover over any data point to see the exact value and metadata in the status bar.

### History & Data Maintenance
*   **Persistent Storage**: All results are saved to CSV. 
*   **Comparison**: Select any two results in the ListBrowser and click "Compare" for a detailed delta report.
*   **Cleanup**: Specific results can be deleted individually, or the entire database cleared via the History menu.

## Building from Source

AmigaDiskBench is cross-compiled using a Docker-based toolchain (GCC 11).

### Prerequisites
*   **WSL2** (Windows users) or **Linux/macOS**
*   **Docker** (Ensure the daemon is running)

### Build Command
From the project root:

```bash
# Preferred method (works with or without Docker Desktop)
wsl build_wsl.md (See .agent/workflows/build_wsl.md for sequence)
```

Direct Docker command:
```bash
docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make all
```

## Licensing

AmigaDiskBench is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

---
**Copyright (C) 2026 Team Derfs**
