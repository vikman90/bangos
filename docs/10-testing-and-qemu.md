# 10 - Specification-Driven Testing & QEMU Automation

BangOS features a comprehensive, two-tier testing architecture validating both **Ring 0 kernel subsystems** and **Ring 3 POSIX contracts** headlessly in local development environments and CI pipelines.

---

## 🧪 Two-Tier Verification Architecture

```text
+----------------------------------------------------------------------------------+
| LAYER 1: Ring 0 In-Kernel Unit Test Framework (kernel/tests/)                    |
| - Runs during early boot before jumping to userland                              |
| - Directly validates kernel memory, page tables, string libraries, and scheduler |
+----------------------------------------------------------------------------------+
                                         |
                                         v
+----------------------------------------------------------------------------------+
| LAYER 2: Ring 3 Userland Specification Suites (userland/src/test_*)              |
| - Executed under static musl libc from the USTAR ramdisk                         |
| - Validates POSIX error handling (-EBADF, -EFAULT, -ENOSYS), mmap & process state|
+----------------------------------------------------------------------------------+
                                         |
                                         v
+----------------------------------------------------------------------------------+
| ORCHESTRATION: Python QEMU TCP Serial Socket Test Runner (scripts/test_runner.py)|
| - Launches headless QEMU with OVMF UEFI and TCP serial server (Port 4444)        |
| - Drives deterministic state machine, exercises user applications, asserts PASS  |
+----------------------------------------------------------------------------------+
```

---

## 🔬 Layer 1: Ring 0 In-Kernel Test Suites (`kernel/tests/`)

Executed automatically by `ktest_run_all()` during `kernel_main()`:

1. **`test_kstring.c`**:
   - Asserts correctness of `kstrlen`, `kstrcmp`, `kstrncmp`, `kstrncpy`, `kmemset`, and `kmemcpy`.
2. **`test_pmm.c`**:
   - Tests 4KB frame allocation (`alloc_page()`), contiguous multi-page allocation (`alloc_pages()`), frame freeing, and bitmap boundary conditions.
3. **`test_vmm.c`**:
   - Tests page table mapping/unmapping, huge-page splitting, `vma_create()`, `vma_find()`, `vma_protect()`, and `vma_remove()`.
4. **`test_tarfs.c`**:
   - Validates USTAR header parsing, octal size decoding, path normalization (`/bin/init` vs `bin/init`), and lookup safety.
5. **`test_sched.c`**:
   - Validates process table layout invariants, PID generation, and `futex_wait()` / `futex_wake()` sleeping channels.

---

## 🛡️ Layer 2: Ring 3 Userland Specification Suites

Packaged inside `initrd.tar` and launched by `/bin/init`:

1. **`test_syscall_safety.c`**:
   - Asserts `-EBADF` (-9) when reading/writing invalid file descriptors.
   - Asserts `-EFAULT` (-14) when passing NULL or kernel-space virtual address pointers.
   - Asserts `-ENOSYS` (-38) when dispatching unimplemented system call numbers.
   - Asserts correct handling of 0-byte I/O buffer requests.
2. **`test_vmm_demand.c`**:
   - Allocates 8 MB anonymous memory via `mmap()`, touches every page to trigger on-demand `#PF` allocation, verifies 100% zero-fill initialization, tests `mprotect()`, and validates partial-region unmapping with `munmap()`.
3. **`test_process_lifecycle.c`**:
   - Performs stress `fork()` operations, validates child process execution, and verifies exit status code propagation to `waitpid()`.

---

## 🤖 Automated QEMU Test Runner (`scripts/test_runner.py`)

The automated harness runs QEMU in headless mode (`-nographic`) with the serial console routed to a TCP socket on port `4444`:

```bash
qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:build/esp,format=raw \
    -serial tcp:127.0.0.1:4444,server=on,wait=off \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -nographic \
    -net none \
    -monitor none
```

### Deterministic State Machine Sequence:

1. **Connect**: Connects over TCP socket to serial port `4444`.
2. **Step 1**: Launches `/bin/sysinfo` (Option 2) and asserts hardware metrics.
3. **Step 2**: Launches `/bin/bench` (Option 3) and asserts FPU/SSE Pi calculation.
4. **Step 3**: Launches `/bin/calc` (Option 1), inputs sides `3` and `4`, asserts hypotenuse `5.00`.
5. **Step 4**: Launches `/bin/tasks` (Option 4), sends `status` and `ping`, asserts `pong`.
6. **Step 5**: Launches `/bin/threads` (Option 5), runs all tests, asserts `MUTEX SYNCHRONIZATION SUCCESS!`.
7. **Step 6**: Launches specification suites (Option 6), asserts all Ring 0 & Ring 3 tests pass.
8. **Step 7**: Sends exit command (Option 7), triggers clean shutdown via `isa-debug-exit`.

---

## 🚀 Running Tests

### Local Execution:
```bash
make test
```

### Containerized Docker Execution:
```bash
make docker-test
```
