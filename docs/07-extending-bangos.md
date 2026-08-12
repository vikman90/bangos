# 07 - Extending BangOS Developer Guide

This guide explains step-by-step how to extend **BangOS** by adding new system calls, hardware drivers, or userland C applications.

---

## ➕ Adding a New System Call (Syscall)

To add a new Linux syscall (e.g., `SYS_GETPID = 39`):

### 1. Define Constant in `kernel/syscall/syscall.h`
```c
#define SYS_GETPID 39
```

### 2. Implement Handler in `kernel/syscall/syscall.c`
Add a new `case` inside `do_syscall()`:

```c
        case SYS_GETPID: {
            // Returns current process PID (1 for main application)
            return 1;
        }
```

### 3. Test New Syscall from C Userland
Compile an application in `app/` that invokes `getpid()` or uses inline assembly:

```c
#include <unistd.h>
#include <stdio.h>

int main(void) {
    printf("My PID on BangOS is: %d\n", getpid());
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
4. Call driver initialization functions inside `kernel_main()` prior to launching ELF userland binaries.

---

## 🏃 Adding New Userland Applications

To change the default user application executed at boot:

1. Create or edit C code in `app/calc.c` or create a new file in `app/`.
2. Update `app/Makefile` if changing the target output binary name.
3. Update the `esp` rule in the root `Makefile` to copy the new target binary to the FAT32 ESP image:
   ```makefile
   esp: app $(EFI_TARGET)
   	cp app/your_new_app $(ESP_DIR)/your_new_app
   ```
4. Update `boot/main.c` to open your new ELF binary file:
   ```c
   Status = uefi_call_wrapper(RootVolume->Open, 5, RootVolume, &AppFile, L"your_new_app", EFI_FILE_MODE_READ, 0);
   ```
