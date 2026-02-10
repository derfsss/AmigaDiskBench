# AmigaDiskBench

**AmigaDiskBench** is a modern, high-performance disk benchmarking utility specifically designed for **AmigaOS 4.1 Final Edition**. It provides a robust, ReAction-based GUI to measure and analyze the performance of various storage devices, filesystems, and hardware configurations.

![AmigaDiskBench Icon](AmigaDiskBench.info)

## Key Features

- **Multiple Benchmark Profiles**:
  - **Sprinter**: Small files and metadata performance.
  - **Heavy Lifter**: Large file sequential throughput with big chunks.
  - **Legacy**: Large file performance with standard 512-byte blocks.
  - **Daily Grind**: A pseudo-random mix of operations representing real-world usage.
- **Microsecond Precision**: Uses `timer.device` for high-accuracy timing.
- **Advanced Metrics**: Reports MB/s, IOPS, and supports Trimmed Mean calculations to filter out background OS interference.
- **Hardware & FS Identification**: Automatically detects hardware device names, units, and filesystem types (NGFS, SFS, FFS, etc.).
- **Persistence**: Saves results to a standardized CSV format for historical tracking.
- **Global Reporting**: Generates aggregate statistics across all past benchmarks.

## How to Use

1. **Select a Target**: Use the "Benchmark Control" tab to select a volume or partition.
2. **Configure Tests**: Choose the test type, block size, and number of passes.
3. **Run**: Click the "Run Benchmark" button. The status light will indicate activity.
4. **Analyze**:
   - View live updates in the "Benchmark" tab.
   - Access persistent records in the "History" tab.
   - Right-click any result for a "Show Details" report.
5. **Preferences**: Customize your defaults, including the CSV storage path and trimmed mean settings.

## How to Compile

AmigaDiskBench is cross-compiled using a modern Docker-based toolchain.

### Prerequisites
- **WSL2** (on Windows) or **Linux/macOS**.
- **Docker** installed and running.
- **AmigaOS 4 Cross-Compiler Container**:
  - Link: [walkero/amigagccondocker](https://hub.docker.com/r/walkero/amigagccondocker)
  - Pull command: `docker pull walkero/amigagccondocker:os4-gcc11` (for GCC v11)

### Build Instructions

The project uses the `walkero/amigagccondocker:os4-gcc11` container which includes the AmigaOS 4 SDK.

Run the following command from the project root:

```bash
docker run --rm -v $(pwd):/work -w /work walkero/amigagccondocker:os4-gcc11 make clean all
```

This will:
1. Clean previous build artifacts.
2. Compile all modules (GUI, Engine, System).
3. Link the final `AmigaDiskBench` binary into the `build/` directory.

### Installation

To copy the binary and icons to a `dist/` folder:

```bash
docker run --rm -v $(pwd):/work -w /work walkero/amigagccondocker:os4-gcc11 make install
```

## Licensing

AmigaDiskBench is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License as published by the Free Software Foundation, either version 3 of the License**, or (at your option) any later version.

See the [LICENSE](LICENSE) file for the full text.

---
**Copyright (C) 2026 Team Derfs**
