/* vfree.c - vfree*/

#include <xinu.h>

void vfree(char *ptr, uint32 nbytes){
	struct procent *prptr = &proctab[getpid()];
   resume(create(kernel_service_free, 1024, prptr->prprio + 1, "free", 3, ptr, nbytes, getpid()));
}
