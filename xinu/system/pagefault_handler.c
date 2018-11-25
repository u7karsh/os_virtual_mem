/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>

unsigned int error_code;
/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 1. Pop out error code from stack
 2. Check for is_vmalloc
 3. Create new entry
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   uint32 cr2, cr3;
   pdbr_t pdbr;
   virt_addr_t virt;
   pd_t *dir;
   pt_t *pt;

   cr2  = read_cr2();
   cr3  = read_cr3();
   pdbr = *((pdbr_t*)&cr3);
   virt = *((virt_addr_t*)&cr2);
   dir  = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);
   pt   = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);

   // 1. Pop out error code from stack
   if( !(error_code & 0x1) ){
      // Enter priveledged mode
      if( pt[virt.pt_offset].pt_isvmalloc ){
         kernel_mode_enter();
         pt[virt.pt_offset].pt_base      = getffsframe();
         kernel_mode_exit();

         pt[virt.pt_offset].pt_pres      = 1;
         pt[virt.pt_offset].pt_isvmalloc = 0;
      } else{
         // Segfault
         kprintf("SEGMENTATION FAULT %08X %08X %08X %d\n", cr2, read_cr3(), pt[virt.pt_offset], currpid);
         halt();
      }
   }
}
