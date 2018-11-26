/* vfree.c - vfree*/

#include <xinu.h>

void vfree(char *ptr, uint32 nbytes){
	intmask	mask;			/* Saved interrupt mask		*/
   uint32 start_page, end_page, zero = 0, npages = 0, curraddr;
   virt_addr_t virt;
   pdbr_t pdbr;
   pd_t *dir;
   pt_t *pt;
	struct	procent	*prptr;		/* Pointer to proc. table entry */
   int i;

	mask   = disable();

   kernel_mode_enter();
	prptr = &proctab[getpid()];

   start_page = (uint32)ptr / PAGE_SIZE;
   end_page   = ceil_div( (((uint32)ptr) + nbytes - 1), PAGE_SIZE);

	if (nbytes == 0){
		restore(mask);
		return;
	}

   pdbr           = prptr->pdbr;
   dir            = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

   for(i = start_page; i < end_page; i++){
      npages++;
      curraddr    = i * PAGE_SIZE;
      virt        = *((virt_addr_t*)(&curraddr));
      if( !dir[virt.pd_offset].pd_pres ){
         restore(mask);
         return;
      }

      pt                              = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);

      if( !pt[virt.pt_offset].pt_pres && pt[virt.pt_offset].pt_isswapped ){
         freeswapframe( pt[virt.pt_offset].pt_base );
      } else if( pt[virt.pt_offset].pt_pres ){
         if(!pt[virt.pt_offset].pt_isvmalloc){
            freeffsframe( pt[virt.pt_offset].pt_base );
         }
      } else{
         kprintf("Double free in vfree %08X %d %d %d %08X %d %d %d %08X %08X\n", pt[virt.pt_offset], i, start_page, end_page, ptr, nbytes, virt.pt_offset, virt.pd_offset, virt, pdbr);
         halt();
      }

      pt[virt.pt_offset]              = *((pt_t*)&zero);
   }

   prptr->vfree    += npages;
   n_free_vpages   += npages;

   kernel_mode_exit();
	restore(mask);
}
