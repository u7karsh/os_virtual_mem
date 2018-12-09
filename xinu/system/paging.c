/* paging.c */

#include <xinu.h>

#define IRQPAGE 14

void	*minpdpt;
void	*maxpdpt;
void	*minffs;
void	*maxffs;
void	*minswap;
void	*maxswap;
void	*minvstack;
void	*maxvstack;

uint32 n_static_pages;
uint32 n_free_vpages;

pt_t *ptmap[MAX_FSS_SIZE];
pt_t *swap2ffsmap[MAX_SWAP_SIZE];
uint32 ffs2swapmap[MAX_FSS_SIZE];

long kernel_sp_space[1024];
long kernel_sp = &kernel_sp_space[1000];
long kernel_sp_old;

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
   ASSERT( frame != SYSERR, "Out of pdpt memory!\n");

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

uint32 getswapframe(){
   uint32 frame;
   frame = (uint32)_getfreemem(&swaplist, PAGE_SIZE);

   // Align it
   frame >>= PAGE_OFFSET_BITS;

   return frame;
}

uint32 getvstackframe(){
   uint32 frame;
   frame = (uint32)_getfreemem(&vstacklist, PAGE_SIZE);

   // Align it
   frame >>= PAGE_OFFSET_BITS;

   return frame;
}

syscall freepdptframe(uint32 frame){
   char *blkaddr = (char*)(frame << PAGE_OFFSET_BITS);
   return _freemem(&pdptlist, blkaddr, PAGE_SIZE, minpdpt, maxpdpt);
}

syscall freeffsframe(uint32 frame){
   char *blkaddr = (char*)(frame << PAGE_OFFSET_BITS);
   return _freemem(&ffslist, blkaddr, PAGE_SIZE, minffs, maxffs);
}

syscall freeswapframe(uint32 frame){
   char *blkaddr = (char*)(frame << PAGE_OFFSET_BITS);
   return _freemem(&swaplist, blkaddr, PAGE_SIZE, minswap, maxswap);
}

syscall freevstackframe(uint32 frame){
   char *blkaddr = (char*)(frame << PAGE_OFFSET_BITS);
   return _freemem(&vstacklist, blkaddr, PAGE_SIZE, minvstack, maxvstack);
}

