/* kill.c - kill */

#include <xinu.h>

pid32 _pid;
uint16 _prstate;
sid32 _prsem;
intmask	_mask;			/* Saved interrupt mask		*/
bool8 _pruser;
pid32 _prparent;

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
      pid32		pid		/* ID of process to kill	*/
      )
{
   struct	procent *prptr;		/* Ptr to process's table entry	*/
   int32	i;			/* Index into descriptors	*/

   _mask = disable();
   if (isbadpid(pid) || (pid == NULLPROC)
         || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
      restore(_mask);
      return SYSERR;
   }

   if (--prcount <= 1) {		/* Last user process completes	*/
      xdone();
   }

   for (i=0; i<3; i++) {
      close(prptr->prdesc[i]);
   }
   freestk(prptr->prstkbase, prptr->prstklen);

   _prstate  = prptr->prstate;
   _prsem    = prptr->prsem;
   _pruser   = prptr->pruser;
   _prparent = prptr->prparent;
   _pid      = pid;

   prptr->prstate = PR_FREE;

   // Switch to protected mode/stack
   kernel_mode_enter();
   freevmem(_pid);

   // Utkarsh: Quick fix for receive inconsistency in testcases
   if( _pruser ){
      send(_prparent, _pid);
   }

   switch (_prstate) {
      case PR_CURR:
         resched();

      case PR_SLEEP:
      case PR_RECTIM:
         unsleep(_pid);
         break;

      case PR_WAIT:
         semtab[_prsem].scount++;
         /* Fall through */

      case PR_READY:
         getitem(_pid);		/* Remove from queue */
         /* Fall through */

      default:
         break;
   }

   destroy_directory(_pid);
   kernel_mode_exit();
   restore(_mask);
   return OK;
}
