/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   kprintf("PAGE FAULT!!! %08X %08X %d\n", read_cr2(), read_cr3(), currpid);
   halt();
}
