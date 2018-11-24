/* paging.c */

#include <xinu.h>

#define IRQPAGE 14

void	*minpdpt;
void	*maxpdpt;

/*------------------------------------------------------------------------
 * pdptinit - initialize directory/table free list
 *------------------------------------------------------------------------
 */

void __init(struct	memblk *list, char *__start, uint32 listlength, void **minstructP, void **maxstructP){
   uint32 minstruct;
   struct	memblk	*memptr;	/* Ptr to memory block		*/

   /* Initialize the memory list */
   minstruct   = (uint32) roundmb((uint32)__start);
   listlength  = (uint32) truncmb(listlength);

   /* initialize to one block */
   list->mnext     = memptr = (struct memblk *)(minstruct);
   memptr->mlength = listlength;
   memptr->mnext   = (struct memblk *) NULL;

   if ((char *)(minstruct) <= HOLESTART) {
      kprintf("HOLE found in __init\n");
      stacktrace(currpid);
      halt();
   }

   *minstructP = (void*)(minstruct);
   *maxstructP = (void*)(minstruct + listlength);
}

local char *_getfreemem(struct	memblk	*list, uint32 nbytes){
   intmask	mask;			/* Saved interrupt mask		*/
   struct	memblk	*prev, *curr, *leftover;

   mask = disable();
   if (nbytes == 0) {
      restore(mask);
      kprintf("SYSERR: _getfreemem\n");
      return (char *)SYSERR;
   }

   nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/

   prev = list;
   curr = list->mnext;
   while (curr != NULL) {			/* Search free list	*/

      if (curr->mlength == nbytes) {	/* Block is exact match	*/
         prev->mnext = curr->mnext;
         list->mlength -= nbytes;
         restore(mask);
         return (char *)(curr);

      } else if (curr->mlength > nbytes) { /* Split big block	*/
         leftover = (struct memblk *)((uint32) curr +	nbytes);
         prev->mnext = leftover;
         leftover->mnext = curr->mnext;
         leftover->mlength = curr->mlength - nbytes;
         list->mlength -= nbytes;
         restore(mask);
         return (char *)(curr);
      } else {			/* Move to next block	*/
         prev = curr;
         curr = curr->mnext;
      }
   }
   kprintf("SYSERR2: _getfreemem\n");
   restore(mask);
   return (char *)SYSERR;
}

local syscall	_freemem(
      struct	memblk	*list,
      char		*blkaddr,	/* Pointer to memory block	*/
      uint32	nbytes,		/* Size of block in bytes	*/
      char     *__minstruct,
      char     *__maxstruct
      )
{
   intmask	mask;			/* Saved interrupt mask		*/
   struct	memblk	*next, *prev, *block;
   uint32	top;

   mask = disable();
   if ((nbytes == 0) || ((uint32) blkaddr < (uint32) __minstruct)
         || ((uint32) blkaddr > (uint32) __maxstruct)) {
      kprintf("SYSERR: _freemem %08X %08X %08X\n", (uint32) blkaddr, (uint32)__minstruct, (uint32)__maxstruct);
      restore(mask);
      return SYSERR;
   }

   nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/
   block = (struct memblk *)blkaddr;

   prev = list;			/* Walk along free list	*/
   next = list->mnext;
   while ((next != NULL) && (next < block)) {
      prev = next;
      next = next->mnext;
   }

   if (prev == list) {		/* Compute top of previous block*/
      top = (uint32) NULL;
   } else {
      top = (uint32) prev + prev->mlength;
   }

   /* Ensure new block does not overlap previous or next blocks	*/

   if (((prev != list) && (uint32) block < top)
         || ((next != NULL)	&& (uint32) block+nbytes>(uint32)next)) {
      kprintf("SYSERR2: _freemem\n");
      restore(mask);
      return SYSERR;
   }

   list->mlength += nbytes;

   /* Either coalesce with previous block or add to free list */

   if (top == (uint32) block) { 	/* Coalesce with previous block	*/
      prev->mlength += nbytes;
      block = prev;
   } else {			/* Link into list as new node	*/
      block->mnext = next;
      block->mlength = nbytes;
      prev->mnext = block;
   }

   /* Coalesce with next block if adjacent */

   if (((uint32) block + block->mlength) == (uint32) next) {
      block->mlength += next->mlength;
      block->mnext = next->mnext;
   }
   restore(mask);
   return OK;
}

uint32 getpdptframe(){
   uint32 frame;
   frame = (uint32)_getfreemem(&pdptlist, PAGE_SIZE);

   // Align it
   frame >>= PAGE_OFFSET_BITS;

   return frame;
}

