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

- **Analysis Tools**:
  - **Comparison Mode**: Compare any two results side-by-side (Session or History).
  - **Global Report**: Aggregate statistics (Avg/Max) across all run tests.
  - **Visualizations**: Interactive trend graphs to track performance over time.
  - **Hardware ID**: Automatic detection of device names (e.g., `ahci.device`), units, and filesystem types.

## Installation

No special installation is required. Just extract the archive to a location of your choice.
Requires **AmigaOS 4.1 Final Edition** or newer.

## How to Use

### Running a Benchmark
1.  **Select a Target**: Use the "Benchmark Control" tab to select a volume or partition.
2.  **Configure Tests**: Choose a Test Type, Block Size, and number of Passes (3-5 recommended).
3.  **Run**: Click the "Run Benchmark" button.
    *   *Tip*: Toggle the "Bulk" tab to run multiple tests in sequence.

### Analyzing Results
*   **Benchmark Tab**: Displays results from your *current session*.
*   **History Tab**: Displays all historical results saved to your CSV file.
    *   **Compare**: Check the boxes next to any two results and click "Compare Selected".
    *   **Details**: Double-click any result to see detailed hardware info and raw metrics.
    *   **Delete/Clear**: Remove specific entries or wipe the entire history.
*   **Visualization Tab**: View performance trends filtered by Volume and Test Type.

### Preferences
Access the Preferences menu to:
*   Set default test parameters (Passes, Block Size).
*   Toggle "Trimmed Mean" calculation.
*   Change the location of the history CSV file.

## Building from Source

AmigaDiskBench is cross-compiled using a Docker-based toolchain (GCC 11).

### Prerequisites
*   **Docker Desktop** (Windows/Mac/Linux)
*   **WSL2** (Windows users)

### Build Command
From the project root:

```bash
docker run --rm -v $(pwd):/work -w /work walkero/amigagccondocker:os4-gcc11 make clean all
```

This will compile the `AmigaDiskBench` binary into the `build/` directory.

## Licensing

AmigaDiskBench is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

See the [LICENSE](LICENSE) file for the full text.

---
**Copyright (C) 2026 Team Derfs**
