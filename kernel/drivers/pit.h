#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQ 1193182

void     pic_remap(void);
void     pic_send_eoi(uint8_t irq);
void     pit_init(uint32_t frequency_hz);
void     pit_unmask_irq0(void);
uint64_t pit_get_ticks(void);

#endif /* PIT_H */
