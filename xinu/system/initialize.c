/* initialize.c - nulluser, sysinit */

/* Handle system initialization and become the null process */

#include <xinu.h>
#include <string.h>

extern	void	start(void);	/* Start of Xinu code			*/
extern	void	*_end;		/* End of Xinu code			*/

/* Function prototypes */

extern	void main(void);	/* Main is the first process created	*/
static	void sysinit(); 	/* Internal system initialization	*/
extern	void meminit(void);	/* Initializes the free memory list	*/
local	process startup(void);	/* Process to finish startup tasks	*/

/* Declarations of major kernel variables */

struct	procent	proctab[NPROC];	/* Process table			*/
struct	sentry	semtab[NSEM];	/* Semaphore table			*/
struct	memblk	memlist;	/* List of free memory blocks		*/
struct	memblk	pdptlist;	/* Head of PD/PT list	*/
struct	memblk	ffslist;	/* Head of ffs list	*/

/* Active system status */

int	prcount;		/* Total number of live processes	*/
pid32	currpid;		/* ID of currently executing process	*/

/* Control sequence to reset the console colors and cusor positiion	*/

#define	CONSOLE_RESET	" \033[0m\033[2J\033[;H"

void printmem(struct	memblk	*_memptr, char* name){
	uint32	free_mem;		/* Total amount of free memory	*/
   struct	memblk	*memptr;

   free_mem = 0;
   for (memptr = _memptr; memptr != NULL; memptr = memptr->mnext) {
      free_mem += memptr->mlength;
   }
   kprintf("%10d bytes of free memory.  %s:\n", free_mem, name);
   for (memptr = _memptr; memptr != NULL; memptr = memptr->mnext) {
      kprintf("           [0x%08X to 0x%08X]\n",
            (uint32)memptr, ((uint32)memptr) + memptr->mlength - 1);
   }
}

/*------------------------------------------------------------------------
 * nulluser - initialize the system and become the null process
 *
 * Note: execution begins here after the C run-time environment has been
 * established.  Interrupts are initially DISABLED, and must eventually
 * be enabled explicitly.  The code turns itself into the null process
 * after initialization.  Because it must always remain ready to execute,
 * the null process cannot execute code that might cause it to be
 * suspended, wait for a semaphore, put to sleep, or exit.  In
 * particular, the code must not perform I/O except for polled versions
 * such as kprintf.
 *------------------------------------------------------------------------
 */

void	nulluser()
{	
	
	/* Initialize the system */

	sysinit();

	/* Output Xinu memory layout */
   printmem(ffslist.mnext, "FFS List");
   printmem(pdptlist.mnext, "PD/PT List");
   printmem(memlist.mnext, "Free List");

	kprintf("%10d bytes of Xinu code.\n",
		(uint32)&etext - (uint32)&text);
	kprintf("           [0x%08X to 0x%08X]\n",
		(uint32)&text, (uint32)&etext - 1);
	kprintf("%10d bytes of data.\n",
		(uint32)&ebss - (uint32)&data);
	kprintf("           [0x%08X to 0x%08X]\n\n",
		(uint32)&data, (uint32)&ebss - 1);

	/* Enable interrupts */

	enable();

	/* Initialize the network stack and start processes */

	net_init();

	/* Create a process to finish startup and start main */

	resume(create((void *)startup, INITSTK, INITPRIO,
					"Startup process", 0, NULL));

	/* Become the Null process (i.e., guarantee that the CPU has	*/
	/*  something to run when no other process is ready to execute)	*/

	while (TRUE) {
		;		/* Do nothing */
	}

}


/*------------------------------------------------------------------------
 *
 * startup  -  Finish startup takss that cannot be run from the Null
 *		  process and then create and resume the main process
 *
 *------------------------------------------------------------------------
 */
local process	startup(void)
{
	uint32	ipaddr;			/* Computer's IP address	*/
	char	str[128];		/* String used to format output	*/


	/* Use DHCP to obtain an IP address and format it */

	ipaddr = getlocalip();
	if ((int32)ipaddr == SYSERR) {
		kprintf("Cannot obtain an IP address\n");
	} else {
		/* Print the IP in dotted decimal and hex */
		ipaddr = NetData.ipucast;
		sprintf(str, "%d.%d.%d.%d",
			(ipaddr>>24)&0xff, (ipaddr>>16)&0xff,
			(ipaddr>>8)&0xff,        ipaddr&0xff);
	
		kprintf("Obtained IP address  %s   (0x%08x)\n", str,
								ipaddr);
	}

	/* Create a process to execute function main() */

	resume(create((void *)main, INITSTK, INITPRIO,
					"Main process", 0, NULL));

	/* Startup process exits at this point */

	return OK;
}


