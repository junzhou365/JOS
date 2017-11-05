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

TRAPHANDLER_NOEC(divide_zero, T_DIVIDE);    		// divide error
TRAPHANDLER_NOEC(debug, T_DEBUG);     		// debug exception
TRAPHANDLER_NOEC(non_maskable, T_NMI);       		// non-maskable interrupt
TRAPHANDLER_NOEC(breakpoint, T_BRKPT);     		// breakpoint
TRAPHANDLER_NOEC(overflow, T_OFLOW);     		// overflow
TRAPHANDLER_NOEC(bounds_check, T_BOUND);     		// bounds check
TRAPHANDLER_NOEC(illegal_opcode, T_ILLOP);     		// illegal opcode
TRAPHANDLER_NOEC(device_unavailable, T_DEVICE);    		// device not available
TRAPHANDLER(double_fault, T_DBLFLT);    		// double fault
// reserved 9
TRAPHANDLER(invalid_tss, T_TSS);       		// invalid task switch segment
TRAPHANDLER(segment_not_present, T_SEGNP);
TRAPHANDLER(stack_exception, T_STACK);
TRAPHANDLER(general_protection_fault, T_GPFLT);
TRAPHANDLER(page_fault, T_PGFLT);
// reserved 15
TRAPHANDLER_NOEC(floating_point_error, T_FPERR);
TRAPHANDLER(alignment_check, T_ALIGN);
TRAPHANDLER_NOEC(machine_check, T_MCHK);
TRAPHANDLER_NOEC(simd_fp_error, T_SIMDERR);
// reserved 20-31
// IRQs:
TRAPHANDLER_NOEC(irq_0, IRQ_OFFSET);
TRAPHANDLER_NOEC(irq_1, IRQ_OFFSET + 1);
TRAPHANDLER_NOEC(irq_2, IRQ_OFFSET + 2);
TRAPHANDLER_NOEC(irq_3, IRQ_OFFSET + 3);
TRAPHANDLER_NOEC(irq_4, IRQ_OFFSET + 4);
TRAPHANDLER_NOEC(irq_5, IRQ_OFFSET + 5);
TRAPHANDLER_NOEC(irq_6, IRQ_OFFSET + 6);
TRAPHANDLER_NOEC(irq_7, IRQ_OFFSET + 7);
TRAPHANDLER_NOEC(irq_8, IRQ_OFFSET + 8);
TRAPHANDLER_NOEC(irq_9, IRQ_OFFSET + 9);
TRAPHANDLER_NOEC(irq_10, IRQ_OFFSET + 10);
TRAPHANDLER_NOEC(irq_11, IRQ_OFFSET + 11);
TRAPHANDLER_NOEC(irq_12, IRQ_OFFSET + 12);
TRAPHANDLER_NOEC(irq_13, IRQ_OFFSET + 13);
TRAPHANDLER_NOEC(irq_14, IRQ_OFFSET + 14);
TRAPHANDLER_NOEC(irq_15, IRQ_OFFSET + 15);
// syscall
TRAPHANDLER_NOEC(syscall_handler, T_SYSCALL);


.data
.global vectors
vectors:
    .long divide_zero
    .long debug
    .long non_maskable
    .long breakpoint
    .long overflow
    .long bounds_check
    .long illegal_opcode
    .long device_unavailable
    .long double_fault
    .long 0 // reserved 9
    .long invalid_tss
    .long segment_not_present
    .long stack_exception
    .long general_protection_fault
    .long page_fault
    .long 0 // reserved 15
    .long floating_point_error
    .long alignment_check
    .long machine_check
    .long simd_fp_error
    .long 0 // reserved 20
    .long 0 // reserved 21
    .long 0 // reserved 22
    .long 0 // reserved 23
    .long 0 // reserved 24
    .long 0 // reserved 25
    .long 0 // reserved 26
    .long 0 // reserved 27
    .long 0 // reserved 28
    .long 0 // reserved 29
    .long 0 // reserved 30
    .long 0 // reserved 31
    .long irq_0
    .long irq_1
    .long irq_2
    .long irq_3
    .long irq_4
    .long irq_5
    .long irq_6
    .long irq_7
    .long irq_8
    .long irq_9
    .long irq_10
    .long irq_11
    .long irq_12
    .long irq_13
    .long irq_14
    .long irq_15

/*
 * Lab 3: Your code here for _alltraps
 */
.text
_alltraps:
    push %ds
    push %es
    /* Push all registers */
    pushal

    movw $GD_KD, %ax
    movw %ax, %ds
    movw %ax, %es

    pushl %esp

    call trap

