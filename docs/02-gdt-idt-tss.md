# 02 - Segmentation, GDT, TSS, and IDT

Unlike 32-bit Protected Mode, the x86_64 Long Mode architecture uses a Flat Memory Model. However, descriptors for the **GDT** (Global Descriptor Table) and **TSS** (Task State Segment) remain mandatory to define privilege levels CPL0 (Kernel) and CPL3 (User).

---

## 🛡️ GDT Layout in BangOS (`kernel/arch/x86_64/gdt.c`)

| Index | Selector | Name | Attributes / DPL | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0` | `0x00` | Null Descriptor | - | Mandatory null descriptor |
| `1` | `0x08` | Kernel Code (CS) | `0x9A`, Long Mode (DPL 0) | Kernel Code segment |
| `2` | `0x10` | Kernel Data (DS/SS) | `0x92` (DPL 0) | Kernel Data/Stack segment |
| `3` | `0x1B` | User Data (DS/SS) | `0xF2` (DPL 3, selector `0x18 | 3`) | User Data/Stack segment |
| `4` | `0x23` | User Code (CS) | `0xFA`, Long Mode (DPL 3, selector `0x20 | 3`) | User Code segment |
| `5` | `0x28` | TSS Descriptor | `0x89` (System Descriptor 16-byte) | Task State Segment |

---

## 📌 TSS Task State & IST Exception Stack

The **TSS** descriptor in x86_64 is used primarily to:
1. Store the kernel stack pointer (`RSP0`) used when transitioning from Ring 3 to Ring 0.
2. Define the **Interrupt Stack Table (IST)**. BangOS configures `IST1` with a dedicated 16KB stack to ensure critical CPU faults (such as Double Fault `#DF` or Page Fault `#PF`) never trigger a Triple Fault due to stack overflow.

---

## ⚡ IDT and Exception Handling (`kernel/arch/x86_64/idt.c`)

The **IDT (Interrupt Descriptor Table)** initializes 256 64-bit interrupt gates.

```c
// Disable legacy 8259 PIC
pic_disable();

// Register descriptors for all 32 native CPU exceptions
for (int i = 0; i < 32; i++) {
    uint8_t ist = (i == 8 || i == 13 || i == 14) ? 1 : 0;
    idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E, ist);
}
```

If a Page Fault (`#PF`), General Protection Fault (`#GP`), or Invalid Opcode (`#UD`) occurs, the CPU jumps to `exception_handler()`, which dumps all general-purpose registers (`RAX`..`R15`), `RIP`, `RSP`, and `CR2` (faulting address) over the serial port, safely halting the system.
