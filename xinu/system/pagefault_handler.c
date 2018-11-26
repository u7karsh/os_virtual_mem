/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>
#include <stdio.h>

unsigned int error_code;

local void copy_page(uint32, uint32, bool8);

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 1. Pop out error code from stack
 2. Check for is_vmalloc
 3. Create new entry
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   uint32 cr2, evict_frame, phys_frame, swapframe, maxpdptframe, ptmapindex;
   pdbr_t pdbr;
   virt_addr_t virt;
   pd_t *dir;
   pt_t *pt;
   pt_t *ptP;

   kernel_mode_enter();
   if( !(error_code & 0x1) ){
      // Read cr2, and cr3
      cr2  = read_cr2();
      pdbr = proctab[getpid()].pdbr;

      if( cr2 < (uint32)maxswap ){
         kprintf("SYSERR: Pagefault on illegal addr range %08X %08X %d\n", cr2, (uint32)maxswap, currpid);
         halt();
      }

      // Decode cr2
      virt = *((virt_addr_t*)&cr2);

      // Get the directory corresponding to the faulty page
      dir  = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

      if( dir[virt.pd_offset].pd_pres ){
         // Get the page entry corresponding to the faulty page
         pt   = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);
         ptP  = &pt[virt.pt_offset];

         if( ptP->pt_pres ){
            kprintf("SEGMENTATION FAULT (pt_pres) %08X %08X %08X %d\n", cr2, read_cr3(), *ptP, currpid);
            halt();
            return;
         }

         // Handle the fault IFF it was given a virtual addr
         if( ptP->pt_isvmalloc || ptP->pt_isswapped ){
            if( ptP->pt_isvmalloc && ptP->pt_isswapped ){
               kprintf("SYSERR: isvmalloc and isswapped set together\n");
               halt();
            }
            // 1. Enter priveledged mode
            // 2. Allocate the page
            // 3. Exit priveledged mode
            phys_frame    = getffsframe();

            maxpdptframe  = ceil_div( ((uint32)maxpdpt), PAGE_SIZE );
            if( phys_frame == (uint32)SYSERR >> PAGE_OFFSET_BITS ){
               // There is no space in FFS region
               // 1. Find a random FFS frame to swap out
               ptmapindex  = rand() % MAX_FSS_SIZE;
               evict_frame = maxpdptframe + ptmapindex;

               // 2. Check if dirty
               //    -> yes => allocate a frame in swap mem and do a swap
               //    -> no  => drop the page
               swapframe                       = ptP->pt_isswapped ? ptP->pt_base : getswapframe();
               phys_frame                      = evict_frame;
               ptmap[ptmapindex]->pt_base      = swapframe;
               ptmap[ptmapindex]->pt_pres      = 0;
               ptmap[ptmapindex]->pt_isswapped = 1;
               if( ptmap[ptmapindex]->pt_dirty || ptP->pt_isswapped ){
                  copy_page(evict_frame, swapframe, ptP->pt_isswapped);
               }
            } else{
               ptmapindex         = phys_frame - maxpdptframe;
               // If the swapped out page gets a free FFS region, bring it back
               if( ptP->pt_isswapped ){
                  copy_page(ptP->pt_base, phys_frame, FALSE);
                  freeswapframe(ptP->pt_base);
               }
            }

            if( ptmap[ptmapindex] != NULL ){
               if( ptmap[ptmapindex]->pt_pres ){
                  kprintf("ptmap anomaly\n");
               }
            }

            ptmap[ptmapindex]     = ptP;

            ptP->pt_base          = phys_frame;
            ptP->pt_pres          = 1;
            ptP->pt_isvmalloc     = 0;
            ptP->pt_isswapped     = 0;
         } else{
            // Segfault
            kprintf("SEGMENTATION FAULT (!isvmalloc && !isswapped) %08X %08X %08X %d\n", cr2, read_cr3(), *ptP, currpid);
            halt();
         }
      } else{
         // Segfault
         kprintf("SEGMENTATION FAULT (!pdpres) %08X %08X %d\n", cr2, read_cr3(), currpid);
         halt();
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
