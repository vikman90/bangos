# 07 - Extending BangOS Developer Guide

This guide explains step-by-step how to extend **BangOS** by adding new system calls, hardware drivers, or standalone userland applications.

---

## ➕ Adding a New System Call (Syscall)

To add a new Linux syscall (e.g., `SYS_GETCWD = 79`):

### 1. Define Constant in `kernel/syscall/syscall.h`
```c
#define SYS_GETCWD 79
```

### 2. Implement Handler in `kernel/syscall/syscall.c`
Add a new `case` inside `do_syscall()`:

```c
        case SYS_GETCWD: {
            char *buf = (char *)arg1;
            size_t size = (size_t)arg2;
            if (!buf || size < 2) return -14; // -EFAULT
            buf[0] = '/';
            buf[1] = '\0';
            return (int64_t)buf;
        }
```

### 3. Test New Syscall from C Userland
Compile an application in `userland/src/` that invokes `getcwd()`:

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

---

## 🎮 Adding a New Hardware Driver

To add a new driver (e.g., PIT 8254 timer or full PS/2 keyboard controller):

1. Create `kernel/drivers/new_driver.c` and `kernel/drivers/new_driver.h`.
2. Include the header in `kernel/main.c`.
3. The root `Makefile` automatically detects and builds any `.c` file located inside `kernel/`:
   ```makefile
   C_SRCS = $(shell find kernel -name "*.c" 2>/dev/null)
   ```
4. Call driver initialization functions inside `kernel_main()` prior to launching userland binaries.

---

## 🏃 Adding New Standalone Userland Applications

The userland environment uses standard USTAR `initrd.tar` packaging with standalone ELFs:

1. Create your standalone C source file in `userland/src/myapp.c` with standard `int main(int argc, char **argv)`.
2. Add your binary target to `userland/Makefile`:
   ```makefile
   $(BIN_DIR)/myapp: src/myapp.c src/tui.o | $(BIN_DIR)
   	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)
   ```
3. Include `myapp` in the `TARGETS` list inside `userland/Makefile` and in the `initrd.tar` archive rule.
4. Call `launch_program("/bin/myapp", "myapp")` from `userland/src/init.c`.
5. Run `make esp` to compile and package into `build/esp/initrd.tar`.
