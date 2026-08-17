# 06 - Automated Testing and QEMU Simulation

BangOS includes a comprehensive, specification-driven test architecture covering both Ring 0 kernel subsystems and Ring 3 userland POSIX contracts, orchestrated by `scripts/test_runner.py` for headless local and GitHub CI validation.

---

## 🧪 Testing Architecture Layers

### 1. Ring 0 In-Kernel Unit Test Framework (`kernel/tests/`)
Executed during early boot prior to launching userland `/bin/init`:
* **`test_kstring.c`**: Validates kernel string manipulation and memory copying primitives (`kstrlen`, `kstrcmp`, `kstrncmp`, `kstrncpy`, `kmemset`, `kmemcpy`).
* **`test_pmm.c`**: Validates Physical Memory Manager page frames, multi-page contiguity, alignment, and bounds.
* **`test_vmm.c`**: Validates page table mapping/unmapping, protection bit modifications, and VMA punch-hole splitting.
* **`test_tarfs.c`**: Validates USTAR header parsing, path normalization, and bounds safety.
* **`test_sched.c`**: Validates scheduler state invariants, process table layout, and futex wait/wake edge cases.

### 2. Ring 3 Userland Specification Suites (`userland/src/`)
Packaged into `initrd.tar` and executed under static musl libc:
* **`test_syscall_safety.c`**: Asserts POSIX error propagation (`-EBADF` on invalid descriptors, `-EFAULT` on NULL/kernel virtual addresses, `-ENOSYS` on unimplemented syscalls, zero-byte I/O handling).
* **`test_vmm_demand.c`**: Validates 8MB anonymous `mmap()` demand paging `#PF`, zero-fill initialization, `mprotect()`, and partial hole `munmap()`.
* **`test_process_lifecycle.c`**: Validates stress `fork()`, child process execution, and `waitpid()` status code propagation.

---

## ⚡ QEMU Instant-Exit Device (`isa-debug-exit`)

To enable deterministic and fast teardown in CI pipelines, QEMU is started with the `isa-debug-exit` device:

```bash
qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:build/esp,format=raw \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -serial tcp:127.0.0.1:4444,server=on,wait=off \
    -nographic \
    -net none \
    -monitor none
```

Writing to I/O port `0xf4` allows the kernel to trigger immediate host termination upon fatal panic or completed test runs.

---

## 🤖 Executing Tests

Run the complete test harness locally:

```bash
make test
```

Or inside an isolated Docker container:

```bash
make docker-test
```
