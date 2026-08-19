# 02 - Segmentation, GDT, TSS & Exception Architecture

In x86_64 Long Mode, the memory model is predominantly **flat**: the CPU ignores segment base addresses and limits for standard code and data segments (treating the virtual address space as a contiguous 64-bit range from `0x0000000000000000` to `0xFFFFFFFFFFFFFFFF`).

However, the **Global Descriptor Table (GDT)** and **Task State Segment (TSS)** remain strictly required by the x86_64 architecture to:

1. Define **Privilege Levels**: Ring 0 (Kernel CPL 0) vs. Ring 3 (User CPL 3).
2. Configure **Segment Selectors** required by hardware `syscall`/`sysret` instructions (`STAR` MSR).
3. Provide the **Kernel Stack Pointer (`RSP0`)** upon privilege transitions.
4. Provide the **Interrupt Stack Table (IST)** to handle critical CPU faults on known-good stacks.

---

## 🛡️ Global Descriptor Table (GDT) Layout (`kernel/arch/x86_64/gdt.c`)

BangOS configures a 6-entry GDT (where the 6th entry is a 16-byte 64-bit TSS system descriptor):

| Index | Selector | Segment Name | Access Byte | Flags | DPL | Description |
| :---: | :---: | :--- | :---: | :---: | :---: | :--- |
| `0` | `0x00` | **Null Descriptor** | `0x00` | `0x0` | - | Mandatory null descriptor |
| `1` | `0x08` | **Kernel Code (CS)** | `0x9A` | `0xA` (64-bit) | 0 | Ring 0 Long Mode executable code |
| `2` | `0x10` | **Kernel Data (DS/SS)** | `0x92` | `0xC` | 0 | Ring 0 read/write data & stack |
| `3` | `0x1B` | **User Data (DS/SS)** | `0xF2` | `0xC` | 3 | Ring 3 read/write data & stack (`0x18 \| 3`) |
| `4` | `0x23` | **User Code (CS)** | `0xFA` | `0xA` (64-bit) | 3 | Ring 3 Long Mode executable code (`0x20 \| 3`) |
| `5` | `0x28` | **TSS Descriptor** | `0x89` | `0x0` | 0 | 16-byte Task State Segment descriptor |

### GDT Installation & Register Reload
Loading the GDT requires the `lgdt` instruction. Because the CPU does not allow directly `mov`ing into register `CS`, a **far return (`lretq`)** is used to reload the Code Segment register with selector `0x08`:

```c
__asm__ volatile (
    "lgdt %0\n"
    "mov $0x10, %%ax\n"
    "mov %%ax, %%ds\n"
    "mov %%ax, %%es\n"
    "mov %%ax, %%fs\n"
    "mov %%ax, %%gs\n"
    "mov %%ax, %%ss\n"
    "pushq $0x08\n"          // Push new Kernel CS selector
    "leaq 1f(%%rip), %%rax\n"// Compute target RIP address
    "pushq %%rax\n"          // Push return RIP
    "lretq\n"                // Far return: pops RIP and CS atomically
    "1:\n"
    "mov $0x28, %%ax\n"      // TSS selector (0x28)
    "ltr %%ax\n"             // Load Task Register
    : : "m"(gdt_r) : "rax"
);
```

---

## 📌 Task State Segment (TSS) & IST Stacks

The 64-bit TSS in x86_64 is 104 bytes long. Unlike 32-bit hardware task switching (which is deprecated in Long Mode), the 64-bit TSS serves two critical hardware roles:

1. **`RSP0` (Privilege Stack Pointer)**:
   When an interrupt or exception occurs while the CPU is in Ring 3, the CPU automatically switches its stack pointer (`RSP`) to the address stored in `TSS.RSP0` before pushing the userland registers. BangOS updates `TSS.RSP0` dynamically on every context switch via `gdt_set_kernel_stack()`.

2. **`IST1` (Interrupt Stack Table 1)**:
   If a stack overflow occurs in user space or kernel space, triggering an exception on the same corrupted stack would immediately trigger a Double Fault (`#DF`), which in turn would fail on the corrupted stack and cause an unrecoverable **Triple Fault (Hardware CPU Reset)**.
   
   BangOS allocates a dedicated **16 KB safe stack** for `IST1`:
   ```c
   kernel_tss.ist1 = (uint64_t)&exception_ist_stack[sizeof(exception_ist_stack)];
   ```
   Critical CPU fault gates (Double Fault `#DF` Vector 8 and General Protection Fault `#GP` Vector 13) are assigned `IST=1` in their IDT descriptors, ensuring they always execute on a clean, valid stack.

---

## ⚡ Interrupt Descriptor Table (IDT) (`kernel/arch/x86_64/idt.c`)

The IDT is a table of 256 16-byte gate descriptors loaded via `lidt`.

```text
64-Bit Interrupt Gate Descriptor Layout (16 Bytes):
+---------------------------------------------------------------+
| Bytes 12-15: Offset bits 32..63 (High 32 bits of ISR RIP)     |
+---------------------------------------------------------------+
| Bytes 8-11:  Reserved (Must be 0)                             |
+---------------------------------------------------------------+
| Byte 5:      Type & Attributes (0x8E = Present, Ring 0, Gate) |
| Byte 4:      IST Offset (bits 0..2)                           |
| Bytes 2-3:   Segment Selector (0x08 = Kernel CS)              |
| Bytes 0-1:   Offset bits 0..15 (Low 16 bits of ISR RIP)       |
+---------------------------------------------------------------+
```

### IDT Configuration Steps:

1. **Remap 8259 PIC**: Remaps Master PIC vectors to `0x20..0x27` (32..39) and Slave PIC to `0x28..0x2F` (40..47) to prevent overlap with standard CPU exceptions `0..31`.
2. **Register CPU Exception Handlers (0..31)**: Connects each vector to an assembly stub in `kernel/arch/x86_64/isr.s` via `isr_stub_table[i]`.
3. **Register Timer IRQ 0 (Vector 32)**: Connects vector 32 to `isr_32`, driving the 100 Hz preemptive scheduler.
4. **Load IDT**: Executes `lidt` with the descriptor table bounds.

---

## 🔍 Context Frame & Exception Diagnostics

When a CPU exception or interrupt fires, the CPU automatically pushes:
`SS`, `RSP`, `RFLAGS`, `CS`, `RIP`, and optionally an `Error Code`.

The assembly stubs in [`kernel/arch/x86_64/isr.s`](file:///root/test/little-bang/kernel/arch/x86_64/isr.s) push the remaining general-purpose registers to construct a complete [`context_frame_t`](file:///root/test/little-bang/kernel/arch/x86_64/idt.h):

```c
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec_num;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) context_frame_t;
```

### Panic Diagnostics Output
If an unhandled exception occurs, `exception_handler()` formats a full diagnostic register dump over the serial console:

```text
======================================================
 [CPU EXCEPTION #13] ErrorCode=0x0000000000000000
 RIP=0x00000000004010a2  RSP=0x00007ffffffefef0  CR2=0x0000000000000000
 RAX=0x0000000000000000  RBX=0x0000000000402000  RCX=0x0000000000000010  RDX=0x0000000000000000
 RSI=0x0000000000403000  RDI=0x0000000000000001  RBP=0x00007ffffffeff20
======================================================
```
For Page Faults (`Vector 14`), the handler inspects `CR2` and delegates to the Virtual Memory Manager (`vmm_handle_page_fault()`) for Demand Paging before falling back to terminating the faulting process.
