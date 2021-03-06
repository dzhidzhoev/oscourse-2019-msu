/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_PICIRQ_H
#define JOS_KERN_PICIRQ_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#define MAX_IRQS	16	// Number of IRQs

// I/O Addresses of the two 8259A programmable interrupt controllers
#define IO_PIC1		0x20	// Master (IRQs 0-7)
#define IO_PIC2		0xA0	// Slave (IRQs 8-15)

#define IO_PIC1_CMND	IO_PIC1
#define IO_PIC1_DATA	IO_PIC1+1

#define IO_PIC2_CMND	IO_PIC2
#define IO_PIC2_DATA	IO_PIC2+1

#define IRQ_SLAVE	2	// IRQ at which slave connects to master

#define PIC_EOI		0x20


#ifndef __ASSEMBLER__

#include <inc/types.h>
#include <inc/x86.h>

extern uint16_t irq_mask_8259A;
void pic_init(void);
uint16_t irq_readmask_8259A(void);
void irq_setmask_8259A(uint16_t mask);
void pic_send_eoi(uint8_t irq);
#endif // !__ASSEMBLER__

#endif // !JOS_KERN_PICIRQ_H
