// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	extern volatile pte_t uvpt[];
	if (!((err & FEC_WR) && uvpt[PGNUM(addr)] & PTE_P)) {
		panic("invalid copy on write operation!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0) {
		panic("alloc page failed: %e", r);
	}
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy(PFTEMP, addr, PGSIZE);

	if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W)) < 0)
		panic("%e", r);

	if ((r = sys_page_unmap(0, PFTEMP)) < 0) 
		panic("%e", r);
	
	// panic("pgfault not implemented");
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

	// LAB 4: Your code here.
	if (!(pn >= PGNUM(UTEXT) && pn < PGNUM(USTACKTOP))) {
		cprintf("invalid address: %p\n", pn * PGSIZE);
	}
	extern volatile pte_t uvpt[];

	uintptr_t vaddr = pn * PGSIZE;
	int perm = PGOFF(uvpt[pn]);

	if ((perm & PTE_SHARE)) {
		if ((r = sys_page_map(thisenv->env_id, (void *)vaddr, envid, (void *)vaddr, perm & PTE_SYSCALL)) < 0)
			panic("sys page map share %e", r);
		return 0;
	}

	if ((perm & PTE_W) || (perm & PTE_COW))
		perm = (PGOFF(uvpt[pn]) & (~PTE_W)) | PTE_COW;

	if ((r = sys_page_map(thisenv->env_id, (void *)vaddr, envid, (void *)vaddr, perm & PTE_SYSCALL)) < 0) {  //注意这里要&PTE_SYSCALL
		panic("sys page map %e", r);
	}
	if ((r = sys_page_map(thisenv->env_id, (void *)vaddr, thisenv->env_id, (void *)vaddr, perm & PTE_SYSCALL)) < 0) {
		panic("sys page map 2 %e", r);
	} //标记COW：重新映射一遍而不是直接修改uvpt，否则会无效写。换句话说uvpt是只读的

	// panic("duppage not implemented");
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
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	int r;
	envid_t son_env_id;

	son_env_id = sys_exofork();

	if (son_env_id < 0)
		panic("fork failed");
	if (son_env_id == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	extern volatile pte_t uvpt[];
	extern volatile pde_t uvpd[];

	for (unsigned int i = PGNUM(UTEXT); i < PGNUM(USTACKTOP); i++) {  // 这里必须是unsigned int，否则会出现无效地址写
		if ((uvpd[PDX(i * PGSIZE)] & PTE_P) && (uvpt[i] & PTE_P)) {
			if ((r = duppage(son_env_id, i)) < 0)
				panic("duplicate page %e", r);
		}
	}

	if ((r = sys_page_alloc(son_env_id, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_W | PTE_U)) < 0)
		panic("sys_page_alloc %e", r);

	extern void _pgfault_upcall(void);

	if ((r = sys_env_set_pgfault_upcall(son_env_id, _pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall %e", r);

	if ((r = sys_env_set_status(son_env_id, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status %e", r);

	return son_env_id;
	// panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
