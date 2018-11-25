/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>

unsigned error_code;
/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 1. Pop out error code from stack
 2. Check for is_vmalloc
 3. Create new entry
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   uint32 cr2;
   pdbr_t pdbr;
   virt_addr_t virt;
   pd_t *dir;
   pt_t *pt;

   kprintf("chk3 %08X %08X\n", read_cr3(), read_cr4());
   kernel_mode_enter();

   cr2  = read_cr2();
   pdbr = proctab[getpid()].pdbr;
   virt = *((virt_addr_t*)&cr2);
   dir  = (pd_t*)(pdbr.pdbr_base << PAGE_OFFSET_BITS);
   pt   = (pt_t*)(dir[virt.pd_offset].pd_base << PAGE_OFFSET_BITS);

   kprintf("chk4 %08X\n", pdbr);
   // 1. Pop out error code from stack
	asm("pop %eax");
	asm("mov %eax, error_code");

   kprintf("chk1 %08X\n", error_code);
   if( !(error_code & 0x1) ){
      // Enter priveledged mode
      if( pt[virt.pt_offset].pt_isvmalloc ){
         pt[virt.pt_offset].pt_base      = getffsframe();
         pt[virt.pt_offset].pt_pres      = 1;
         pt[virt.pt_offset].pt_isvmalloc = 0;
         kprintf("FIX %08X %08X %08X %d\n", cr2, read_cr3(), pt[virt.pt_offset], currpid);
         printmem(ffslist.mnext, "PG HANDLER");
      } else{
         // Segfault
         kprintf("SEGMENTATION FAULT %08X %08X %08X %d\n", cr2, read_cr3(), pt[virt.pt_offset], currpid);
         halt();
      }
   }
   
   kernel_mode_exit();
   kprintf("chk2 %08X\n", read_cr3());
}
