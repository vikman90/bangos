# 17 - Build System, Tooling & Developer Workflows

This document provides a comprehensive developer guide to the BangOS build system, toolchain dependencies, Makefile targets, interactive QEMU simulation, kernel debugging with GDB, containerized Docker environments, and the MkDocs documentation workflow.

---

## 🛠️ 1. Prerequisites & Toolchain Setup

BangOS is built from source using freestanding GCC, NASM, GNU-EFI, and static `musl-gcc` for Ring 3 userland applications.

### 1.1 Native Toolchain Installation

#### Debian / Ubuntu / Linux Mint:
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc \
    nasm \
    gnu-efi \
    musl \
    musl-tools \
    qemu-system-x86 \
    ovmf \
    e2fsprogs \
    python3 \
    python3-venv \
    python3-pip \
    docker.io
```

#### macOS (via Homebrew):
```bash
brew install nasm x86_64-elf-gcc qemu python3 e2fsprogs
```

---

## 📋 2. Comprehensive Makefile Targets Reference

The root `Makefile` orchestrates the compilation of kernel modules, assembly trampolines, userland binaries, filesystem disk images, UEFI staging directories, and automated test runners.

| Target | Command | Description |
| :--- | :--- | :--- |
| **`all`** *(default)* | `make all` | Compiles userland binaries, builds `initrd.tar`, generates `disk.img` (ext2), compiles the EFI kernel (`BOOTX64.EFI`), and populates the FAT32 ESP directory. |
| **`esp`** | `make esp` | Compiles userland executables and packages the EFI bootloader binary at `build/esp/EFI/BOOT/BOOTX64.EFI` alongside `build/esp/initrd.tar`. |
| **`userland`** | `make userland` | Builds all Ring 3 C applications in `userland/src/` statically using `musl-gcc` and bundles them into the USTAR `initrd.tar` ramdisk archive. |
| **`dist`** | `make dist` | Compiles all targets and packages compressed release archives (`bangos-<version>-x86_64.zip` and `.tar.gz`) containing the ESP partition and `disk.img`. |
| **`run-qemu`** | `make run-qemu` | Launches QEMU interactively in headless serial mode with OVMF UEFI firmware, the ext2 secondary disk (`disk.img`), and VirtIO-Net networking. |
| **`test`** | `make test` | Executes the automated end-to-end Python test runner (`scripts/test_runner.py`) asserting 100% test pass rates across Ring 0 and Ring 3 suites. |
| **`clean`** | `make clean` | Wipes the `build/` directory and removes all intermediate object files and staging trees. |
| **`docker-image`** | `make docker-image` | Builds the isolated `bangos-builder` Docker container image (`Dockerfile`) for cross-platform reproducible builds. |
| **`docker-build`** | `make docker-build` | Executes a clean build (`make clean all`) inside the Docker container. |
| **`docker-test`** | `make docker-test` | Runs the automated test suite inside the Docker container. |
| **`docker-shell`** | `make docker-shell` | Spawns an interactive `bash` shell inside the Docker build container. |
| **`docs-serve`** | `make docs-serve` | Starts a local live-reload documentation server on `http://0.0.0.0:8000` using Material for MkDocs. |
| **`docs-build`** | `make docs-build` | Validates and compiles the documentation into static HTML (`site/`) with strict validation enabled (`--strict`). |

---

## 🖥️ 3. Running & Interacting with QEMU

BangOS executes as a 64-bit UEFI application on QEMU x86_64.

### 3.1 Interactive Mode (`make run-qemu`)

Execute the following command to boot BangOS into the interactive serial TUI:

```bash
make run-qemu
```

#### Under the Hood:
```bash
qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:build/esp,format=raw \
    -drive file=build/disk.img,format=raw,index=1,media=disk \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    -serial stdio \
    -nographic \
    -monitor none
```

- **`-m 512M`**: Allocates 512 MB of physical RAM.
- **`-bios OVMF.fd`**: Loads Open Virtual Machine Firmware for x86_64 UEFI boot.
- **`-drive file=fat:rw:build/esp`**: Mounts the FAT32 EFI System Partition (ESP) containing `BOOTX64.EFI` and `initrd.tar`.
- **`-drive file=build/disk.img`**: Mounts the 32MB ext2 secondary hard disk on the primary ATA channel (`/dev/ata0` / `/mnt/ext2`).
- **`-device virtio-net-pci`**: Attaches an OASIS VirtIO PCI network adapter mapped to QEMU's SLIRP user-mode network.
- **`-serial stdio -nographic`**: Routes serial port COM1 (`0x3F8`) directly to your host terminal.

### 3.2 TUI Navigation & Keybindings

Once booted, `/bin/init` displays the interactive main menu:

