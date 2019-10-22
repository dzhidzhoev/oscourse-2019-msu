/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/trap.h>
#include <kern/kclock.h>
#include <kern/picirq.h>

void
rtc_init(void)
{
	nmi_disable();
	outb(IO_RTC_CMND, RTC_BREG);
	int reg_b = inb(IO_RTC_DATA);
	reg_b |= RTC_PIE;
	outb(IO_RTC_CMND, RTC_BREG);
	outb(IO_RTC_DATA, reg_b);

	// freq
	outb(IO_RTC_CMND, RTC_AREG);
	int reg_a = inb(IO_RTC_DATA);
	outb(IO_RTC_CMND, RTC_AREG);
	outb(IO_RTC_DATA, 0xF | (reg_a & 0xF0));

	irq_setmask_8259A(irq_readmask_8259A() & ~(1 << IRQ_CLOCK));
	rtc_check_status();
	nmi_enable();
}

uint8_t
rtc_check_status(void)
{
	uint8_t status = 0;
	
	outb(IO_RTC_CMND, RTC_CREG);
	status = inb(IO_RTC_DATA);

	return status;
}

unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC_CMND, reg);
	return inb(IO_RTC_DATA);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC_CMND, reg);
	outb(IO_RTC_DATA, datum);
}

