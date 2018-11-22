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

char *getpdptframe(){
   uint32 frame;
   frame = (uint32)_getfreemem(&pdptlist, PAGE_SIZE);

   // Align it
   frame >>= 12;
   
   kprintf("Frame: %08X\n", frame);
   return (char *)frame;
}

syscall freepdptframe(char *frame){
   char *blkaddr = ((uint32) frame) << 12;
   return _freemem(&pdptlist, blkaddr, PAGE_SIZE, minpdpt, maxpdpt);
}

pdbr_t create_pdbr(){
   pdbr_t pdbr;

   pdbr.pdbr_mb1   = 1;
   pdbr.pdbr_rsvd  = 0;
   pdbr.pdbr_pwt   = 0;
   pdbr.pdbr_pcd   = 0;
   pdbr.pdbr_rsvd2 = 0;
   pdbr.pdbr_avail = 0;
   pdbr.pdbr_base  = (unsigned long)getpdptframe();

   return pdbr;
}

void init_paging(){
   // Init PD/PT
   __init( &pdptlist, (char*)((uint32)maxheap + 1), PAGE_SIZE * MAX_PT_SIZE, &minpdpt, &maxpdpt );
   /* Set interrupt vector for the pagefault to invoke pagefault_handler_disp */
   //set_evec(IRQPAGE, (uint32)pagefault_handler_disp);
   //enable_paging();
}
