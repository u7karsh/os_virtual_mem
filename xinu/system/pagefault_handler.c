/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
   kprintf("PAGE FAULT!!! %d %d\n", read_cr3(), currpid);
   halt();
}
