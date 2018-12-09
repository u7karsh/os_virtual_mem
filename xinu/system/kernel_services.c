#include <xinu.h>

/*------------------------------------------------------------------------
 *  vmalloc -  Allocate heap storage, returning lowest word address
 *------------------------------------------------------------------------
 */
void kernel_service_malloc(uint32 nbytes, bool8 is_stack, pid32 pid){
	intmask	mask;			/* Saved interrupt mask		*/
   uint32 npages, vaddr;
   virt_addr_t virt;
   pdbr_t pdbr;
   pd_t *dir;
   pt_t *pt;
	struct procent *prptr;
   int i;

	mask = disable();

   prptr  = &proctab[pid];
   npages = ceil_div( nbytes, PAGE_SIZE );

	ASSERT(!(nbytes == 0 || (!is_stack && (npages > prptr->vfree))), "kernel_service_malloc\n");

	vaddr  = prptr->vmax << PAGE_OFFSET_BITS;

   pdbr   = prptr->pdbr;
   dir    = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

   for(i = 0; i < npages; i++){
      virt        = *((virt_addr_t*)&vaddr);
      if( !dir[virt.pd_offset].pd_pres ){
         create_directory_entry(&dir[virt.pd_offset], -1, -1, 0, 0);
      }

      pt                                     = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);
      pt[virt.pt_offset].pt_pres	            = is_stack;	/* page is present?		*/
      pt[virt.pt_offset].pt_write            = 1;	/* page is writable?		*/
      pt[virt.pt_offset].pt_user	            = 0;	/* is use level protection?	*/
      pt[virt.pt_offset].pt_pwt	            = 0;	/* write through for this page? */
      pt[virt.pt_offset].pt_pcd	            = 1;	/* cache disable for this page? */
      pt[virt.pt_offset].pt_acc	            = 0;	/* page was accessed?		*/
      pt[virt.pt_offset].pt_dirty            = 0;	/* page was written?		*/
      pt[virt.pt_offset].pt_mbz	            = 0;	/* must be zero			*/
      pt[virt.pt_offset].pt_global           = 0;	/* should be zero in 586	*/
      pt[virt.pt_offset].pt_isvmalloc        = 1;	/* for programmer's use		*/
      pt[virt.pt_offset].pt_isswapped        = 0;	/* for programmer's use		*/
      pt[virt.pt_offset].pt_already_swapped  = 0;	/* for programmer's use		*/
      pt[virt.pt_offset].pt_base             = is_stack ? getvstackframe() : 0;

      vaddr                          += PAGE_SIZE;
   }

   prptr->vmax   += npages;

   if( !is_stack ){
      prptr->vfree  -= npages;
      n_free_vpages -= npages;
   }

	restore(mask);
}

void free_vpage(pd_t *dir, uint32 i, bool8 nofail){
   virt_addr_t virt;
   uint32 curraddr, pd_base, ffsframe, maxpdptframe, maxffsframe, zero = 0;
   pt_t *pt;

   maxpdptframe  = ceil_div( ((uint32)maxpdpt), PAGE_SIZE );
   maxffsframe   = ceil_div( ((uint32)maxffs), PAGE_SIZE );

   curraddr    = i * PAGE_SIZE;
   virt        = *((virt_addr_t*)(&curraddr));
   ASSERT( dir[virt.pd_offset].pd_pres, "pd_pres in free_vpage\n");

   pd_base = dir[virt.pd_offset].pd_base;
   pt      = (pt_t*)(pd_base << PAGE_OFFSET_BITS);

   if( pt[virt.pt_offset].pt_pres ){
      ASSERT(!pt[virt.pt_offset].pt_isvmalloc, "Illegal value of isvmalloc");
      ffsframe   = pt[virt.pt_offset].pt_base;
      freeffsframe( ffsframe );
      // As an ffs frame is being freed, we should clear the mapping of this page
      // to page table
      ptmap[ffsframe-maxpdptframe] = NULL;
      if(pt[virt.pt_offset].pt_already_swapped){
         // There is an entry in swap that needs to be freed
         ASSERT( ffs2swapmap[ffsframe-maxpdptframe] != -1, "Illegal ffs2swapmap mapping in vfree\n" );
         freeswapframe( ffs2swapmap[ffsframe-maxpdptframe] );
         swap2ffsmap[ffs2swapmap[ffsframe-maxpdptframe] - maxffsframe] = NULL;
      }
      ffs2swapmap[ffsframe-maxpdptframe] = -1;
   } else if( pt[virt.pt_offset].pt_isswapped ){
      ASSERT( pt[virt.pt_offset].pt_already_swapped, "Illegal state of pt_already_swapped with pt_isswapped in vfree" );
      ASSERT( swap2ffsmap[pt[virt.pt_offset].pt_base - maxffsframe] == NULL, "Non null mapping in swap2ffsmap in vfree\n" );
      freeswapframe( pt[virt.pt_offset].pt_base );
      swap2ffsmap[pt[virt.pt_offset].pt_base - maxffsframe] = NULL;
   } else if(!nofail){
      ASSERT(FALSE, "Double free in kernel_service_free %08X %d %d %d %08X\n", pt[virt.pt_offset], i, virt.pt_offset, virt.pd_offset, virt);
   }
   pt[virt.pt_offset]              = *((pt_t*)&zero);
}

void kernel_service_free(char *ptr, uint32 nbytes, pid32 pid){
	intmask	mask;			/* Saved interrupt mask		*/
   uint32 start_page, end_page, npages = 0;
   pdbr_t pdbr;
   pd_t *dir;
	struct	procent	*prptr;		/* Pointer to proc. table entry */
   int i;

	mask   = disable();

	prptr = &proctab[pid];

   start_page = (uint32)ptr / PAGE_SIZE;
   end_page   = ceil_div( (((uint32)ptr) + nbytes - 1), PAGE_SIZE);

	if (nbytes == 0){
      kprintf("SYSERR: kernel_service_free\n");
      halt();
	}

   pdbr          = prptr->pdbr;
   dir           = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

   for(i = start_page; i < end_page; i++){
      npages++;
      free_vpage(dir, i, TRUE);
   }

   prptr->vfree    += npages;
   n_free_vpages   += npages;
   ASSERT( n_free_vpages >= 0 && n_free_vpages <= MAX_HEAP_SIZE, "Illegal value of n_free_vpages (=%d)\n", n_free_vpages );

	restore(mask);
}