pdbr_t create_directory(){
   pdbr_t pdbr;
   uint32 dirframeno;
   uint32 i;
   uint32 *diruint;
   uint32 npages;
   uint32 nentries;

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
   npages           = ceil_div( ((uint32)minpdpt), PAGE_SIZE );
   nentries         = ceil_div( npages, N_PAGE_ENTRIES );
   for( i = 0; i < nentries; i++ ){
      create_directory_entry((pd_t*)&diruint[i], i, i*N_PAGE_ENTRIES, 0, N_PAGE_ENTRIES);
   }

   // For the very first time, nullproc will update this variable
   if( n_static_pages == -1 ){
      n_static_pages  = nentries;
   }
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
      j                        = ventrystart + i;
      pt[j].pt_pres	          = 1;	/* page is present?		*/
      pt[j].pt_write           = 1;	/* page is writable?		*/
      pt[j].pt_user	          = 0;	/* is use level protection?	*/
      pt[j].pt_pwt	          = 0;	/* write through for this page? */
      pt[j].pt_pcd	          = 1;	/* cache disable for this page? */
      pt[j].pt_acc	          = 0;	/* page was accessed?		*/
      pt[j].pt_dirty           = 0;	/* page was written?		*/
      pt[j].pt_mbz	          = 0;	/* must be zero			*/
      pt[j].pt_global          = 0;	/* should be zero in 586	*/
      pt[j].pt_isvmalloc       = 0;	/* for programmer's use		*/
      pt[j].pt_isswapped       = 0;	/* for programmer's use		*/
      pt[j].pt_already_swapped = 0;	/* for programmer's use		*/
      pt[j].pt_base	          = (phybaseaddr + i); /* location of page?		*/
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

void write_pdbr( pdbr_t pdbr ){
   uint32 val = *((uint32*)&pdbr);
   write_cr3( val );
   disable_paging();
   enable_paging();
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

void destroy_directory(pid32 pid){
   pdbr_t pdbr     = proctab[pid].pdbr;
   uint32 dirno    = pdbr.pdbr_base;
   pd_t *frame     = (pd_t*)(dirno << PAGE_OFFSET_BITS);
   pt_t *table;
   uint32 nstart, npages;
   int i, j;

   // Destroy directory IFF user process
   if(!proctab[pid].pruser) return;

   // No need to free static pages as they are shared and nullproc
   // allocated them
   // As nullproc will never be freed, no need to free it
   npages  = ceil_div( ((uint32)minffs), PAGE_SIZE );
   nstart  = ceil_div( npages, N_PAGE_ENTRIES );
   for( i = nstart; i < N_PAGE_ENTRIES; i++ ){
      if( frame[i].pd_pres ){
         // There is a valid directory entry. Iterate through the page table
         // to free any allocated memory to prevent memory leaks
         table     = (pt_t*)(frame[i].pd_base << PAGE_OFFSET_BITS);
         for( j = 0; j < N_PAGE_ENTRIES; j++ ){
            ASSERT( !table[j].pt_pres, "Inconsistency in free at destroy\n" );
         }
         // Free the table
         ASSERT( freepdptframe(frame[i].pd_base) != SYSERR, "Unable to free PD/PT frame %08X\n", frame[i]);
      }
   }

   // Free directory
   ASSERT(freepdptframe(dirno) != SYSERR, "Unable to free PD/PT directory");
}

void freevmem(pid32 pid){
   pdbr_t pdbr     = proctab[pid].pdbr;
   uint32 dirno    = pdbr.pdbr_base;
   pd_t *frame     = (pd_t*)(dirno << PAGE_OFFSET_BITS);
   pt_t *table;
   virt_addr_t virt_addr;
   uint32 npages, nstart, addr;
   int i, j;

   // Destroy directory IFF user process
   if(!proctab[pid].pruser) return;

   // No need to free static pages as they are shared and nullproc
   // allocated them
   npages  = ceil_div( ((uint32)minffs), PAGE_SIZE );
   nstart  = ceil_div( npages, N_PAGE_ENTRIES );
   // As nullproc will never be freed, no need to free it
   for( i = nstart; i < N_PAGE_ENTRIES; i++ ){
      if( frame[i].pd_pres ){
         // There is a valid directory entry. Iterate through the page table
         // to free any allocated memory to prevent memory leaks
         table     = (pt_t*)(frame[i].pd_base << PAGE_OFFSET_BITS);
         for( j = 0; j < N_PAGE_ENTRIES; j++ ){
            if( table[j].pt_pres || table[j].pt_isswapped ){
               virt_addr.pd_offset = i;
               virt_addr.pt_offset = j;
               addr = *((uint32*)&virt_addr);
               // Free any physical memory associated with it
               free_vpage(frame, addr >> PAGE_OFFSET_BITS, FALSE);
               ASSERT( !table[j].pt_pres, "Inconsistency in free at freevmem\n" );
            }
         }
      }
   }
   n_free_vpages += proctab[pid].hsize - proctab[pid].vfree;
   ASSERT( n_free_vpages >= 0 && n_free_vpages <= MAX_HEAP_SIZE, "Illegal value of n_free_vpages (=%d)\n", n_free_vpages );
}


void init_paging(){
   int i;

   // Init PD/PT
   __init( &pdptlist, (char*)((uint32)maxheap + 1), PAGE_SIZE * MAX_PT_SIZE, &minpdpt, &maxpdpt );
   n_static_pages = -1;
   n_free_vpages  = MAX_HEAP_SIZE;

   // Init FFS region
   __init( &ffslist, (char*)((uint32)maxpdpt + 1), PAGE_SIZE * MAX_FSS_SIZE, &minffs, &maxffs );
   for( i = 0; i < MAX_FSS_SIZE; i++){
      ptmap[i]       = NULL;
      ffs2swapmap[i] = -1;
   }
   for( i = 0; i < MAX_SWAP_SIZE; i++){
      swap2ffsmap[i] = NULL;
   }

   // Init swap region
   __init( &swaplist, (char*)((uint32)maxffs + 1), PAGE_SIZE * MAX_SWAP_SIZE, &minswap, &maxswap );

   // Init virtual stack region
   __init( &vstacklist, (char*)((uint32)maxswap + 1), PAGE_SIZE * MAX_STACK_SIZE, &minvstack, &maxvstack );

   /* Set interrupt vector for the pagefault to invoke pagefault_handler_disp */
   set_evec(IRQPAGE, (uint32)pagefault_handler_disp);
}