syscall freepdptframe(uint32 frame){
   char *blkaddr = (char*)(frame << PAGE_OFFSET_BITS);
   return _freemem(&pdptlist, blkaddr, PAGE_SIZE, minpdpt, maxpdpt);
}



pdbr_t create_directory(){
   pdbr_t pdbr;
   uint32 dirframeno;
   uint32 i;
   uint32 j;
   uint32 *diruint;
   uint32 npages;
   uint32 nentries;
   uint32 ptbase;
   pd_t   *pd;
   pt_t   *pt;

   dirframeno      = getpdptframe();
   diruint         = (uint32*)(dirframeno << PAGE_OFFSET_BITS);

   pdbr.pdbr_mb1   = 0;
   pdbr.pdbr_rsvd  = 0;
   pdbr.pdbr_pwt   = 0;
   pdbr.pdbr_pcd   = 1;
   pdbr.pdbr_rsvd2 = 0;
   pdbr.pdbr_avail = 0;
   pdbr.pdbr_base  = dirframeno;

   // Zero all 1k entries
   for( i = 0; i < 1024; i++ ){
      diruint[i] = 0;
   }

   // Allocate bare minimum pages
   npages           = ceil( ((uint32)maxpdpt), PAGE_SIZE );
   nentries         = ceil( npages, 1024 );
   for( i = 0; i < nentries; i++ ){
      ptbase        = getpdptframe();
      pt            = (pt_t*)(ptbase << PAGE_OFFSET_BITS);
      for( j = 0; j < 1024; j++ ){
         pt[j].pt_pres	 = 1;	/* page is present?		*/
         pt[j].pt_write  = 1;	/* page is writable?		*/
         pt[j].pt_user	 = 0;	/* is use level protection?	*/
         pt[j].pt_pwt	 = 0;	/* write through for this page? */
         pt[j].pt_pcd	 = 1;	/* cache disable for this page? */
         pt[j].pt_acc	 = 0;	/* page was accessed?		*/
         pt[j].pt_dirty  = 0;	/* page was written?		*/
         pt[j].pt_mbz	 = 0;	/* must be zero			*/
         pt[j].pt_global = 0;	/* should be zero in 586	*/
         pt[j].pt_avail  = 0;	/* for programmer's use		*/
         // TODO: Akshay
         pt[j].pt_base	 = (i*1024 + j); /* location of page?		*/
      }

      pd              = (pd_t*)&diruint[i];

      pd->pd_pres	    = 1;	/* page table present?		*/
      pd->pd_write    = 1;	/* page is writable?		*/
      pd->pd_user	    = 0;	/* is use level protection?	*/
      pd->pd_pwt	    = 0;	/* write through cachine for pt?*/
      pd->pd_pcd	    = 1;	/* cache disable for this pt?	*/
      pd->pd_acc	    = 0;	/* page table was accessed?	*/
      pd->pd_mbz	    = 0;	/* must be zero			*/
      pd->pd_fmb	    = 0;	/* four MB pages?		*/
      pd->pd_global   = 0;	/* global (ignored)		*/
      pd->pd_avail    = 0;	/* for programmer's use		*/
      pd->pd_base	    = ptbase;		/* location of page table?	*/
   }

   return pdbr;
}

void print_page(pd_t pd){
   int i;
   uint32* page = (uint32*)(pd.pd_base << PAGE_OFFSET_BITS);
   for( i = 0; i < 1024; i++ ){
      if( page[i] & 0x1 ){
         kprintf("        %08d: 0x%08X\n", i, page[i]);
      }
   }
}

void print_directory(pdbr_t pdbr){
   int i;
   uint32* dir = (uint32*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);
   kprintf("PDBR: %08X\n", *((pdbr_t*)&pdbr));
   for( i = 0; i < 1024; i++ ){
      if( dir[i] & 0x1 ){
         kprintf("%08d: 0x%08X\n", i, dir[i]);
      }
   }
}

void destroy_directory(pdbr_t pdbr){
   uint32 dirno  = pdbr.pdbr_base;
   pd_t *frame   = (pd_t*)(dirno << PAGE_OFFSET_BITS);
   int i;

   // Free all pages
   for( i = 0; i < 1024; i++ ){
      if( frame[i].pd_pres == TRUE ){
         freepdptframe(frame[i].pd_base);
      }
   }

   // Free directory
   freepdptframe(dirno);
}

void init_paging(){
   // Init PD/PT
   __init( &pdptlist, (char*)((uint32)maxheap + 1), PAGE_SIZE * MAX_PT_SIZE, &minpdpt, &maxpdpt );
   /* Set interrupt vector for the pagefault to invoke pagefault_handler_disp */
   set_evec(IRQPAGE, (uint32)pagefault_handler_disp);
   //enable_paging();
}
