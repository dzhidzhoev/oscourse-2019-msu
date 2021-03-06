#include <inc/mmu.h>
#include <inc/memlayout.h>

.data
// Define the global symbols 'envs', 'pages', 'vsys', 'uvpt', and 'uvpd'
// so that they can be used in C as if they were ordinary global arrays.
	.globl envs
envs:	.long UENVS
	.globl pages
pages:	.long UPAGES
	.globl vsys
vsys:	.long UVSYS
	.globl uvpt
uvpt:	.long UVPT
	.globl uvpd
uvpd:	.long (UVPT+(UVPT>>12)*4)
__stack_chk_guard: .long 0xdeadbeef
	.globl __stack_chk_guard

// Entrypoint - this is where the kernel (or our parent environment)
// starts us running when we are initially loaded into a new environment.
.text
.globl _start
_start:
	// See if we were started with arguments on the stack
#ifndef CONFIG_KSPACE
	cmpl $USTACKTOP, %esp
	jne args_exist
#endif

	// If not, push dummy argc/argv arguments.
	// This happens when we are loaded by the kernel,
	// because the kernel does not know about passing arguments.
	pushl $0
	pushl $0

args_exist:
	call libmain
1:	jmp 1b

