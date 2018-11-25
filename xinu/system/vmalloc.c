/* vmalloc.c - vmalloc*/

#include <xinu.h>

/*------------------------------------------------------------------------
 *  vmalloc -  Allocate heap storage, returning lowest word address
 *------------------------------------------------------------------------
 */
char  	*vmalloc(
	  uint32	nbytes		/* Size of memory requested	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
   uint32 npages, vaddr, cr3;
   virt_addr_t virt;
   pdbr_t pdbr;
   pd_t *dir;
   pt_t *pt;
	struct procent *prptr;
   int i;

	mask = disable();

   prptr  = &proctab[getpid()];
   npages = ceil_div( nbytes, PAGE_SIZE );

	if (nbytes == 0 || npages > prptr->vfree){
		restore(mask);
		return (char *)SYSERR;
	}

	vaddr          = prptr->vmax << PAGE_OFFSET_BITS;

   cr3            = read_cr3();
   pdbr           = *((pdbr_t*)&cr3);
   dir            = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

   for(i = 0; i < npages; i++){
      virt        = *((virt_addr_t*)&vaddr);
      if( !dir[virt.pd_offset].pd_pres ){
         create_directory_entry(&dir[virt.pd_offset], -1, -1, 0, 0);
      }

      pt                              = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);
      pt[virt.pt_offset].pt_pres	     = 0;	/* page is present?		*/
      pt[virt.pt_offset].pt_write     = 1;	/* page is writable?		*/
      pt[virt.pt_offset].pt_user	     = 0;	/* is use level protection?	*/
      pt[virt.pt_offset].pt_pwt	     = 0;	/* write through for this page? */
      pt[virt.pt_offset].pt_pcd	     = 1;	/* cache disable for this page? */
      pt[virt.pt_offset].pt_acc	     = 0;	/* page was accessed?		*/
      pt[virt.pt_offset].pt_dirty     = 0;	/* page was written?		*/
      pt[virt.pt_offset].pt_mbz	     = 0;	/* must be zero			*/
      pt[virt.pt_offset].pt_global    = 0;	/* should be zero in 586	*/
      pt[virt.pt_offset].pt_isvmalloc = 1;	/* for programmer's use		*/
      pt[virt.pt_offset].pt_isswapped = 0;	/* for programmer's use		*/
      pt[virt.pt_offset].pt_avail     = 0;	/* for programmer's use		*/

      vaddr                          += PAGE_SIZE;
   }

	vaddr                   = prptr->vmax << PAGE_OFFSET_BITS;
   prptr->vmax            += npages;
   prptr->vfree           -= npages;

	restore(mask);
	return (char*)vaddr;
}
