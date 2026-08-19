# 07 - FPU & SIMD / SSE Floating-Point Support

Modern 64-bit standard C libraries (`musl`, `glibc`) rely heavily on SIMD/SSE vector registers (`%xmm0` .. `%xmm15`) and hardware instructions (`sqrtsd`, `addsd`, `mulsd`, `movaps`) for:

- Basic floating-point calculations (`sqrt()`, `pow()`, trigonometry).
- String and memory formatting (`printf("%.2f")`, `scanf("%lf")`).
- Vectorized standard library optimizations.

---

## ⚠️ The CPU State Post-UEFI & `#UD` Faults

Upon exiting UEFI boot services (`ExitBootServices`), the firmware leaves the CPU control registers in a state where SSE extensions are disabled for 64-bit execution.

If an application attempts to execute an SSE instruction (such as `sqrtsd`) while SSE support is disabled:

- The CPU encounters an unhandled opcode and triggers an **Invalid Opcode Exception (#UD, Vector 6)**.
- If `CR0.TS` (Task Switched) is set without clearing, the CPU triggers a **Device Not Available Exception (#NM, Vector 7)**.

---

## 🛠️ Hardware Control Register Configuration (`kernel/main.c`)

During `kernel_main()`, BangOS explicitly configures `%cr0` and `%cr4`:

```c
static void fpu_sse_init(void) {
    uint64_t cr0, cr4;

    // 1. Configure Control Register 0 (%cr0)
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear CR0.EM (Disable software FPU emulation)
    cr0 |= (1ULL << 1);  // Set CR0.MP   (Monitor Coprocessor)
    cr0 &= ~(1ULL << 3); // Clear CR0.TS (Clear Task Switched flag)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    // 2. Configure Control Register 4 (%cr4)
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set CR4.OSFXSR (Enable FXSAVE/FXRSTOR & SSE instructions)
    cr4 |= (1ULL << 10); // Set CR4.OSXMMEXCPT (Enable SIMD Floating-Point Exceptions #XF)
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));

    kprintf("[FPU/SSE] Hardware floating-point & SSE extensions enabled.\n");
}
```

### Control Flags Explanation:

* **`CR0.EM` (Bit 2 - Emulation)**: When cleared, forces CPU to execute math instructions on physical silicon instead of trapping to software emulation handlers.
* **`CR0.MP` (Bit 1 - Monitor Coprocessor)**: Controls interaction with `WAIT`/`FWAIT` instructions.
* **`CR0.TS` (Bit 3 - Task Switched)**: When cleared, allows immediate FPU/SSE execution without generating `#NM` faults.
* **`CR4.OSFXSR` (Bit 9 - OS FXSAVE/FXRSTOR Support)**: Enables 64-bit execution of SSE/SSE2 instructions and permits execution of `fxsave64` / `fxrstor64`.
* **`CR4.OSXMMEXCPT` (Bit 10 - OS Unmasked Exception Support)**: Routes SIMD floating-point exceptions to the `#XF` exception handler (Vector 19).

---

## 💾 FPU State Preservation During Context Switches

Each process and thread maintains an isolated 512-byte FPU state buffer aligned to a 16-byte boundary (`uint8_t fpu_state[512] __attribute__((aligned(16)))`):

### 1. Initialization (`process_create_from_elf`)
```c
// Reset FPU to clean default state and save initial FXSAVE image
__asm__ volatile ("fninit; fxsave64 %0" : "=m"(proc->fpu_state));
```

### 2. Context Switching (`scheduler_tick`)
When switching from `curr` to `next`:
```c
if (curr->active) {
    __asm__ volatile ("fxsave64 %0" : "=m"(curr->fpu_state));
}
__asm__ volatile ("fxrstor64 %0" : : "m"(next->fpu_state));
```

---

## 📐 System V AMD64 ABI 16-Byte Stack Alignment

The x86_64 System V ABI mandates that the stack pointer `RSP` must be **16-byte aligned immediately before a `call` instruction** (so that upon function entry, `(RSP + 8)` is a multiple of 16).

If userland or kernel code executes aligned SSE vector instructions (such as `movaps`) with an unaligned `RSP`, the CPU immediately triggers a **General Protection Fault (`#GP`, Vector 13)**. BangOS guarantees 16-byte alignment when initializing user stacks in `process_create_from_elf()` and `userland/include/synch.h`.
