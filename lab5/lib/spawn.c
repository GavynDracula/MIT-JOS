#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)	((void*) (addr) + (USTACKTOP - PGSIZE) - UTEMP)
#define UTEMP2			(UTEMP + PGSIZE)
#define UTEMP3			(UTEMP2 + PGSIZE)

// Helper functions for spawn.
static int init_stack(envid_t child, const char **argv, uintptr_t *init_esp);

// Spawn a child process from a program image loaded from the file system.
// prog: the pathname of the program to run.
// argv: pointer to null-terminated array of pointers to strings,
// 	 which will be passed to the child as its command-line arguments.
// Returns child envid on success, < 0 on failure.
int
spawn(const char *prog, const char **argv)
{
    int r;
    int fdnum;
	unsigned char elf_buf[512];
	struct Trapframe child_tf;
	envid_t child;
	(void) child;
    struct Elf *ELFHDR;
    struct Proghdr *ph, *eph;

	// Insert your code, following approximately this procedure:
	//
	//   - Open the program file.
	//
	//   - Read the ELF header, as you have before, and sanity check its
	//     magic number.  (Check out your load_icode!)
	//
	//   - Use sys_exofork() to create a new environment.
	//
	//   - Set child_tf to an initial struct Trapframe for the child.
	//     Hint: The sys_exofork() system call has already created
	//     a good basis, in envs[ENVX(child)].env_tf.
	//     Hint: You must do something with the program's entry point.
	//     What?  (See load_icode!)
	//
	//   - Call the init_stack() function above to set up
	//     the initial stack page for the child environment.
	//
	//   - Map all of the program's segments that are of p_type
	//     ELF_PROG_LOAD into the new environment's address space.
	//     Use the p_flags field in the Proghdr for each segment
	//     to determine how to map the segment:
	//
	//	* If the ELF flags do not include ELF_PROG_FLAG_WRITE,
	//	  then the segment contains text and read-only data.
	//	  Use read_map() to read the contents of this segment,
	//	  and map the pages it returns directly into the child
	//        so that multiple instances of the same program
	//	  will share the same copy of the program text.
	//        Be sure to map the program text read-only in the child.
	//        Read_map is like read but returns a pointer to the data in
	//        *blk rather than copying the data into another buffer.
	//
	//	* If the ELF segment flags DO include ELF_PROG_FLAG_WRITE,
	//	  then the segment contains read/write data and bss.
	//	  As with load_icode() in Lab 3, such an ELF segment
	//	  occupies p_memsz bytes in memory, but only the FIRST
	//	  p_filesz bytes of the segment are actually loaded
	//	  from the executable file - you must clear the rest to zero.
	//        For each page to be mapped for a read/write segment,
	//        allocate a page in the parent temporarily at UTEMP,
	//        read() the appropriate portion of the file into that page
	//	  and/or use memset() to zero non-loaded portions.
	//	  (You can avoid calling memset(), if you like, if
	//	  page_alloc() returns zeroed pages already.)
	//        Then insert the page mapping into the child.
	//        Look at init_stack() for inspiration.
	//        Be sure you understand why you can't use read_map() here.
	//
	//     Note: None of the segment addresses or lengths above
	//     are guaranteed to be page-aligned, so you must deal with
	//     these non-page-aligned values appropriately.
	//     The ELF linker does, however, guarantee that no two segments
	//     will overlap on the same page; and it guarantees that
	//     PGOFF(ph->p_offset) == PGOFF(ph->p_va).
	//
	//   - Call sys_env_set_trapframe(child, &child_tf) to set up the
	//     correct initial eip and esp values in the child.
	//
	//   - Start the child process running with sys_env_set_status().

	// LAB 5: Your code here.
	
    fdnum = open(prog, O_RDWR);
    if (fdnum < 0)
        return fdnum;
    read(fdnum, elf_buf, 512);
    ELFHDR = (struct Elf *)elf_buf;
    if (ELFHDR->e_magic != ELF_MAGIC) {
        panic("load_icode: Give binary is not a valid ELF.\n");
        return -E_INVAL;
    }
    child = sys_exofork();
    if (child < 0)
        return child;
    child_tf =  envs[ENVX(child)].env_tf;
    child_tf.tf_eip = ELFHDR->e_entry;
    r = init_stack(child, argv, &child_tf.tf_esp);
    if (r < 0)
        return r;
    ph = (struct Proghdr *)((uint8_t *) ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph++) {
        if (ph->p_type == ELF_PROG_LOAD) {
            uintptr_t start = ROUNDDOWN(ph->p_offset, PGSIZE);
            uintptr_t end;
            uintptr_t va = ROUNDDOWN(ph->p_va, PGSIZE);
            int i;
            void *blk;
            if ((ph->p_flags & ELF_PROG_FLAG_WRITE) == 0) {
                // Text
                end = ROUNDUP(ph->p_filesz + ph->p_offset, PGSIZE);
                for (i = start; i < end; i += PGSIZE) {
                    r = read_map(fdnum, i, &blk);
                    if (r < 0)
                        return r;
                    r = sys_page_map(0, blk, child, (void *)(va + i - start), PTE_U | PTE_P);
                    if (r < 0)
                        return r;
                }
            }
            else {
                // Bss and Date
                uintptr_t limit = ph->p_offset + ph->p_filesz;
                end = ROUNDUP(ph->p_memsz + ph->p_offset, PGSIZE);
                seek(fdnum, ph->p_offset);
                for (i = start; i < end; i += PGSIZE) {
                    r = sys_page_alloc(0, UTEMP, PTE_U | PTE_W | PTE_P);
                    if (r < 0)
                        return r;
                    memset(UTEMP, 0, PGSIZE);
                    if (i < limit) {
                        // Data
                        r = read(fdnum, UTEMP, PGSIZE);
                        if (r < 0)
                            return r;
                        if (i == ROUNDDOWN(limit, PGSIZE))
                            memset(UTEMP + limit - i, 0, PGSIZE - (limit - i));
                    }
                    r = sys_page_map(0, UTEMP, child, (void *)(va + i - start), PTE_U | PTE_W | PTE_P);
                    if (r < 0)
                        return r;
                    r = sys_page_unmap(0, UTEMP);
                    if (r < 0)
                        return r;
                }
            }
        }
    }
    r = sys_env_set_trapframe(child, &child_tf);
    if (r < 0)
        return r;
    r = sys_env_set_status(child, ENV_RUNNABLE);
    if (r < 0)
        return r;
    return child;
}