```text
======================================================================
        BangOS (x86_64) - Bare Metal Kernel v0.3.0        
     PID: 1 (init) | RAM: 128 MB Total (126 MB Free) | Uptime: 0 s
======================================================================

  [1] Geometric Calculator           (execve /bin/calc)
  [2] System Information             (execve /bin/sysinfo)
  [3] Subsystem Benchmarks           (execve /bin/bench)
  [4] Interactive Task Manager       (execve /bin/tasks)
  [5] Multithreading & Synchronization (execve /bin/threads)
  [6] Specification Test Suites      (Ring 0 & Ring 3 Tests)
  [7] Disk Storage & ext2 Tool       (execve /bin/disktool)
  [8] Network Fetch & HTTP Client    (execve /bin/netfetch)
  [9] Shutdown / Halt System         (exit)

Select an option [1-9]:
```

- Press keys **`1`** through **`8`** to execute userland programs.
- Press **`9`** to perform a clean kernel shutdown and power off QEMU.
- To forcefully terminate QEMU at any time from your host terminal, press **`Ctrl + A`** followed by **`X`**.

---

## 🐞 4. Kernel Debugging with GDB

You can attach the GNU Debugger (`gdb`) to inspect kernel state, single-step assembly instructions, inspect page tables, and set breakpoints.

### Step 1: Launch QEMU in Waiting Debug Mode
Add the `-s -S` flags to QEMU (or run directly):

```bash
qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:build/esp,format=raw \
    -drive file=build/disk.img,format=raw,index=1,media=disk \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    -serial stdio \
    -nographic \
    -s -S
```

- **`-s`**: Shorthand for `-gdb tcp::1234` (listens on GDB TCP port 1234).
- **`-S`**: Freezes CPU execution at physical reset vector until GDB issues `continue`.

### Step 2: Connect GDB
In a separate terminal window:

```bash
gdb -ex "target remote localhost:1234" -ex "symbol-file build/bangos.so"
```

Useful GDB commands:
```gdb
(gdb) break kernel_main
(gdb) continue
(gdb) info registers
(gdb) x/10i $rip
(gdb) print *current_process
(gdb) next
(gdb) stepi
```

---

## 🐳 5. Containerized Docker Workflows

To ensure reproducible builds regardless of the host OS, a multi-stage Docker environment is provided:

### 1. Build Docker Image
```bash
make docker-image
```

### 2. Compile OS inside Docker
```bash
make docker-build
```

### 3. Run Automated Tests inside Docker
```bash
make docker-test
```

### 4. Interactive Development Shell
```bash
make docker-shell
```

---

## 📖 6. Documentation Workflow (MkDocs Material)

The BangOS documentation is built using [MkDocs](https://www.mkdocs.org/) with the [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) theme.

### 6.1 Local Setup

Create a dedicated Python virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install mkdocs mkdocs-material
```

### 6.2 Live-Reloading Development Server

Start the local server with hot reloading:

```bash
make docs-serve
```

Open **`http://localhost:8000`** in your browser. Any edits made to `mkdocs.yml` or files in `docs/` are reflected immediately.

### 6.3 Strict Build Validation

To verify all internal markdown links, mermaid diagrams, and configuration syntax:

```bash
make docs-build
```

### 6.4 Continuous Deployment (GitHub Pages)

The documentation is automatically built and deployed to GitHub Pages on every push to the `main` branch via the `.github/workflows/deploy-docs.yml` GitHub Actions workflow.

### 6.5 Automated Release Assets (GitHub Releases)

When a release or pre-release is published on GitHub, the `.github/workflows/release.yml` workflow automatically:
1. Validates the codebase by running the full automated QEMU test suite (`make test`).
2. Packages the EFI bootloader partition and storage disk image into distribution archives (`make dist`).
3. Attaches the release bundles (`.zip`, `.tar.gz`) and standalone assets (`BOOTX64.EFI`, `initrd.tar`, `disk.img`) directly to the GitHub Release.

---

## 🔄 7. Git & Contribution Conventions

When contributing to BangOS, adhere strictly to repository policies defined in `AGENTS.md`:

1. **Branching**: Never commit directly to `main`. Create an isolated topic branch (`feat/<name>`, `fix/<name>`, `docs/<name>`, `test/<name>`).
2. **Conventional Commits**: Format commit messages using standard types (`feat`, `fix`, `docs`, `test`, `build`, `ci`, `refactor`, `chore`) and scopes (`kernel`, `boot`, `syscall`, `mm`, `fs`, `drivers`, `userland`, `mkdocs`).
3. **AI Trailer**: Include the `Assisted-by:` trailer in commit footers:
   ```text
   Assisted-by: AI Assistant <assistant@antigravity.ai>
   ```
4. **Verification**: Always ensure `make clean all`, `make test`, and `make docs-build` execute with zero warnings or failures prior to submitting pull requests.
