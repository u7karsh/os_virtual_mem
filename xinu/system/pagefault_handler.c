/* pagefault_handler.c - pagefault_handler */

#include <xinu.h>

#define IRQPAGE 14

void page_init(){
	/* Set interrupt vector for the pagefault to invoke pagefault_handler_disp */
	set_evec(IRQPAGE, (uint32)pagefault_handler_disp);
}

/*------------------------------------------------------------------------
 * pagefault_handler - high level page interrupt handler
 *------------------------------------------------------------------------
 */
void	pagefault_handler(){
}
