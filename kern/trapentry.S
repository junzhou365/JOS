/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
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

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
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

TRAPHANDLER(divide_zero, T_DIVIDE);    		// divide error
TRAPHANDLER(debug, T_DEBUG);     		// debug exception
TRAPHANDLER(non_maskable, T_NMI);       		// non-maskable interrupt
TRAPHANDLER(breakpoint, T_BRKPT);     		// breakpoint
TRAPHANDLER(overflow, T_OFLOW);     		// overflow
TRAPHANDLER(bounds_check, T_BOUND);     		// bounds check
TRAPHANDLER(illegal_opcode, T_ILLOP);     		// illegal opcode
TRAPHANDLER(device_unavailable, T_DEVICE);    		// device not available
TRAPHANDLER(double_fault, T_DBLFLT);    		// double fault
TRAPHANDLER(invalid_tss, T_TSS);       		// invalid task switch segment

/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
    movw $GD_KD, %ax
    movw %ax, %ds
    movw %ax, %es

    /* Push all registers */
    pushal

    pushl %esp
    call trap

