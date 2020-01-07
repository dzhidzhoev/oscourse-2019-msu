// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

extern void _pgfault_upcall();

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;addr=addr;
	uint32_t err = utf->utf_err;err=err;
	envid_t envid = sys_getenvid();
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	if (!(utf->utf_err & FEC_WR)) {
		panic("pgfault: not write %p", addr);
	}
	if (!(uvpt[PGNUM(addr)] & PTE_COW)) {
		panic("pgfault: not cow %p", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	//   Make sure you DO NOT use sanitized memcpy/memset routines when using UASAN.

	if ((r = sys_page_alloc(envid, PFTEMP, PTE_U | PTE_P | PTE_W))) {
		panic("pgfault: failed to allocate temporary memory %i", r);
	}
#ifdef SANITIZE_USER_SHADOW_BASE
	__nosan_memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
#else
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
#endif
	if ((r = sys_page_map(envid, PFTEMP, envid, ROUNDDOWN(addr, PGSIZE), PTE_U | PTE_P | PTE_W))) {
		panic("pgfault: failed to map %i", r);
	}
	sys_page_unmap(envid, PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr = (void*)(pn * PGSIZE);
	if ((uvpt[pn] & PTE_COW) || (uvpt[pn] & PTE_W)) {
		envid_t penvid = sys_getenvid();
		if ((r = sys_page_map(penvid, addr, envid, addr, PTE_P | PTE_U | PTE_COW))) {
			panic("duppage: failed to map page to child process with cow %i", r);
		}
		if ((r = sys_page_map(penvid, addr, penvid, addr, PTE_P | PTE_U | PTE_COW))) {
			panic("duppage: failed to mark own page as COW %i", r);
		}
	} else {
		if ((r = sys_page_map(sys_getenvid(), addr, envid, addr, PTE_P | PTE_U))) {
			panic("duppage: failed to map r/o page %i", r);
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	envid_t envid = sys_getenvid();
	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if (envid < 0) {
		return envid;
	}
	if (!envid) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (uintptr_t addr = 0; addr < USTACKTOP; addr += PGSIZE) {
#ifdef SANITIZE_USER_SHADOW_BASE
		if (addr >= SANITIZE_USER_SHADOW_BASE && addr <= SANITIZE_USER_SHADOW_BASE + SANITIZE_USER_SHADOW_SIZE)			 
			continue;

		if (addr >= SANITIZE_USER_EXTRA_SHADOW_BASE && addr <= SANITIZE_USER_EXTRA_SHADOW_BASE + SANITIZE_USER_EXTRA_SHADOW_SIZE) 
			continue;

		if (addr >= SANITIZE_USER_FS_SHADOW_BASE && addr <= SANITIZE_USER_FS_SHADOW_BASE + SANITIZE_USER_FS_SHADOW_SIZE) 
			continue;
#endif
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)) {
			if (uvpt[PGNUM(addr)] & PTE_SHARE) {
				if (sys_page_map(0, (void*)addr, envid, (void*)addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) {
					panic("fork: failed to map shared page");
				}
			} else {
				if (duppage(envid, PGNUM(addr))) {
					panic("fork: duppage failed");
				}
			}
		}
	}

	duppage(envid, PGNUM(UCANARY));
	if (sys_page_alloc(envid, (void*)UXSTACKTOP - UXSTACKSIZE, PTE_P | PTE_U | PTE_W)) {
		panic("fork: failed to allocate exception stack");
	}
	if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) {
		panic("fork: sys_env_set_pgfault_upcall failed for child");
	}

// Duplicating shadow addresses is insane. Make sure to skip shadow addresses in COW above.
#ifdef SANITIZE_USER_SHADOW_BASE
	{
		uint32_t addr;
		for (addr = SANITIZE_USER_SHADOW_BASE; addr < SANITIZE_USER_SHADOW_BASE +
			SANITIZE_USER_SHADOW_SIZE; addr += PGSIZE)
			if (sys_page_alloc(envid, (void *)addr, PTE_P | PTE_U | PTE_W))
				panic("Fork: failed to alloc shadow base page");
		for (addr = SANITIZE_USER_EXTRA_SHADOW_BASE; addr < SANITIZE_USER_EXTRA_SHADOW_BASE +
			SANITIZE_USER_EXTRA_SHADOW_SIZE; addr += PGSIZE)
			if (sys_page_alloc(envid, (void *)addr, PTE_P | PTE_U | PTE_W))
				panic("Fork: failed to alloc shadow extra base page");
		for (addr = SANITIZE_USER_FS_SHADOW_BASE; addr < SANITIZE_USER_FS_SHADOW_BASE +
			SANITIZE_USER_FS_SHADOW_SIZE; addr += PGSIZE)
			if (sys_page_alloc(envid, (void *)addr, PTE_P | PTE_U | PTE_W))
				panic("Fork: failed to alloc shadow fs base page");
	}
#endif

	if (sys_env_set_status(envid, ENV_RUNNABLE)) {
		panic("fork: can't make child runnable");
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
