/* vcreate.c - vcreate */

#include <xinu.h>

uint32		savsp, *pushsp;
intmask 	mask;    	/* Interrupt mask		*/
pid32		pid;		/* Stores new process id	*/
struct	procent	*prptr;		/* Pointer to proc. table entry */
int32		i;
uint32		*a;		/* Points to list of args	*/
uint32		*saddr;		/* Stack address		*/
uint32   _nargs;
void    *_funcaddr;
uint32  arg_container[1024];

/*------------------------------------------------------------------------
 *  create  -  Create a process to start running a function on x86
 *------------------------------------------------------------------------
 */
pid32	vcreate(
      void		*funcaddr,	/* Address of the function	*/
      uint32	ssize,		/* Stack size in bytes		*/
      uint32	hsize,		/* Virtual heap size in bytes		*/
      pri16		priority,	/* Process priority > 0		*/
      char		*name,		/* Name (for debugging)		*/
      uint32	nargs,		/* Number of args that follow	*/
      ...
      )
{
   mask = disable();
   if (ssize < MINSTK)
      ssize = MINSTK;
   ssize = (uint32) roundmb(ssize);
   if ( (priority < 1) || ((pid=newpid()) == SYSERR) || (hsize > MAX_HEAP_SIZE) ) {
      restore(mask);
      return SYSERR;
   }

   // TODO: don't allow vcreate to vcreate if virtual stack is enabled
   prcount++;
   prptr = &proctab[pid];

   /* Initialize process table entry for new process */
   prptr->prstate = PR_SUSP;	/* Initial state is suspended	*/
   prptr->prprio = priority;
   prptr->prstklen = ssize;
   prptr->prname[PNMLEN-1] = NULLCH;
   for (i=0 ; i<PNMLEN-1 && (prptr->prname[i]=name[i])!=NULLCH; i++)
      ;
   prptr->prsem = -1;
   prptr->prparent = (pid32)getpid();
   prptr->prhasmsg = FALSE;
   prptr->pruser   = TRUE;

   /* Set up stdin, stdout, and stderr descriptors for the shell	*/
   prptr->prdesc[0] = CONSOLE;
   prptr->prdesc[1] = CONSOLE;
   prptr->prdesc[2] = CONSOLE;

   /* The following is required to support paging */
   kernel_mode_enter();
   prptr->pdbr      = create_directory();
   kernel_mode_exit();

   prptr->hsize     = hsize;
   prptr->vmax      = ceil_div(((uint32)maxvstack + 1), PAGE_SIZE);
   prptr->vfree     = hsize;

   // Stash everything to safe location before changing pdbr
   _funcaddr        = funcaddr;
   _nargs           = nargs;
   a = (uint32 *)(&nargs + 1);	/* Start of args		*/
   a += nargs -1;			/* Last argument		*/
   i = 0;
   for ( ; nargs > 0 ; nargs--){	/* Machine dependent; copy args	*/
      arg_container[i] = *a--;	/* onto created process's stack	*/
      i++;
   }

   /* Initialize stack as if the process was called		*/
#ifdef VSTACK
   saddr = (uint32 *)getvstk(ssize, pid);
#else
   saddr = (uint32 *)getstk(ssize);
#endif
   
   // ------------ POINT OF NO RETURN -------------------
   write_pdbr(prptr->pdbr);

   prptr->prstkbase = (char *)saddr;
   *saddr = STACKMAGIC;
   savsp = (uint32)saddr;

   /* Push arguments */
   i = 0;
   for ( ; _nargs > 0 ; _nargs--){	/* Machine dependent; copy args	*/
      *--saddr = arg_container[i];	/* onto created process's stack	*/
      i++;
   }

   *--saddr = (long)INITRET;	/* Push on return address	*/

   /* The following entries on the stack must match what ctxsw	*/
   /*   expects a saved process state to contain: ret address,	*/
   /*   ebp, interrupt mask, flags, registers, and an old SP	*/

   *--saddr = (long)_funcaddr;	/* Make the stack look like it's*/
   /*   half-way through a call to	*/
   /*   ctxsw that "returns" to the*/
   /*   new process		*/
   *--saddr = savsp;		/* This will be register ebp	*/
   /*   for process exit		*/
   savsp = (uint32) saddr;		/* Start of frame for ctxsw	*/
   *--saddr = 0x00000200;		/* New process runs with	*/
   /*   interrupts enabled		*/

   /* Basically, the following emulates an x86 "pushal" instruction*/

   *--saddr = 0;			/* %eax */
   *--saddr = 0;			/* %ecx */
   *--saddr = 0;			/* %edx */
   *--saddr = 0;			/* %ebx */
   *--saddr = 0;			/* %esp; value filled in below	*/
   pushsp = saddr;			/* Remember this location	*/
   *--saddr = savsp;		/* %ebp (while finishing ctxsw)	*/
   *--saddr = 0;			/* %esi */
   *--saddr = 0;			/* %edi */

   *pushsp = (unsigned long) (prptr->prstkptr = (char *)saddr);
   write_pdbr(proctab[getpid()].pdbr);
   restore(mask);
   return pid;
}
