/* vmalloc.c - vmalloc*/

#include <xinu.h>

/*------------------------------------------------------------------------
 *  vmalloc -  Allocate heap storage, returning lowest word address
 *------------------------------------------------------------------------
 */
char *vmalloc(uint32 nbytes){
   uint32 npages, vaddr;
	struct procent *prptr = &proctab[getpid()];

   npages         = ceil_div( nbytes, PAGE_SIZE );

	if (nbytes == 0 || npages > prptr->vfree){
		return (char *)SYSERR;
	}

   resume(create(kernel_service_malloc, 1024, prptr->prprio + 1, "malloc", 3, nbytes, FALSE, getpid()));

	vaddr          = prptr->vmax << PAGE_OFFSET_BITS;
   prptr->vmax   += npages;
   prptr->vfree  -= npages;
   return (char*)vaddr;
}

char *getvstk(uint32 nbytes, pid32 pid){
   uint32 npages, vaddr;
	struct procent *prptr = &proctab[pid];

   npages         = ceil_div( nbytes, PAGE_SIZE );

	if (nbytes == 0){
		return (char *)SYSERR;
	}

   resume(create(kernel_service_malloc, 1024, prptr->prprio + 1, "getvstk", 3, nbytes, TRUE, pid));

	vaddr          = prptr->vmax << PAGE_OFFSET_BITS;
   prptr->vmax   += npages;
   return (char*)(vaddr + nbytes - 1);
}
