// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/trap.h>
#include <kern/kdebug.h>
#include<kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


unsigned read_eip() __attribute__((noinline));

int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_alloc_page(int argc, char **argv, struct Trapframe *tf);
int mon_free_page(int argc, char **argv, struct Trapframe *tf);
int mon_page_status(int argc, char **argv, struct Trapframe *tf);

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a listing of function call frames", mon_backtrace },
	{ "showmappings", "Display physical mappings and corresponding info", mon_showmappings },
	{ "alloc_page", "Allocate pages explicitly", mon_alloc_page },
	{ "free_page", "Free pages explicitly", mon_free_page },
	{ "page_status", "Display status of any given page of physical memory", mon_page_status },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%C%s %C- %C%s\n%C", COLOR_GRN, commands[i].name, COLOR_CYN, COLOR_YLW, commands[i].desc, COLOR_CYN);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("%CSpecial kernel symbols:\n%C", COLOR_GRN, COLOR_YLW);
	cprintf("  _start %08x (virt)  %08x (phys)\n", _start, _start - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("%CKernel executable memory footprint: %dKB\n%C",
		COLOR_GRN, (end-_start+1023)/1024, COLOR_CYN);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    uint32_t ebp, eip;
    struct Eipdebuginfo info;
    char func_name[36];
    eip = read_eip();
    ebp = read_ebp();
    cprintf("%CStack backtrace:\n", COLOR_GRN);
    while(ebp) {
        debuginfo_eip(eip, &info);
        strncpy(func_name, info.eip_fn_name, info.eip_fn_namelen);
        func_name[info.eip_fn_namelen] = '\0';
        cprintf("%C%s:%d: %s+%x\n", COLOR_YLW, info.eip_file, info.eip_line, func_name, eip - info.eip_fn_addr);
        cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n%C", ebp, *((uint32_t *)ebp + 1), 
            *((uint32_t *)ebp + 2), *((uint32_t *)ebp + 3), *((uint32_t *)ebp + 4), *((uint32_t *)ebp + 5), *((uint32_t *)ebp + 6), COLOR_CYN);
        eip = *((uint32_t *)ebp + 1);
        ebp = *(uint32_t *)ebp;
    }
	return 0;
}

int 
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
    if (argc == 3) {
        uint32_t lva = ROUNDDOWN(strtol(argv[1], 0, 0), PGSIZE);
        uint32_t hva = ROUNDUP(strtol(argv[2], 0, 0), PGSIZE);
        if (hva >= lva) {
            uint32_t i;
            pte_t *pte;
            for (i = lva; i <= hva; i += PGSIZE)
            {
                pte = pgdir_walk(boot_pgdir, (void *)i, 0);
                cprintf("%C0x%x %C- %C0x%x    ", COLOR_GRN, i, COLOR_CYN, COLOR_GRN, i + PGSIZE);
                if (pte != NULL && (*pte & PTE_P)) {
                    cprintf("%Cmapped %C0x%x  ", COLOR_YLW, COLOR_PUR, PTE_ADDR(*pte));
                    if (*pte & PTE_U)
                        cprintf ("%Cuser: ", COLOR_BLK);
                    else
                        cprintf ("%Ckernel: ", COLOR_BLK);
                    if (*pte & PTE_W)
                        cprintf ("%Cread/write", COLOR_PUR);
                    else
                        cprintf ("%Cread only", COLOR_PUR);
                    cprintf("\n%C", COLOR_CYN);
                }
                else {
                    cprintf("%Cnot mapped\n%C", COLOR_RED, COLOR_CYN);
                }
            }
        }
        else
            cprintf("%Cshowmappings: Invalid address\n%C", COLOR_RED, COLOR_CYN);
    }
    else
        cprintf("%CUsage: showmappings LOWER_VIRTUAL_ADDR HIGHER_VIRTUAL_ADDR\n%C", COLOR_GRN, COLOR_CYN);
    return 0;
}

int 
mon_alloc_page(int argc, char **argv, struct Trapframe *tf)
{
    struct Page *page;
    if (page_alloc(&page) == 0) {
        page->pp_ref += 1;
        cprintf("    %C0x%x\n%C", COLOR_GRN, page2pa(page), COLOR_CYN);
    }
    else
        cprintf("    %CAllocate failed!\n%C", COLOR_RED, COLOR_CYN);
    return 0;
}

int 
mon_free_page(int argc, char **argv, struct Trapframe *tf)
{
    if (argc == 2) {
        struct Page *page = pa2page(strtol(argv[1], 0, 0));
        if (page->pp_ref == 1) {
            page_decref(page);
            cprintf("    %CFree successfully!\n%C", COLOR_GRN, COLOR_CYN);
        }
        else 
            cprintf("    %CFree failed!\n%C", COLOR_RED, COLOR_CYN);
    }
    else 
        cprintf("%CUsage: free_page PHYSIC_ADDR\n%C", COLOR_GRN, COLOR_CYN);
    return 0;
}

int 
mon_page_status(int argc, char **argv, struct Trapframe *tf)
{
    if (argc == 2) {
        struct Page *page = pa2page(strtol(argv[1], 0, 0));
        if (page->pp_ref > 0) 
            cprintf("    %Callocated\n%C", COLOR_GRN, COLOR_CYN);
        else
            cprintf("    %Cfree\n%C", COLOR_GRN, COLOR_CYN);
    }
    else
        cprintf("%CUsage: page_status PHYSIC_ADDR\n%C", COLOR_GRN, COLOR_CYN);
    return 0;
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
			cprintf("%CToo many arguments (max %d)\n%C", COLOR_RED, MAXARGS, COLOR_CYN);
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
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("%CUnknown command '%s'\n%C", COLOR_RED, argv[0], COLOR_CYN);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("%CWelcome to the JOS kernel monitor!\n", COLOR_PUR);
	cprintf("%CType 'help' for a list of commands.\n%C", COLOR_BLK, COLOR_CYN);

    /*
     *if (tf != NULL) {
     *    print_trapframe(tf);
     *}
     */

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
