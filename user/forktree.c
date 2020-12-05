// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

#define DEPTH 3

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH+1];

	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);

	// for debug
	//cprintf("this is %04x ready to fork\n", sys_getenvid());

	if (sfork() == 0) {
		// for debug
		//cprintf("this is %04x ready to forktree and exit\n", sys_getenvid());
		forktree(nxt);
		exit();
	}
}

void
forktree(const char *cur)
{
	cprintf("%04x: I am '%s'\n", sys_getenvid(), cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

void
umain(int argc, char **argv)
{
	forktree("");
	// for debug
	//cprintf("this is %04x ready to exit\n", sys_getenvid());
}

