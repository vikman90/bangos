# 11 - Extending BangOS Developer Guide

This developer guide provides step-by-step instructions for extending **BangOS** by adding new system calls, hardware drivers, standalone userland applications, and automated test suites.

---

## ➕ 1. Adding a New System Call (Syscall)

To implement a new Linux system call (e.g. `SYS_GETCWD = 79`):

### Step 1: Define Syscall Number in `kernel/syscall/syscall.h`
```c
#define SYS_GETCWD 79
```

### Step 2: Implement Handler in `kernel/syscall/syscall.c`
Add a `case` statement to `do_syscall()`. Ensure all userland pointers are validated:

```c
case SYS_GETCWD: {
    char *buf = (char *)arg1;
    size_t size = (size_t)arg2;

    // Validate userland pointer safety
    if (!buf || (uint64_t)buf >= 0xFFFFFFFF00000000ULL) {
        return -14; // -EFAULT
    }
    if (size < 2) {
        return -34; // -ERANGE
    }

    buf[0] = '/';
    buf[1] = '\0';
    return (int64_t)buf;
}
```

### Step 3: Invoke from C Userland
Write standard C code in `userland/src/` invoking `getcwd()` or `syscall(SYS_GETCWD, ...)`:

```c
#include <unistd.h>
#include <stdio.h>

int main(void) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("Current working directory: %s\n", cwd);
    }
    return 0;
}
```

### Step 4: Synchronize Documentation
Update [`docs/04-syscall-abi.md`](04-syscall-abi.md) with the new syscall number, argument semantics, and error codes.

---

## 🎮 2. Adding a New Hardware Driver

To add a new hardware peripheral driver (e.g. RTC clock or serial mouse):

1. Create `kernel/drivers/rtc.c` and `kernel/drivers/rtc.h`.
2. The root `Makefile` automatically compiles all `.c` files in `kernel/`:
   ```makefile
   C_SRCS = $(shell find kernel -name "*.c" 2>/dev/null)
   ```
3. Include the header and call the driver initialization function inside `kernel_main()` in [`kernel/main.c`](file:///root/test/little-bang/kernel/main.c) prior to launching userland.
4. Document the hardware port map and registers in [`docs/08-hardware-drivers.md`](08-hardware-drivers.md).

---

## 🏃 3. Adding a Standalone Userland Application

All userland applications are compiled with `musl-gcc` and packed into `initrd.tar`:

### Step 1: Create Source File in `userland/src/myapp.c`
```c
#include "tui.h"
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    tui_print_header("My New BangOS Application");
    printf("Hello from user space!\n");
    tui_pause();
    return 0;
}
```

### Step 2: Register in `userland/Makefile`
Add the binary target and include it in `TARGETS` and the `initrd.tar` archive rule:

```makefile
$(BIN_DIR)/myapp: src/myapp.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

TARGETS = ... $(BIN_DIR)/myapp
```

### Step 3: Register Menu Option in `userland/src/init.c`
Add an entry in `show_menu()` and invoke `launch_program("/bin/myapp", "myapp")`.

---

## 🧪 4. Adding Tests & Updating Test Harness

1. **Ring 0 Unit Tests**: Add test assertions in `kernel/tests/` using `KASSERT` and register in `kernel/tests/ktest.c`.
2. **Ring 3 Specification Tests**: Add standalone test binaries in `userland/src/test_*.c`.
3. **QEMU Harness**: If menu options or output strings change, update the state transitions and regex assertions in [`scripts/test_runner.py`](file:///root/test/little-bang/scripts/test_runner.py).

---

## ✅ Compliance & Quality Checklist

Before submitting changes:
- [ ] Dedicated topic branch created (`feat/`, `fix/`, `docs/`, `test/`).
- [ ] Code, comments, documentation, and commit messages are 100% in English.
- [ ] `make clean && make all` compiles cleanly without warnings (`-Wall -Wextra`).
- [ ] `make test` executes with 0 assertion failures.
- [ ] Documentation updated across relevant files in `docs/` and `README.md`.
- [ ] Commits follow Conventional Commits format with `Assisted-by: AI Assistant <assistant@antigravity.ai>` trailer.
