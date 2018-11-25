/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>
#include <stdio.h>

unsigned int error_code;

local void swap(uint32 ffsframe, uint32 swapframe, pt_t *ffsvmap);

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 1. Pop out error code from stack
 2. Check for is_vmalloc
 3. Create new entry
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   uint32 cr2, cr3, evict_frame, phys_frame, swapframe, maxpdptframe, ptmapindex;
   pdbr_t pdbr;
   virt_addr_t virt;
   pd_t *dir;
   pt_t *pt;
   pt_t *ptP, *oldPtP;

   if( !(error_code & 0x1) ){
      // Read cr2, and cr3
      cr2  = read_cr2();
      cr3  = read_cr3();

      // Decode cr2, cr3
      pdbr = *((pdbr_t*)&cr3);
      virt = *((virt_addr_t*)&cr2);

      // Get the directory corresponding to the faulty page
      dir  = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);

      if( dir[virt.pd_offset].pd_pres ){
         // Get the page entry corresponding to the faulty page
         pt   = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);
         ptP  = &pt[virt.pt_offset];

         // Handle the fault IFF it was given a virtual addr
         if( ptP->pt_isvmalloc || ptP->pt_isswapped ){
            // 1. Enter priveledged mode
            // 2. Allocate the page
            // 3. Exit priveledged mode
            kernel_mode_enter();
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
               if( ptmap[ptmapindex]->pt_dirty || ptP->pt_isswapped ){
                  // ffsframe, swapframe, ffsvmap
                  swapframe       = ptP->pt_isswapped ? ptP->pt_base : getswapframe();
                  swap(evict_frame, swapframe, ptmap[ptmapindex]);
                  phys_frame      = evict_frame;
               }
            } else{
               ptmapindex         = phys_frame - maxpdptframe;
            }

            ptP->pt_base          = phys_frame;
            ptmap[ptmapindex]     = ptP;
            kernel_mode_exit();

            ptP->pt_pres          = 1;
            ptP->pt_isvmalloc     = 0;
            ptP->pt_isswapped     = 0;
         } else{
            // Segfault
            kprintf("SEGMENTATION FAULT (!isvmalloc) %08X %08X %08X %d\n", cr2, read_cr3(), *ptP, currpid);
            halt();
         }
      } else{
         // Segfault
         kprintf("SEGMENTATION FAULT (!pdpres) %08X %08X %d\n", cr2, read_cr3(), currpid);
         halt();
      }
   }
}

local void swap(uint32 ffsframe, uint32 swapframe, pt_t *ffsvmap){
   int i;
   uint32 temp;
   uint32 *ffsuint32;
   uint32 *swapuint32;

   // Update the old entry belonging to this phys mem
   ffsuint32            = (uint32*)(ffsvmap->pt_base >> PAGE_OFFSET_BITS);
   swapuint32           = (uint32*)(swapframe >> PAGE_OFFSET_BITS);
   ffsvmap->pt_base     = swapframe;

   // Swap the contents
   for(i = 0; i < N_PAGE_ENTRIES; i++){
      temp          = ffsuint32[i];
      ffsuint32[i]  = swapuint32[i];
      swapuint32[i] = temp;
   }

   ffsvmap->pt_pres      = 0;
   ffsvmap->pt_isswapped = 1;
   if( ffsvmap->pt_isvmalloc ){
      kprintf("SYSERR: vmalloc pagefault_handler");
      halt();
   }
}
