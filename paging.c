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
  uint bno = balloc_page(1);
	uint pa = PTE_ADDR(*pte);
	write_page_to_disk(1, P2V(pa), bno);
  *pte &= ~PTE_P;
	*pte = bno<<12 | PTE_S;
  kfree(P2V(pa));

  //char* v = P2V(KERNBASE);
	// asm volatile(" invlpg %0 ": : "r"(*pte) );
  //asm volatile("invlpg [%0]" :: "r" (*v));
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
  int cnt = 0;
  while(mem == 0) {
    cnt += 1;
    swap_page(pgdir);
    mem = kalloc();
    if(cnt > 3)
      panic("tumse na ho paayega");
  }

  memset(mem, 0, PGSIZE);
  int cnt2 = 0;
  
  while (mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
    cnt2 += 1;
    if(cnt2>15)
      panic("stop");
    swap_page(pgdir);

    // return;
  }

  uint bno;
  bno = getswappedblk(pgdir, addr);
  if(bno != -1){
    pte_t *pte = walkpgdir(pgdir, &addr, 0);
    read_page_from_disk(1, P2V(PTE_ADDR(*pte)), bno);
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
