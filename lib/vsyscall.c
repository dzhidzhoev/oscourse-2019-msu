#include <inc/vsyscall.h>
#include <inc/lib.h>

static inline int32_t
vsyscall(int num)
{
	return ((int*)UVSYS)[num];
}

int vsys_gettime(void)
{
	return vsyscall(VSYS_gettime);
}