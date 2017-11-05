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
    pte_t entry = uvpt[PGNUM((uintptr_t)addr)];
    if (!((err & FEC_WR) == FEC_WR && (entry & PTE_COW) == PTE_COW))
        panic("Incorrect permission for page fault. err: %d, addr: %x, entry: %x, permission: %x, esp: %x, eip: %x\n",
                err, addr, entry, (entry & PTE_COW), utf->utf_esp, utf->utf_eip);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    uintptr_t *taddr = (uintptr_t *)PFTEMP;
    if ((r = sys_page_alloc(0, taddr, PTE_W | PTE_U | PTE_P)) < 0)
        panic("sys_page_alloc: %e", r);

    memcpy(taddr, ROUNDDOWN(addr, PGSIZE), PGSIZE);

    if ((r = sys_page_map(0, taddr, 0, ROUNDDOWN(addr, PGSIZE), PTE_W | PTE_U | PTE_P)) < 0)
        panic("sys_page_map: %e", r);

    if ((r = sys_page_unmap(0, taddr)) < 0)
        panic("sys_page_unmap: %e", r);
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
    pte_t entry = uvpt[pn];
    bool require_cow = (entry & PTE_W) == PTE_W || (entry & PTE_COW) == PTE_COW;
    int perm = PTE_U | PTE_P;
    if (require_cow)
        perm |= PTE_COW;

    uintptr_t *addr = (uintptr_t*)(pn * PGSIZE);
    if ((r = sys_page_map(0, addr, envid, addr, perm)) < 0)
        panic("sys_page_map: %e", r);

    // make the addr copy-on-write again
    // The reason of this ordering is that if we remapped the page that is
    // being visited to COW, as stack grows, a new writable page will be
    // remapped to the addr again, which later makes the child's addr mapped to
    // that writable page. The child lost context until next duppage call on
    // the same page.
    //
    // The reason we need to mark it COW again is that if the parent's stack pointer
    // reaches to the COW page before mapping into child, the page becomes writable.
    // The child's addr is mapped to a writable page again.
    if (require_cow && (r = sys_page_map(0, addr, 0, addr, perm)) < 0)
        panic("sys_page_map: %e", r);

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
    int r;

    set_pgfault_handler(pgfault);
    envid_t id = sys_exofork();

    if (id < 0)
        panic("sys_exofork: %e", id);
    else if (id == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // parent
    for (uintptr_t p = UTEXT; p < USTACKTOP; p += PGSIZE) {
        if ((uvpd[PDX(p)] & PTE_P) == PTE_P && (uvpt[PGNUM(p)] & PTE_P) == PTE_P) {
            if ((r = duppage(id, PGNUM(p))) < 0)
                panic("duppage: %e", r);
        }
    }

    if ((r = sys_env_set_pgfault_upcall(id, thisenv->env_pgfault_upcall)) < 0)
        panic("sys_env_set_pgfault_upcall: %e", r);
    if ((r = sys_page_alloc(id, (uintptr_t *)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P)) < 0)
        panic("sys_page_alloc for env %d: %e", id, r);


    if ((r = sys_env_set_status(id, ENV_RUNNABLE)) < 0)
        panic("sys_env_set_status: %e", r);

    return id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
