/* paging.c */

#include <xinu.h>

#define IRQPAGE 14

void	*minpdpt;
void	*maxpdpt;
void	*minffs;
void	*maxffs;
void	*minswap;
void	*maxswap;
uint32 n_static_pages;
uint32 n_free_vpages;

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
   *maxstructP = (void*)(minstruct + listlength - 1);
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
   kprintf("SYSERR2: _getfreemem %d\n", nbytes);
   halt();
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

uint32 getffsframe(){
   uint32 frame;
   frame = (uint32)_getfreemem(&ffslist, PAGE_SIZE);

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
   uint32 *diruint;
   uint32 npages;
   uint32 nentries;
   bool8 nullproc_share;

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
   for( i = 0; i < N_PAGE_ENTRIES; i++ ){
      diruint[i] = 0;
   }

   // Allocate bare minimum pages a.k.a flat mapping
   npages           = ceil_div( ((uint32)maxpdpt), PAGE_SIZE );
   nentries         = ceil_div( npages, N_PAGE_ENTRIES );
   for( i = 0; i < nentries; i++ ){
      nullproc_share = i != (nentries - 1);
      create_directory_entry((pd_t*)&diruint[i], nullproc_share ? i : -1, i*N_PAGE_ENTRIES, 0, nullproc_share ? N_PAGE_ENTRIES : npages % N_PAGE_ENTRIES);
   }

   n_static_pages  = nentries;
   return pdbr;
}

// nullproc_share_index: != -1 if an entry has to be shared from null proc directory (flatmap mem)
void create_directory_entry(pd_t *pd, uint32 nullproc_share_index, uint32 phybaseaddr, uint32 ventrystart, uint32 nventries){
   int i;
   pd_t   *nullprocdir;
   uint32 *pageuint;

   pd->pd_pres	     = 1;	/* page table present?		*/
   pd->pd_write     = 1;	/* page is writable?		*/
   pd->pd_user	     = 0;	/* is use level protection?	*/
   pd->pd_pwt	     = 0;	/* write through cachine for pt?*/
   pd->pd_pcd	     = 1;	/* cache disable for this pt?	*/
   pd->pd_acc	     = 0;	/* page table was accessed?	*/
   pd->pd_mbz	     = 0;	/* must be zero			*/
   pd->pd_fmb	     = 0;	/* four MB pages?		*/
   pd->pd_global    = 0;	/* global (ignored)		*/
   pd->pd_avail     = 0;	/* for programmer's use		*/

   if( n_static_pages == -1 && currpid != 0 ){
      kprintf("SYSERR: n_static_pages is -1 for pid %d\n", currpid);
      halt();
   }

   pageuint        = (uint32*)((uint32)pd->pd_base << PAGE_OFFSET_BITS);

   // Flash clear all directory entries
   // Zero all 1k entries
   for( i = 0; i < N_PAGE_ENTRIES; i++ ){
      pageuint[i] = 0;
   }

   // Logic to share static page table amongst all processes
   if( n_static_pages != -1 && nullproc_share_index != -1 ){
      // Steal the entries from nullproc
      nullprocdir  = (pd_t*)((uint32)(proctab[0].pdbr.pdbr_base) << PAGE_OFFSET_BITS);
      pd->pd_base	 = nullprocdir[nullproc_share_index].pd_base;
   } else{
      pd->pd_base	 = create_pagetable_entries(0, phybaseaddr, ventrystart, nventries);		/* location of page table?	*/
   }
}

uint32 create_pagetable_entries(uint32 ptbase, uint32 phybaseaddr, uint32 ventrystart, uint32 nventries){
   pt_t *pt;
   int i, j;

   // Allocate page frame if not allocated
   if( ptbase == 0 ){
      ptbase          = getpdptframe();
   }
   pt                 = (pt_t*)(ptbase << PAGE_OFFSET_BITS);

   for( i = 0; i < nventries; i++ ){
      j                  = ventrystart + i;
      pt[j].pt_pres	    = 1;	/* page is present?		*/
      pt[j].pt_write     = 1;	/* page is writable?		*/
      pt[j].pt_user	    = 0;	/* is use level protection?	*/
      pt[j].pt_pwt	    = 0;	/* write through for this page? */
      pt[j].pt_pcd	    = 1;	/* cache disable for this page? */
      pt[j].pt_acc	    = 0;	/* page was accessed?		*/
      pt[j].pt_dirty     = 0;	/* page was written?		*/
      pt[j].pt_mbz	    = 0;	/* must be zero			*/
      pt[j].pt_global    = 0;	/* should be zero in 586	*/
      pt[j].pt_isvmalloc = 0;	/* for programmer's use		*/
      pt[j].pt_isswapped = 0;	/* for programmer's use		*/
      pt[j].pt_avail     = 0;	/* for programmer's use		*/
      pt[j].pt_base	    = (phybaseaddr + i); /* location of page?		*/
   }
   return ptbase;
}

void print_page(pd_t pd){
   int i;
   uint32* page = (uint32*)(pd.pd_base << PAGE_OFFSET_BITS);
   for( i = 0; i < N_PAGE_ENTRIES; i++ ){
      if( page[i] & 0x1 ){
         kprintf("        %08d: 0x%08X\n", i, page[i]);
      }
   }
}

// We assume that our kernel is nullproc
// CTXSW PDBR to null proc to emulate
// entering kernel mode
void kernel_mode_enter(){
   write_cr3( *((uint32*)&(proctab[0].pdbr)) );
}

// We assume that our kernel is nullproc
// CTXSW PDBR to curr proc to emulate
// exiting kernel mode
void kernel_mode_exit(){
   write_cr3( *((uint32*)&(proctab[currpid].pdbr)) );
}

void print_directory(pdbr_t pdbr){
   int i;
   uint32* dir = (uint32*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);
   kprintf("PDBR: %08X\n", *((pdbr_t*)&pdbr));
   for( i = 0; i < N_PAGE_ENTRIES; i++ ){
      if( dir[i] & 0x1 ){
         kprintf("%08d: 0x%08X\n", i, dir[i]);
      }
   }
}

void destroy_directory(pdbr_t pdbr){
   uint32 dirno    = pdbr.pdbr_base;
   pd_t *frame     = (pd_t*)(dirno << PAGE_OFFSET_BITS);
   uint32 npages   = ceil_div( ((uint32)maxpdpt), PAGE_SIZE );
   uint32 nentries = ceil_div( npages, N_PAGE_ENTRIES );
   int i;

   // No need to free static pages as they are shared and nullproc
   // allocated them
   // As nullproc will never be freed, no need to free it
   for( i = nentries - 1; i < N_PAGE_ENTRIES; i++ ){
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
   n_static_pages = -1;
   n_free_vpages  = MAX_HEAP_SIZE;

   // Init FFS region
   __init( &ffslist, (char*)((uint32)maxpdpt + 1), PAGE_SIZE * MAX_FSS_SIZE, &minffs, &maxffs );

   // Init swap region
   __init( &swaplist, (char*)((uint32)maxffs + 1), PAGE_SIZE * MAX_SWAP_SIZE, &minswap, &maxswap );

   /* Set interrupt vector for the pagefault to invoke pagefault_handler_disp */
   set_evec(IRQPAGE, (uint32)pagefault_handler_disp);
   //enable_paging();
}
