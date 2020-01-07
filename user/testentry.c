#include <inc/lib.h>
#include <inc/x86.h>

void
umain(int argc, char **argv)
{
	if (argc == 1) {
		cprintf("[%x] My eip 0x%x\n", sys_getenvid(), read_eip());
	} else if (argc > 1) {
		int eip = read_eip();
		if (write(1, &eip, sizeof(eip)) != sizeof(eip)) {
			panic("unable to write eip");
		}
	}
	assert((uint32_t)read_eip() >= UTEXT);
	assert((uint32_t)read_eip() < FDTABLE);
}
