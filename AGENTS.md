# AI Agent Guidelines and Repository Standards

This document establishes the technical constraints, software architecture specifications, repository conventions, and verification policies for AI assistants contributing to BangOS.

---

## 1. Language Policy

* All source code, assembly files, inline comments, documentation files (`README.md`, `docs/*.md`), commit messages, pull request descriptions, test cases, and log output must be written exclusively in English.
* Natural language interactions with repository maintainers may occur in other languages, but all repository assets must remain 100% in English.

---

## 2. Source Control and Commit Conventions

### 2.1 Branching Strategy
Direct commits to the `main` branch are prohibited. Every task or feature modification must be executed in an isolated topic branch:

* `feat/<topic>`: Functional additions (e.g., new system calls, drivers, userland tools).
* `fix/<topic>`: Bug fixes and memory safety corrections.
* `refactor/<topic>`: Code structural refactoring without behavioral changes.
* `docs/<topic>`: Documentation updates.
* `test/<topic>`: Test runner modifications and test suite expansions.

### 2.2 Commit Message Format
Commit messages must adhere strictly to the Conventional Commits specification:

```text
<type>(<scope>): <imperative summary>

[optional body explaining technical rationale]

Assisted-by: AI Assistant <assistant@antigravity.ai>
```

#### Supported Types:
`feat`, `fix`, `docs`, `test`, `build`, `ci`, `refactor`, `chore`.

#### Standard Scopes:
`kernel`, `boot`, `syscall`, `process`, `mm`, `fs`, `drivers`, `userland`.

### 2.3 Required Commit Trailers
AI-generated or AI-assisted commits must include the `Assisted-by:` trailer in the commit footer instead of `Co-Authored-By:`.

```text
Assisted-by: AI Assistant <assistant@antigravity.ai>
```

### 2.4 Commit Atomicity
Commits must be granular and thematic. Kernel changes, userland tools, test harness updates, and documentation modifications must be staged and committed as distinct logical steps.

---

## 3. Architecture and Technical Constraints

### 3.1 Kernel Execution Environment (Ring 0)
* **Freestanding Compilation**: Kernel modules and UEFI bootloader code run on bare-metal x86_64 without an underlying host operating system.
* **Standard Library Exclusion**: Header includes from the host C standard library (`<stdio.h>`, `<stdlib.h>`, `<string.h>`) are prohibited in kernel space.
* **Kernel Abstractions**: Console output (`kprintf`), string manipulation (`kernel/lib/kstring.h`), and memory management must rely solely on kernel-provided interfaces in `include/` and `kernel/`.
* **Compiler Directives**: Kernel compilation units require `-mno-red-zone`, `-fno-stack-protector`, and `-fpic`.

### 3.2 Userland Execution Environment (Ring 3)
* **Static musl Runtime**: Userland programs in `userland/src/` are compiled statically using `musl-gcc`.
* **Standard Headers**: Userland C programs may include standard C library headers.
* **Syscall Interface**: Userland processes depend on system calls dispatched by `kernel/syscall/syscall.c`. Unimplemented system call numbers must evaluate to `-ENOSYS` (-38).
* **Ramdisk Packaging**: Userland executables are packaged into a USTAR archive (`initrd.tar`) loaded into memory at boot time.
* **Process Lifecycle**: The root process `/bin/init` (PID 1) executes child ELF binaries via `fork()` + `execve()` and collects exit statuses via `waitpid()`.

### 3.3 System Call ABI Specifications
* **Hardware Mechanism**: Execution transitions via x86_64 native `syscall` / `sysret` MSR configuration (`STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`).
* **Register Passing Conventions**:
  * Syscall identifier: `rax`
  * Arguments: `rdi` (arg1), `rsi` (arg2), `rdx` (arg3), `r10` (arg4), `r8` (arg5), `r9` (arg6)
  * Return code: `rax` (negative integer on error, e.g., `-EFAULT`, `-EINVAL`, `-ENOSYS`)
