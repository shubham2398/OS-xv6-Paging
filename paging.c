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
{	uint bno = balloc_page(1);
	uint pa = PTE_ADDR(*pte);
	*pte &= ~PTE_P;
	write_page_to_disk(1,P2V(pa),bno);
	*pte = bno<<12 | PTE_S;
	pa = P2V(pa);
	//asm volatile ("invlpgl %0 \n\t" : : :"=r");
}

pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
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
	char *mem = kalloc();
    memset(mem, 0, PGSIZE);
    int status = mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U);
    if(status == -2){
      swap_page(pgdir);
      pte_t *pte = walkpgdir(pgdir, &addr, 1);
      if(*pte==0) panic("fuck");
      if((*pte) & PTE_S){
		uint bno = (*pte)>>12;
		read_page_from_disk(1,P2V(PTE_ADDR(*pte)),bno);
		}
      // panic("swap error");
    } else if(status<0) {
      // printf("allocuvm out of memory (2)\n");
    panic("bla");
      kfree(mem);
      panic("allocation = 0 in walk pgdir");
    }
}

/* page fault handler */
void
handle_pgfault()
{
	unsigned addr;
	struct proc *curproc = myproc();

	asm volatile ("movl %%cr2, %0 \n\t" : "=r" (addr));
	addr &= ~0xfff;
	map_address(curproc->pgdir, addr);
}
