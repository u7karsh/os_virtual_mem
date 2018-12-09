/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>
#include <stdlib.h>

unsigned int error_code;
uint32 cr2, evict_frame, phys_frame, swapframe, maxpdptframe, maxffsframe, ptmapindex;
pdbr_t pdbr;
virt_addr_t virt;
pd_t *dir;
pt_t *pt;
pt_t *ptP;
uint32 cr3;

local void copy_page(uint32, uint32, bool8);
local uint32 swap_get_evict_candidate(uint32);

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 1. Pop out error code from stack
 2. Check for is_vmalloc
 3. Create new entry
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   cr3 = read_cr3();

   // Make sure no variables are on stack as it can cause issues
   // during virtual stack scenario
   // Also no function can be called in here as function calls
   // involve stack operation
   kernel_mode_enter();
   if( !(error_code & 0x1) ){
      // Read cr2, and cr3
      cr2  = read_cr2();
      pdbr = proctab[getpid()].pdbr;

      ASSERT( cr2 > (uint32)maxvstack, "Pagefault on illegal addr range %08X %08X %d '%s' %08X %08X\n", cr2, (uint32)maxvstack, currpid, proctab[getpid()].prname, cr3, pdbr);

      // Decode cr2
      virt = *((virt_addr_t*)&cr2);

      // Get the directory corresponding to the faulty page
      dir  = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

      if( dir[virt.pd_offset].pd_pres ){
         // Get the page entry corresponding to the faulty page
         pt   = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);
         ptP  = &pt[virt.pt_offset];

         ASSERT( !ptP->pt_pres, "SEGMENTATION FAULT (pt_pres) %08X %08X %08X %d\n", cr2, read_cr3(), *ptP, currpid);

         // Handle the fault IFF it was given a virtual addr
         if( ptP->pt_isvmalloc || ptP->pt_isswapped ){
            ASSERT( !(ptP->pt_isvmalloc && ptP->pt_isswapped), "isvmalloc and isswapped set together\n" );

            phys_frame    = getffsframe();

            maxpdptframe  = ceil_div( ((uint32)maxpdpt), PAGE_SIZE );
            maxffsframe   = ceil_div( ((uint32)maxffs), PAGE_SIZE );
            if( phys_frame == ((uint32)SYSERR >> PAGE_OFFSET_BITS) ){
               // There is no space in FFS region
               // 1. Find a random FFS frame to swap out
               ptmapindex  = rand() % MAX_FSS_SIZE;
               evict_frame = maxpdptframe + ptmapindex;

               // This is when a page being accessed is not in FFS (time to vmalloc) and:
               // 1. Old page being evicted has no swap memory
               // 2. Old page being evicted has a swap memory
               //    a. Since the old page is being evicted to swap, remove
               //       it's swap to ffs mapping
               // -> For the new page being brought to life, there is no swap associated
               //    with it yet so reset ffs2swap mapping 
               if( ptmap[ptmapindex]->pt_already_swapped ){
                  // 2. Old page being evicted has a swap memory
                  ASSERT( ffs2swapmap[ptmapindex] != -1, "ffs2swapmap does not have a mapping\n" );
                  swapframe                 = ffs2swapmap[ptmapindex];
               } else{
                  // 1. Assert if ffs2swap has a mapping
                  // 2. Old page being evicted has no swap memory
                  //    a. Allocate swap memory
                  //       (i).  If out of swap memory, get the first
                  //             frame which is in FFS and not dirty
                  //       (ii). Remove pt_already_swapped from the 
                  //             evicted frame
                  //    b. Remove swap2ffs mapping as the old page only
                  //       exist in swap memory
                  ASSERT( ffs2swapmap[ptmapindex] == -1, "ffs2swapmap has a mapping %d\n", ffs2swapmap[ptmapindex] );
                  swapframe                 = getswapframe();
                  if( swapframe == ((uint32)SYSERR >> PAGE_OFFSET_BITS) ){
                     // Run garbage collection
                     //  (i).  If out of swap memory, get the first
                     //        frame which is in FFS and not dirty
                     //  (ii). Remove pt_already_swapped from the 
                     //        evicted frame
                     swapframe               = swap_get_evict_candidate(cr2);
                     swap2ffsmap[swapframe]
                        ->pt_already_swapped = 0;
                     // Setting dirty so that page can be written back to 
                     // memory if every evicted
                     swap2ffsmap[swapframe]
                        ->pt_dirty           = 1;
                     ffs2swapmap[swap2ffsmap[swapframe]->pt_base - maxpdptframe] = -1;
                     swapframe              += maxffsframe;
                  }
               }
               // -> For the new page being brought to life, there is no swap associated
               //    with it yet so reset ffs2swap mapping 
               ffs2swapmap[ptmapindex]            = -1;
               // The swapped out frame has no mapping so make it NULL
               swap2ffsmap[swapframe-maxffsframe] = NULL;

               phys_frame                      = evict_frame;
               
               // Update old page that is currently being swapped out
               ptmap[ptmapindex]->pt_base      = swapframe;
               ptmap[ptmapindex]->pt_pres      = 0;
               ptmap[ptmapindex]->pt_isswapped = 1;
               ptmap[ptmapindex]->pt_already_swapped = 1;
               if( ptmap[ptmapindex]->pt_dirty || ptP->pt_isswapped ){
                  copy_page(evict_frame, swapframe, FALSE);
               }
               ptmap[ptmapindex]->pt_dirty     = 0;

               // The page being accessed right now is present in swap memory
               // bring it back
               if( ptP->pt_isswapped ){
                  ASSERT( ptP->pt_already_swapped, "Illegal pt_already_swapped when page is swapped\n" );
                  // A frame is being brought back to FFS
                  // Create mappings
                  // evict_frame <--> ptP->pt_base
                  ffs2swapmap[ptmapindex]               = ptP->pt_base;
                  swap2ffsmap[ptP->pt_base-maxffsframe] = ptP;
                  copy_page(ptP->pt_base, evict_frame, FALSE);
               } 

            } else{
               ptmapindex         = phys_frame - maxpdptframe;
               // If the swapped out page gets a free FFS region, bring it back
               if( ptP->pt_isswapped ){
                  ASSERT( ffs2swapmap[ptmapindex] == -1, "ffs2swapmap has a mapping (2) %d %d\n", ptmapindex, ffs2swapmap[ptmapindex] );
                  ASSERT( swap2ffsmap[ptP->pt_base - maxffsframe] == NULL, "swap2ffsmap has a mapping\n" );
                  copy_page(ptP->pt_base, phys_frame, FALSE);
                  // Do not free swap frame yet
                  //freeswapframe(ptP->pt_base);
                  // Add an entry in ffs2swapmap
                  ffs2swapmap[ptmapindex]                 = ptP->pt_base;
                  swap2ffsmap[ptP->pt_base - maxffsframe] = ptP;
               } else {
                  ASSERT( ptP->pt_isvmalloc, "Illegal value of vmalloc when FFS is available\n" );
               }
            }

            if( ptmap[ptmapindex] != NULL ){
               ASSERT( !ptmap[ptmapindex]->pt_pres, "ptmap anomaly\n");
            }

            ptmap[ptmapindex]     = ptP;

            ptP->pt_base          = phys_frame;
            ptP->pt_pres          = 1;
            ptP->pt_isvmalloc     = 0;
            ptP->pt_isswapped     = 0;
            ptP->pt_dirty         = 0;
         } else{
            // Segfault
            ASSERT(FALSE, "SEGMENTATION FAULT (!isvmalloc && !isswapped) %08X %08X %08X %d\n", cr2, read_cr3(), *ptP, currpid);
         }
      } else{
         // Segfault
         ASSERT(FALSE, "SEGMENTATION FAULT (!pdpres) %08X %08X %d\n", cr2, read_cr3(), currpid);
      }
   }
   kernel_mode_exit();
}