* **Pointer Validation**: User-provided virtual memory addresses and buffer lengths must be validated for range safety prior to dereferencing within Ring 0.

### 3.4 Paging and Memory Architecture
* **Paging Hierarchy**: Standard 4-level x86_64 paging (PML4 -> PDPT -> PD -> PT).
* **Page Alignment**: Memory allocation units and page table entries must align on 4096-byte boundaries.
* **Access Control Flags**: User-accessible pages require explicit `PAGE_PRESENT | PAGE_USER | PAGE_WRITE` bit configurations.
* **TLB Maintenance**: Modifications to active page tables require TLB invalidation via `invlpg` or `%cr3` reloads.

---

## 4. Build and Packaging Pipeline

* **Full Build Sequence**: Executing `make all` or `make esp` compiles userland binaries, builds `initrd.tar`, compiles the EFI kernel binary, and populates the FAT32 boot filesystem at `build/esp/`.
* **Clean Build**: Executing `make clean` clears all intermediate build object files and staging directories across kernel and userland environments.
* **Adding Userland Applications**:
  1. Add source module to `userland/src/<app_name>.c`.
  2. Define binary compilation target in `userland/Makefile`.
  3. Include the executable target in the `TARGETS` list and `initrd.tar` archive rule inside `userland/Makefile`.
  4. Register invocation logic in `userland/src/init.c`.
* **Containerized Build Environment**:
  * `make docker-build`: Compiles the operating system inside an isolated container environment.
  * `make docker-test`: Executes automated test runners in Docker.

---

## 5. Automated Testing and QEMU Harness

* **Test Execution Command**: Run `make test` or `make docker-test`.
* **Harness Execution Model**:
  * `scripts/run_qemu_test.sh` delegates to `scripts/test_runner.py`.
  * `scripts/test_runner.py` spawns `qemu-system-x86_64` headlessly (`-nographic`) with serial output routed to a TCP socket (port 4444).
  * The Python harness executes a deterministic state machine, sending console inputs and asserting expected serial stdout stream strings.
* **State Machine Synchronization Requirement**:
  * Any modification to `/bin/init` prompt output, menu indices, or standalone tool terminal output requires updating state transitions and regex expectations in `scripts/test_runner.py`.
  * Inconsistencies between userland output and harness state machines will cause test timeouts.
* **Verification Criterion**: 100% of assertion checks in `scripts/test_runner.py` must evaluate to PASS with zero process failures.

---

## 6. Documentation Synchronization Requirements

Subsystem modifications require corresponding updates to project documentation:

| Subsystem Change | Target Documentation Files |
| :--- | :--- |
| System Call Addition / Modification | `docs/04-syscall-abi.md`, `README.md`, `include/kernel.h`, `kernel/syscall/syscall.h` |
| Hardware Driver Addition | `docs/00-architecture.md`, `docs/07-extending-bangos.md`, `README.md` |
| Userland Application Addition | `docs/07-extending-bangos.md`, `README.md`, `userland/Makefile` |
| Memory Management / Paging | `docs/03-paging-and-memory.md` |
| Bootloader / UEFI Firmware | `docs/01-uefi-bootloading.md` |
| Test Harness / CI Pipeline | `docs/06-testing-and-qemu.md`, `.github/workflows/ci.yml` |

---

## 7. Compliance and Verification Criteria

Prior to concluding any task or submitting changes for merge:

1. Confirm work is performed on a dedicated topic branch (`feat/`, `fix/`, etc.).
2. Ensure all newly added code, comments, documentation, and commit messages are in English.
3. Verify `make clean && make all` builds without compiler warnings (`-Wall -Wextra`).
4. Ensure `make test` executes to completion with 0 assertion failures.
5. Update relevant guides in `docs/` and `README.md`.
6. Format commits according to Conventional Commits guidelines and append the `Assisted-by:` trailer.