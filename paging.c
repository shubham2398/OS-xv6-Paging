#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "paging.h"
#include "fs.h"

/* Allocate eight consecutive disk blocks.
 * Save the content of the physical page in the pte
 * to the disk blocks and save the block-id into the
 * pte.
 */

void
swap_page_from_pte(pte_t *pte)
{ 
  begin_op();
  uint bno = balloc_page(1);
  end_op();
	uint pa = PTE_ADDR(*pte);
  
  write_page_to_disk(1, P2V(pa), bno);
  *pte = 0;
  *pte = bno<<12;
  *pte |= PTE_S;
  
  asm volatile("invlpg (%0)"::"b"((uint)(P2V(pa))):"memory");
  kfree(P2V(pa));
}

/* Select a victim and swap the contents to the disk.
 */
int
swap_page(pde_t *pgdir)
{ 
  pte_t* pte= select_a_victim(pgdir);
  swap_page_from_pte(pte);
	return 1;
}

/* Map a physical page to the virtual address addr.
 * If the page table entry points to a swapped block
 * restore the content of the page from the swapped
 * block and free the swapped block.
 */

void
map_address(pde_t *pgdir, uint addr)
{
  char *mem;
  if(addr >= KERNBASE)
    panic("user not allowed");
  mem = kalloc();
  if(mem == 0) {
    swap_page(pgdir);
    mem = kalloc();
    if(mem == 0)
      panic("This is weird. Should not happen.\n");
  }
  memset(mem, 0, PGSIZE);
  uint bno;
  if((bno = getswappedblk(pgdir, addr)) != -1){
    read_page_from_disk(1, mem, bno);
    begin_op();
    bfree_page(1,bno);
    end_op();
  }
  
  if(mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
    swap_page(pgdir);
    if(mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
      panic("why do mappages require more page?");
    }
  }
}

/* page fault handler */
void
handle_pgfault()
{
  unsigned addr;
	struct proc *curproc = myproc();

	asm volatile ("movl %%cr2, %0 \n\t" : "=r" (addr));
  
  if(addr > KERNBASE)
    panic("wrong input");
	addr &= ~0xfff;
	map_address(curproc->pgdir, addr);
}