// Spawn, taking command-line arguments array directly on the stack.
int
spawnl(const char *prog, const char *arg0, ...)
{
	return spawn(prog, &arg0);
}


// Set up the initial stack page for the new child process with envid 'child'
// using the arguments array pointed to by 'argv',
// which is a null-terminated array of pointers to null-terminated strings.
//
// On success, returns 0 and sets *init_esp
// to the initial stack pointer with which the child should start.
// Returns < 0 on failure.
static int
init_stack(envid_t child, const char **argv, uintptr_t *init_esp)
{
	size_t string_size;
	int argc, i, r;
	char *string_store;
	uintptr_t *argv_store;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (string_size).
	string_size = 0;
	for (argc = 0; argv[argc] != 0; argc++)
		string_size += strlen(argv[argc]) + 1;

	// Determine where to place the strings and the argv array.
	// Set up pointers into the temporary page 'UTEMP'; we'll map a page
	// there later, then remap that page into the child environment
	// at (USTACKTOP - PGSIZE).
	// strings is the topmost thing on the stack.
	string_store = (char*) UTEMP + PGSIZE - string_size;
	// argv is below that.  There's one argument pointer per argument, plus
	// a null pointer.
	argv_store = (uintptr_t*) (ROUNDDOWN(string_store, 4) - 4 * (argc + 1));
	
	// Make sure that argv, strings, and the 2 words that hold 'argc'
	// and 'argv' themselves will all fit in a single stack page.
	if ((void*) (argv_store - 2) < (void*) UTEMP)
		return -E_NO_MEM;

	// Allocate the single stack page at UTEMP.
	if ((r = sys_page_alloc(0, (void*) UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		return r;

	// Replace this with your code to:
	//
	//	* Initialize 'argv_store[i]' to point to argument string i,
	//	  for all 0 <= i < argc.
	//	  Also, copy the argument strings from 'argv' into the
	//	  newly-allocated stack page.
	//	  Hint: Copy the argument strings into string_store.
	//	  Hint: Make sure that argv_store uses addresses valid in the
	//	  CHILD'S environment!  The string_store variable itself
	//	  points into page UTEMP, but the child environment will have
	//	  this page mapped at USTACKTOP - PGSIZE.  Check out the
	//	  UTEMP2USTACK macro defined above.
	//
	//	* Set 'argv_store[argc]' to 0 to null-terminate the args array.
	//
	//	* Push two more words onto the child's stack below 'args',
	//	  containing the argc and argv parameters to be passed
	//	  to the child's umain() function.
	//	  argv should be below argc on the stack.
	//	  (Again, argv should use an address valid in the child's
	//	  environment.)
	//
	//	* Set *init_esp to the initial stack pointer for the child,
	//	  (Again, use an address valid in the child's environment.)
	//
	// LAB 5: Your code here.

    for (i = 0; i < argc; i++) {
        argv_store[i] = UTEMP2USTACK(string_store);
        strcpy(string_store, argv[i]);
        string_store += strlen(argv[i]) + 1;
    }
    argv_store[argc] = 0;
    *(argv_store - 1) = UTEMP2USTACK(argv_store);
    *(argv_store - 2) = argc;
	*init_esp = UTEMP2USTACK(argv_store - 2);

	// After completing the stack, map it into the child's address space
	// and unmap it from ours!
	if ((r = sys_page_map(0, UTEMP, child, (void*) (USTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		goto error;
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		goto error;

	return 0;

error:
	sys_page_unmap(0, UTEMP);
	return r;
}