local void copy_page(uint32 fromframe, uint32 toframe, bool8 bothways){
   int i;
   uint32 temp;
   uint32 *fromuint32;
   uint32 *touint32;

   fromuint32           = (uint32*)(fromframe << PAGE_OFFSET_BITS);
   touint32             = (uint32*)(toframe << PAGE_OFFSET_BITS);

   if( bothways ){
      // Swap the contents
      for(i = 0; i < N_PAGE_ENTRIES; i++){
         temp          = fromuint32[i];
         fromuint32[i] = touint32[i];
         touint32[i]   = temp;
      }
   } else{
      for(i = 0; i < N_PAGE_ENTRIES; i++){
         touint32[i]  = fromuint32[i];
      }
   }
}

local uint32 swap_get_evict_candidate(uint32 cr2){
   int i;
   // Iterate through all swap pages and check if they are non-dirty
   for( i = 0; i < MAX_SWAP_SIZE; i++ ){
      // Give an allocated frame which also exists in FFS and is not dirty
      if( swap2ffsmap[i] != NULL && swap2ffsmap[i]->pt_pres /*&& !swap2ffsmap[i]->pt_dirty*/ ){
         return i;
      }
   }
   printmem(swaplist.mnext, "SWAP OUT:");
   ASSERT(FALSE, "Out of swap memory %d %08X!\n", currpid, cr2);
   return SYSERR;
}
