/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
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

    TRAPHANDLER_NOEC(traphandler_divide, T_DIVIDE)
    TRAPHANDLER_NOEC(traphandler_debug, T_DEBUG)
    TRAPHANDLER_NOEC(traphandler_nmi, T_NMI)
    TRAPHANDLER_NOEC(traphandler_brkpt, T_BRKPT)
    TRAPHANDLER_NOEC(traphandler_oflow, T_OFLOW)
    TRAPHANDLER_NOEC(traphandler_bound, T_BOUND)
    TRAPHANDLER_NOEC(traphandler_illop, T_ILLOP)
    TRAPHANDLER_NOEC(traphandler_device, T_DEVICE)

    TRAPHANDLER(traphandler_dblflt, T_DBLFLT)
    TRAPHANDLER(traphandler_tss, T_TSS)
    TRAPHANDLER(traphandler_segnp, T_SEGNP)
    TRAPHANDLER(traphandler_stack, T_STACK)
    TRAPHANDLER(traphandler_gpflt, T_GPFLT)
    TRAPHANDLER(traphandler_pgflt, T_PGFLT)

    TRAPHANDLER_NOEC(traphandler_fperr, T_FPERR)
    TRAPHANDLER(traphandler_align, T_ALIGN)
    TRAPHANDLER_NOEC(traphandler_mchk, T_MCHK)
    TRAPHANDLER_NOEC(traphandler_simderr, T_SIMDERR)
	
    TRAPHANDLER_NOEC(traphandler_syscall, T_SYSCALL)

    TRAPHANDLER_NOEC(irqhandler_irq0, IRQ_OFFSET + 0);
    TRAPHANDLER_NOEC(irqhandler_irq1, IRQ_OFFSET + 1);
    TRAPHANDLER_NOEC(irqhandler_irq2, IRQ_OFFSET + 2);
    TRAPHANDLER_NOEC(irqhandler_irq3, IRQ_OFFSET + 3);
    TRAPHANDLER_NOEC(irqhandler_irq4, IRQ_OFFSET + 4);
    TRAPHANDLER_NOEC(irqhandler_irq5, IRQ_OFFSET + 5);
    TRAPHANDLER_NOEC(irqhandler_irq6, IRQ_OFFSET + 6);
    TRAPHANDLER_NOEC(irqhandler_irq7, IRQ_OFFSET + 7);
    TRAPHANDLER_NOEC(irqhandler_irq8, IRQ_OFFSET + 8);
    TRAPHANDLER_NOEC(irqhandler_irq9, IRQ_OFFSET + 9);
    TRAPHANDLER_NOEC(irqhandler_irq10, IRQ_OFFSET + 10);
    TRAPHANDLER_NOEC(irqhandler_irq11, IRQ_OFFSET + 11);
    TRAPHANDLER_NOEC(irqhandler_irq12, IRQ_OFFSET + 12);
    TRAPHANDLER_NOEC(irqhandler_irq13, IRQ_OFFSET + 13);
    TRAPHANDLER_NOEC(irqhandler_irq14, IRQ_OFFSET + 14);
    TRAPHANDLER_NOEC(irqhandler_irq15, IRQ_OFFSET + 15);

/*
 * Lab 3: Your code here for _alltraps
 */
	
_alltraps:
    pushw   $0
    pushw   %ds
    pushw   $0
    pushw   %es
    pushal

    movl    $GD_KD, %eax
    movw    %ax, %ds
    movw    %ax, %es

    pushl   %esp
    
    call    trap
