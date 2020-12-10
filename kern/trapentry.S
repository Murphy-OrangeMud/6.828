/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You should not call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU does not push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide, 0);
TRAPHANDLER_NOEC(debug, 1);
TRAPHANDLER(nmi, 2);
TRAPHANDLER_NOEC(breakp, 3);
TRAPHANDLER_NOEC(overflow, 4);
TRAPHANDLER_NOEC(boundcheck, 5);
TRAPHANDLER_NOEC(illegalop, 6);
TRAPHANDLER_NOEC(device, 7);
TRAPHANDLER(doublefault, 8);
TRAPHANDLER(tss, 10);
TRAPHANDLER(segnotpre, 11);
TRAPHANDLER(stackexp, 12);
TRAPHANDLER(genprotect, 13);
TRAPHANDLER(pagefault, 14);
TRAPHANDLER_NOEC(floating, 16);
TRAPHANDLER(aligncheck, 17);
TRAPHANDLER_NOEC(machinecheck, 18);
TRAPHANDLER_NOEC(simderror, 19);
TRAPHANDLER_NOEC(systemcall, 0x30);

TRAPHANDLER_NOEC(hardware_handler_0, 32);
TRAPHANDLER_NOEC(hardware_handler_1, 33);
TRAPHANDLER_NOEC(hardware_handler_2, 34);
TRAPHANDLER_NOEC(hardware_handler_3, 35);
TRAPHANDLER_NOEC(hardware_handler_4, 36);
TRAPHANDLER_NOEC(hardware_handler_5, 37);
TRAPHANDLER_NOEC(hardware_handler_6, 38);
TRAPHANDLER_NOEC(hardware_handler_7, 39);
TRAPHANDLER_NOEC(hardware_handler_8, 40);
TRAPHANDLER_NOEC(hardware_handler_9, 41);
TRAPHANDLER_NOEC(hardware_handler_10, 42);
TRAPHANDLER_NOEC(hardware_handler_11, 43);
TRAPHANDLER_NOEC(hardware_handler_12, 44);
TRAPHANDLER_NOEC(hardware_handler_13, 45);
TRAPHANDLER_NOEC(hardware_handler_14, 46);
TRAPHANDLER_NOEC(hardware_handler_15, 47);

/*
 * Lab 3: Your code here for _alltraps
 */
/*
	Your _alltraps should:
	push values to make the stack look like a struct Trapframe
	load GD_KD into %ds and %es
	pushl %esp to pass a pointer to the Trapframe as an argument to trap()
	call trap (can trap ever return?)
*/
_alltraps:
	/* build trap frame */
	pushl %ds
	pushl %es
	pushal

	/* load GD_KD into %ds and %es */
	movw $(GD_KD), %ax
	movw %ax, %ds
	movw %ax, %es

	/* push %esp */
	pushl %esp
	call trap 
