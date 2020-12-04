// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display information about the function stack", mon_backtrace },
	{ "showmapping", "Display physical address mapping condition in virtual address [arg1, arg2]", mon_showmapping },
	{ "setperm", "Change permission of the page containing address arg1 to arg2, report error if the page is unmapped", mon_setperm },
	{ "dumpmem", "Dump the memory in range [arg1, arg2], arg3 = 1 for virtual address and arg3 = 0 for physical address", mon_dumpmem },
	{ "stepi", "Used in breakpoint to step a single instruction", mon_stepi }, 
	{ "continue", "Used in breakpoint to continue execution", mon_continue }
};

/***** Implementations of basic kernel monitor commands *****/

unsigned long long atoi(char *str)
{
	unsigned long long i = 0;
	int base  = 10;
	if (str[0] == '0') {
		if (str[1] == 'x' || str[1] == 'X') {
			base = 16;
			str = str + 2;
		}
		else {
			base = 8;
			str = str + 1;
		}
	}
	for (; *str; str++) {
		switch(base) {
			case 8:{
				if (!(*str >= '0' && *str <= '7')) {
					cprintf("Wrong format!\n");
					return -1;
				}
				i = i * base + (*str) - '0';
				break;
			}
			case 10:{
				if (!(*str >= '0' && *str <= '9')) {
					cprintf("Wrong format!\n");
					return -1;
				}
				i = i * base + (*str) - '0';
				break;
			}
			case 16:{
				int num;
				if (*str >= '0' && *str <= '9')
					num = (*str) - '0';
				else if (*str >= 'A' && *str <= 'F')
					num = (*str) - 'A' + 10;
				else if (*str >= 'a' && *str <= 'f')
					num = (*str) - 'a' + 10;
				else {
					cprintf("Wrong format!\n");
					return -1;
				}
				i = i * base + num;
				break;
			}
		}
	}
	return i;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t ebp;

	cprintf("Stack backtrace: \n");
	ebp = read_ebp();
	
	while (true) {
		uint32_t eip = *((uint32_t *)ebp + 1);
		uint32_t arglist[5]; 

		for (int i = 0; i < 5; i++) {
			arglist[i] = *((uint32_t *)ebp + (i + 2));
		}

		// print current stack trace
		cprintf(" ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", (void *)ebp, (void *)eip, 
			(void *)arglist[0], (void *)arglist[1], (void *)arglist[2], (void *)arglist[3], (void *)arglist[4]);

		struct Eipdebuginfo info;
		uintptr_t addr;
		if (debuginfo_eip(eip, &info) == -1) {
			cprintf("Error happened when reading symbol table\n");
		} else {
			cprintf("%s:%d: ", info.eip_file, info.eip_line);
			cprintf("%.*s", info.eip_fn_namelen, info.eip_fn_name);
			cprintf("+%d\n", eip - (uint32_t)info.eip_fn_addr);
		}

		ebp = (uint32_t)(*((uint32_t *)ebp));
		if (ebp == 0)
			break;
	}
	
	return 0;
}

int
mon_showmapping(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Error: please enter the begin and end point of the memory area you want to map\n");
		return -1;
	}

	uint32_t vbegin = ROUNDDOWN(atoi(argv[1]), PGSIZE);
	uint32_t vend = ROUNDDOWN(atoi(argv[2]), PGSIZE);

	if (vbegin > vend) {
		cprintf("Error: invalid arguments\n");
		return -1;
	}

	pde_t *pgdir = kern_pgdir;
	uintptr_t vaddr = vbegin;

	for (uint32_t i = vbegin / PGSIZE; i <= vend / PGSIZE; i++) {
		pte_t *pte = pgdir_walk(pgdir, (const void *)vaddr, 0);

		if (!pte) 
			cprintf("%p: Unmapped\n", (void *)vaddr);
		else {
			cprintf("%p: %p\n  Permission: \n", (void *)vaddr, (void *)PTE_ADDR(*pte));
			if (((*pte) & PTE_U) && ((*pte) & PTE_W))
				cprintf("    Kernel: RW-  User: RW-\n");
			else {
				if (((*pte) & PTE_W))
					cprintf("    Kernel: RW-  User: ---\n");
				else if (((*pte) & PTE_U))
					cprintf("    Kernel: R--  User: R--\n");
				else
					cprintf("    Kernel: ---  User: ---\n");
			}
		}
		vaddr += PGSIZE;
	}

	return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Error: please enter the address you want to set and the permission!\n");
		return -1;
	}

	uintptr_t vaddr = atoi(argv[1]);
	int perm = atoi(argv[2]);
	
	pde_t *pgdir = kern_pgdir;

	pte_t *pte = pgdir_walk(pgdir, (const void *)vaddr, 0);
	if (!pte) {
		cprintf("Error: set permission of unmapped page\n");
		return -1;
	}

	int oriperm = PGOFF(*pte);
	*pte = PTE_ADDR(*pte) | perm;
	cprintf("Successfully set permission of page address %p from %d to %d\n", PTE_ADDR(*pte), oriperm, perm);

	return 0;
}

int
mon_dumpmem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 4) {
		cprintf("Error: please enter the range of the address and whether it is virtual address\n");
		return -1;
	}

	bool virtual = atoi(argv[3]);

	if (virtual) {
		uintptr_t vbegin = atoi(argv[1]);
		uintptr_t vend = atoi(argv[2]);

		uint32_t *va = (uint32_t *)vbegin;
		pde_t *pgdir = kern_pgdir;

		for (; (uintptr_t) va <= vend;) {
			cprintf("debug: %p\n", va);
			pte_t *pte = pgdir_walk(pgdir, (const void *)ROUNDDOWN(va, PGSIZE), 0);
			if (!pte) {
				cprintf("%p - %p: Unmapped\n", ROUNDDOWN(va, PGSIZE), ROUNDUP(va, PGSIZE));
				va = ROUNDUP(va, PGSIZE);
				continue;
			}
			uint32_t *pa = (uint32_t *)(PTE_ADDR(*pte) | PGOFF(va));
			if ((uintptr_t) ROUNDUP(va, PGSIZE) <= vend) {
				uintptr_t temp = (uintptr_t) ROUNDUP(va, PGSIZE);
				for (; (uintptr_t) va < temp; va++, pa++) {
					cprintf("%p: %p\n", va, (void *)(*(uint32_t *)(KADDR((physaddr_t)pa))));
				}
			}
			else {
				for (; (uintptr_t) va <= vend; va++, pa++) {
					cprintf("%p: %p\n", va, (void *)(*(uint32_t *)(KADDR((physaddr_t)pa))));
				}
			}
		}
	}
	else {
		physaddr_t pbegin = atoi(argv[1]);
		physaddr_t pend = atoi(argv[2]);

		uint32_t *pa = (uint32_t *)pbegin;
		
		for (; (physaddr_t) pa <= pend; pa++) {
			cprintf("%p: %p\n", pa, (void *)(*(uint32_t *)(KADDR((physaddr_t)pa))));
		}
	}

	return 0;
}

int
mon_stepi(int argc, char **argv, struct Trapframe *tf)
{
	if (tf == NULL) {
		cprintf("No debugger running!\n");
		return 0;
	} 
	else {
		tf->tf_eflags |= (FL_TF);
		return -1;
	}
}

int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	if (tf == NULL) {
		cprintf("No debugger running\n");
		return 0;
	}
	else {
		tf->tf_eflags &= (~FL_TF);
		return -1;
	}
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
