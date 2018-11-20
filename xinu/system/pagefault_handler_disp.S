/* pagefault_handler_disp.s - pagefault_handler_disp (x86) */

/*------------------------------------------------------------------------
 * pagefault_handler_disp -  Interrupt dispatcher for page fault (x86 version)
 *------------------------------------------------------------------------
 */
#include <icu.s>
		.text
		.globl	pagefault_handler_disp		# Page fault interrupt dispatcher
pagefault_handler_disp:
		pushal			# Save registers
		cli			# Disable further interrupts
		movb	$EOI,%al	# Reset interrupt
		outb	%al,$OCW1_2

		call	pagefault_handler	# Call high level handler

		sti			# Restore interrupt status
		popal			# Restore registers
		iret			# Return from interrupt
