---
description: Build the AmigaDiskBench project using WSL2 and Docker (Non-Docker Desktop version)
---

This workflow is used when Docker Desktop is not available on Windows, and instead uses Docker directly within WSL2.

// turbo-all
1. Build and Install:
```bash
wsl sh -c 'cd /mnt/w/Code/amiga/antigravity/projects/AmigaDiskBench && docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make clean && docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make && docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make install'
```

2. Clean build only:
```bash
wsl sh -c 'cd /mnt/w/Code/amiga/antigravity/projects/AmigaDiskBench && docker run --rm -v $(pwd):/src -w /src walkero/amigagccondocker:os4-gcc11 make clean'
```