/*------------------------------------------------------------------------
 *
 * sysinit  -  Initialize all Xinu data structures and devices
 *
 *------------------------------------------------------------------------
 */
static	void	sysinit()
{
	int32	i;
	struct	procent	*prptr;		/* Ptr to process table entry	*/
	struct	sentry	*semptr;	/* Ptr to semaphore table entry	*/
   pdbr_t null_pdbr;
   uint32 start_page, end_page;
   uint32 start_dir, end_dir;
   pd_t *dir;
   uint32 extend_from;

	/* Reset the console */

	kprintf(CONSOLE_RESET);
	kprintf("\n%s\n\n", VERSION);

	/* Initialize the interrupt vectors */

	initevec();
	
	/* Initialize free memory list */
	
	meminit();

   /* Initialize paging */
   init_paging();

	/* Initialize system variables */

	/* Count the Null process as the first process in the system */

	prcount = 1;

	/* Scheduling is not currently blocked */

	Defer.ndefers = 0;

   // Add page directory to NULL proc
   // this will create mappings for:
   // text, bss, data, heap, stack, and pd/pt
   null_pdbr = create_directory();

   // Create mappings for:
   // ffs, and swap
   start_page  = (uint32)minffs / PAGE_SIZE;
   end_page    = ceil_div( ((uint32)maxffs), PAGE_SIZE );
   start_dir   = start_page / N_PAGE_ENTRIES;
   end_dir     = ceil_div( end_page, N_PAGE_ENTRIES );
   dir         = (pd_t*)(null_pdbr.pdbr_base << PAGE_OFFSET_BITS);
   for(i = start_dir; i <= end_dir; i++){
      if( i == (n_static_pages - 1) ){
         extend_from = (ceil_div( ((uint32)maxpdpt), PAGE_SIZE )) % N_PAGE_ENTRIES; // 256
         // Extend the entries in an already existing page table
         create_pagetable_entries((uint32)dir[i].pd_base, (uint32)minffs >> PAGE_OFFSET_BITS, extend_from, N_PAGE_ENTRIES - extend_from );
      } else{
         // Create a new directory entry
         create_directory_entry(&dir[i], -1, i*N_PAGE_ENTRIES, 0, N_PAGE_ENTRIES);
      }
   }
   
   // Create mapping for FFS region and map onto nullproc
   write_cr3(*((unsigned int*)&null_pdbr));
	
	/* Initialize process table entries free */

	for (i = 0; i < NPROC; i++) {
		prptr = &proctab[i];
		prptr->prstate = PR_FREE;
		prptr->prname[0] = NULLCH;
		prptr->prstkbase = NULL;
		prptr->prprio = 0;
	}

	/* Initialize the Null process entry */	

	prptr = &proctab[NULLPROC];
	prptr->prstate = PR_CURR;
	prptr->prprio = 0;
	strncpy(prptr->prname, "prnull", 7);
	prptr->prstkbase = getstk(NULLSTK);
	prptr->prstklen = NULLSTK;
	prptr->prstkptr = 0;
   prptr->pdbr = null_pdbr;
	currpid = NULLPROC;

	/* Initialize semaphores */

	for (i = 0; i < NSEM; i++) {
		semptr = &semtab[i];
		semptr->sstate = S_FREE;
		semptr->scount = 0;
		semptr->squeue = newqueue();
	}

	/* Initialize buffer pools */

	bufinit();

	/* Create a ready list for processes */

	readylist = newqueue();


	/* initialize the PCI bus */

	pci_init();

	/* Initialize the real time clock */

	clkinit();

	for (i = 0; i < NDEVS; i++) {
		init(i);
	}

   // Enable paging
   enable_paging();

	return;
}

int32	stop(char *s)
{
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while(1)
		/* Empty */;
}

int32	delay(int n)
{
	DELAY(n);
	return OK;
}