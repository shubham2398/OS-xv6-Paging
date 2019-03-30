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
  cprintf("writing page to disk\n");
	write_page_to_disk(1, P2V(pa), bno);
  cprintf("hello1\n");
  cprintf("hello2\n");
  *pte = 0;
  *pte = bno<<12;
  *pte |= PTE_S;
  
  cprintf("page written successfully\n");

  asm volatile("invlpg (%0)"::"b"((unsigned long)(P2V(pa))):"memory");

  // lcr3(V2P(myproc()->pgdir));
  kfree(P2V(pa));
  cprintf("hello3\n");

  // char* v = P2V(KERNBASE);
	// asm volatile(" invlpg %0 ": : "r"(*pte) );
  // asm volatile("invlpg [%0]" :: "r" (*v));
}

/* Select a victim and swap the contents to the disk.
 */
int
swap_page(pde_t *pgdir)
{ 
  cprintf("Victim is going to be selected\n");
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
      panic("try while loop");
  }
  memset(mem, 0, PGSIZE);
  uint bno;
  if((bno = getswappedblk(pgdir, addr)) != -1){
    cprintf("yyayyyyyy\n");
    //panic("trying to swap page back");
    // pte_t *pte = walkpgdir(pgdir, &addr, 0);
    read_page_from_disk(1, mem, bno);
    bfree_page(1,bno);
  }
  // addr = PGROUNDDOWN((uint)addr);
  // cprintf("exec\n");
  if(mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
    panic("page table required");
    cprintf("addr in MA= %x\n", addr);
    swap_page(pgdir);
    // needed because top 10 bits of addr may point to some pde which does not exist
    if(mappages(pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
      panic("why do mappages require more page?");
    }
  }

//   cprintf("\nVA %d\n",addr);
//   pte_t *pte;
//   pte = walkpgdir(pgdir, (char*)addr, 1);
//   cprintf("pte value%d\n",*pte);
//   if((*pte & PTE_S)==1 && (*pte & PTE_P)==0)
//   {
//     //panic("pte is 1");
//     int blk = getswappedblk(pgdir, addr);
//     char buffer[PGSIZE]="";
//     cprintf("BLK %d\n", blk);
//     read_page_from_disk(ROOTDEV, buffer, blk);
//     char *kva;
//     kva = kalloc();
//     if(kva==0)  
//     {
//       int b = swap_page(pgdir);
//       if(b!=1)
//         panic("Problem in swapping");
//       kva = kalloc();
//       if(kva == 0)
//         panic("PROBLEM"); 
//       memmove(kva , buffer, PGSIZE);
//       *pte =  V2P(kva) | PTE_P | PTE_W | PTE_U ;
//       bfree_page(ROOTDEV, blk);
//     }
//     else
//     {
//       memmove(kva , buffer, PGSIZE);
//       *pte =  V2P(kva) | PTE_P | PTE_W | PTE_U ;
//       bfree_page(ROOTDEV, blk);
//       return ;
//     }
//   }
//   else
//   { 
//     char *kva;
//     kva = kalloc();
//     if(kva==0)  
//     {
//       int b = swap_page(pgdir);
//       if(b!=1)
//         panic("Sys_swap me gadbad");
//       kva = kalloc();
//       if(kva == 0)
//         panic("PROBLEM"); 
//       memset(kva,0,PGSIZE);
//       *pte =  V2P(kva) | PTE_P | PTE_W | PTE_U ;
//       cprintf("pte value%d\n",*pte);
//       cprintf("New entry set\n");
//     }
//     else
//     {
//     memset(kva,0,PGSIZE);
//     *pte = V2P(kva) | PTE_P | PTE_W | PTE_U ;
//     cprintf("pte value%d\n",*pte);
//     }
// }
}

/* page fault handler */
void
handle_pgfault()
{
  unsigned addr;
	struct proc *curproc = myproc();

	asm volatile ("movl %%cr2, %0 \n\t" : "=r" (addr));
  cprintf("addr = %x\n", addr);
  if(addr > KERNBASE)
    panic("wrong input");
	addr &= ~0xfff;
  cprintf("addr = %x\n", addr);
	map_address(curproc->pgdir, addr);
}
