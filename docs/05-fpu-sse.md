# 05 - FPU and SIMD / SSE Support

Standard 64-bit C libraries (`musl`, `glibc`) and math functions such as `sqrt()`, `pow()`, `sin()`, or floating-point formatting in `printf("%.2f")` utilize SIMD/SSE registers (`%xmm0` .. `%xmm15`) and hardware instructions like `sqrtsd`, `mulsd`, and `addsd`.

---

## ⚠️ The FPU State Post-UEFI

Upon exiting UEFI boot services (`ExitBootServices`), firmware leaves CPU control registers in a state where SIMD/SSE instructions are not fully enabled for 64-bit kernel/user code.

If a binary attempts to execute an instruction like `sqrtsd` without enabling SSE in `%cr4`, the CPU generates an Invalid Opcode Fault (**#UD - Invalid Opcode Exception 6**).

---

## 🛠️ FPU/SSE Initialization (`kernel/main.c`)

BangOS explicitly configures x86_64 CPU control registers during `kernel_main()`:

```c
static void fpu_sse_init(void) {
    uint64_t cr0, cr4;
    
    // 1. Modify CR0
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear CR0.EM (Emulation: disables software emulation)
    cr0 |= (1ULL << 1);  // Set CR0.MP (Monitor Coprocessor)
    cr0 &= ~(1ULL << 3); // Clear CR0.TS (Task Switched)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    // 2. Modify CR4
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set CR4.OSFXSR (Enable FXSAVE/FXRSTOR & SSE instructions)
    cr4 |= (1ULL << 10); // Set CR4.OSXMMEXCPT (Enable SIMD Floating-Point Exceptions)
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));

    kprintf("[FPU/SSE] Hardware floating-point & SSE extensions enabled.\n");
}
```

### Control Flags Explanation:
* **`CR4.OSFXSR` (Bit 9)**: Informs the CPU that the operating system supports saving/restoring FPU/MMX/SSE state and **enables direct execution of 64-bit SSE instructions**.
* **`CR4.OSXMMEXCPT` (Bit 10)**: Directs unmasked SIMD floating-point exceptions to the `#XF` handler in the IDT.
