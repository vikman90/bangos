# 08 - Hardware Device Drivers & Low-Level I/O

BangOS interfaces directly with hardware peripherals via x86_64 I/O port instructions (`inb`, `outb`, `outw`). All driver modules reside in the [`kernel/drivers/`](file:///root/test/little-bang/kernel/drivers/) directory:
1. **16550 UART Serial Driver** (COM1 Serial Console at 38400 baud).
2. **8254 PIT Timer Driver** (100 Hz Preemptive Scheduler Interrupts).
3. **8259 PIC Driver** (Programmable Interrupt Controller remapping).
4. **PS/2 Keyboard Driver** (Scancode decoder).
5. **QEMU Hypervisor Driver** (`isa-debug-exit` & ACPI poweroff ports).

---

## 🔌 Low-Level I/O Port Primitives

Communication with x86_64 legacy hardware uses dedicated bus instructions:

```c
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    // Write to unused port 0x80 to provide hardware bus settling delay
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}
```

---

## 📡 16550 UART Serial Driver (`kernel/drivers/uart.c`)

The 16550 Universal Asynchronous Receiver-Transmitter (UART) provides the primary console interface for BangOS over serial port **COM1 (`0x3F8`)**.

### I/O Port Map:
* `COM1 + 0` (`0x3F8`): Receiver Buffer / Transmitter Holding Register / Divisor Latch Low.
* `COM1 + 1` (`0x3F9`): Interrupt Enable Register / Divisor Latch High.
* `COM1 + 2` (`0x3FA`): FIFO Control Register (FCR).
* `COM1 + 3` (`0x3FB`): Line Control Register (LCR).
* `COM1 + 4` (`0x3FC`): Modem Control Register (MCR).
* `COM1 + 5` (`0x3FD`): Line Status Register (LSR).

### Initialization Sequence:
```c
void uart_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable UART interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (Divisor Latch Access Bit)
    outb(COM1_PORT + 0, 0x03); // Divisor 3 -> 38400 baud (115200 / 3)
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03); // 8 Data Bits, No Parity, 1 Stop Bit (8N1)
    outb(COM1_PORT + 2, 0x07); // Enable FIFO with 1-byte trigger threshold
    outb(COM1_PORT + 4, 0x0B); // Set RTS/DSR lines active
}
```

### Formatted Kernel Printf (`kprintf`)
`kprintf()` provides freestanding variadic output across Ring 0 supporting:
- `%s` (Strings), `%d` (Signed integers), `%u` (Unsigned integers), `%x` (Hexadecimal integers), `%p` (Pointers), `%c` (Characters), `%%` (Literal percent).

---

## ⏱️ 8254 PIT Timer & 8259 PIC Driver (`kernel/drivers/pit.c`)

The **8254 Programmable Interval Timer (PIT)** generates periodic hardware interrupts to drive the multitasking scheduler.

### 1. 8259 PIC Remapping
Legacy PC motherboards map hardware IRQs 0..7 to CPU vectors 8..15 (which overlap with CPU exception vectors). `pic_remap()` shifts the vector offsets:
- **Master PIC**: Vectors `0x20..0x27` (32..39).
- **Slave PIC**: Vectors `0x28..0x2F` (40..47).

### 2. PIT Frequency Programming
The PIT oscillator runs at a base clock of **1,193,182 Hz**. BangOS programs **Channel 0** in **Mode 3 (Square Wave Generator)** for 100 Hz:

$$\text{Divisor} = \frac{1,193,182\text{ Hz}}{100\text{ Hz}} = 11,931\text{ (0x2E9B)}$$

```c
void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    // Channel 0, access lobyte/hibyte, Mode 3 (Square Wave), 16-bit binary
    outb(PIT_COMMAND, 0x36);
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    io_wait();

    pit_ticks = 0;
}
```

---

## ⌨️ PS/2 Keyboard Driver (`kernel/drivers/keyboard.c`)

The PS/2 keyboard controller communicates via:
- **Port `0x64` (Status Register)**: Bit 0 indicates data availability in output buffer.
- **Port `0x60` (Data Register)**: Delivers scancodes.

### Scancode Decoding:
- **Make Codes (Key Press)**: Scancodes with bit 7 clear (`!(scancode & 0x80)`).
- **Break Codes (Key Release)**: Scancodes with bit 7 set (`scancode & 0x80`).
- BangOS translates Scan Code Set 1 into ASCII characters via `scancode_map[]`.

```c
int keyboard_has_char(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 1) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        if (!(scancode & 0x80)) { // Key press
            if (scancode < sizeof(scancode_map)) {
                return scancode_map[scancode];
            }
        }
    }
    return 0;
}
```

---

## ⚡ QEMU Hypervisor Ports (`kernel/drivers/qemu.c`)

To enable deterministic, headless testing in CI pipelines and clean system shutdown:

1. **`isa-debug-exit` Port (`0xF4`)**:
   Writing an exit code to port `0xF4` causes QEMU to terminate immediately with exit code `(val << 1) | 1`.
   ```c
   void qemu_exit(uint8_t code) {
       outb(QEMU_DEBUG_EXIT_PORT, code);
   }
   ```

2. **ACPI Poweroff Port (`0x604`)**:
   Writing `0x2000` to port `0x604` initiates standard ACPI system power down in QEMU.
   ```c
   void qemu_poweroff(void) {
       outw(0x604, 0x2000);
   }
   ```
