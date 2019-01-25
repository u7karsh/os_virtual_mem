# Physical Memory Layout

A free list for the PD/PT region similar to XINU’s freelist for memory has been created. We assign directory entries in a
hierarchical fashion and assign PTs for corresponding directory entries. For physical addresses till the
beginning of PD/PT region, we have flat mappings i.e., virtual address equals physical address. For
default XINU’s configuration, this accounts to 8-page tables. System processes will require only these
mappings, but user processes will additionally have mappings for the heap allocated memory as well
(the number will depend on the amount of memory the process requests). In our implementation, we
assume that all system process are sudo/superuser/kernel/privileged processes, and hence they share the
same page directory that has mappings till virtual stack.

# Where is paging enabled and how?

Paging is enabled at the end of system initialization. (the end of the sysinit() function in
initialize.c). It is initialized by setting the PG bit in the register. It is done by writing 1 to the LSB and MSB on the control register CR0.

The LSB corresponds to the PE bit

Where protected mode is the mode which “allows system software to use features such as virtual
memory, paging and safe multi-tasking designed to increase an operating system's control
over application software.”

# How do you need to modify process creation to support paging?

1. We need to create the translation system (see below) for the process and write the base
directory’s address (PD/PT frame address) to control register CR3. The control register CR3
holds the value that the hardware page walker will use once paging is enabled.

2. Creating a translation system entails the following:
    a. Create a directory for the process. Keep this address saved.
    b. Create page tables which will contain mappings to flat map the static segments (text, data, bss, etc)
    c. We will use create_directory and create_table entry functions. These will allocate a directory and initialize the relevant entries. (These will create mappings corresponding to the flat mapped region (static segments)).
    d. Write this address onto the pdbr of the process. (We have an entry in the proctab for this).
    
3. After Steps 1 and 2, we will use the enable_paging() function given in the control_regs.c file to
enable paging.

4. Since changing mappings is a privileged operation, we need to enter kernel mode. Entering kernel mode refers to setting the pdbr of system procees (null proc) (this does not hold true in
case of virtual stack; will be explained later) Process Termination

# How do you need to modify process termination to support paging?

##### At process termination
a) For system processes we do not need to clear page table entries as all system processes share the same page tables (belonging to null proc).
b) For user processes, we will clear the page table and page directory entries and free the memory used by the page tables and the pages themselves. (we do not free the pages which are flat-mapped). Thus, at termination, all present virtual pages are freed to FFS or swap and the page tables and directories are destroyed.

##### Context Switch - What should be done at context switch to support paging?
a) The hardware register CR3 should be populated with the pdbr of the incoming process. We have chosen to do this in resched.c (for virtual stack, it will be explained later). The incoming process will have the mappings to ctxsw.S code, so that should work fine.
b) Invalidate TLB entries. (We will make this a part of write_pdbr() function, because everytime pdbr is changed to a different address, TLB should be invalidated.) Heap allocation, deallocation and access

# What should be done at heap allocation, deallocation and when the heap is accessed?

We will maintain a free list of the FFS area in the physical memory and implement functions to get a page and free a page in that area. At heap allocation(vmalloc), we will give it the required virtual address, and allocate space for the page tables corresponding to those virtual addresses. Note that these page table entries will have present=0. We also introduce a new bit is_vmalloc out of the available bits which we will set to 1 when we vmalloc a page. This is because actual physical memory is not being reserved at the time of vmalloc. (helps in detecting illegal memory access; is similar to valid bit discussed in class)

At deallocation, we mark the page table entries corresponding to the address as not present. Additionally, if physical frames were reserved for the addresses, we will free them using our interface to the ffs space. (free ffs space it was allocated).

When heap is accessed for the first time, we get a pagefault as the present bit was un-set. In the handle we allocate a free space from the ffs region, and map the Physical frames to the virtual addresses we allocated in vmalloc. (Populate the page table entries with actual physical frame numbers). 

Important note is that we will switch to a process which has all mappings (of ffs area) when doing any such operation related to ffs space. That privileged process will get the pdbr of the current process so that it can write to the page table entries of this process. (kernel mode mentioned earlier) Also, we will keep track of the heap allocated globally to keep it within its limit. 

# In which circumstances will the hardware raise a page fault?
A page fault can occur due to the following reasons:
a. First access to the page due to lazy allocation. In this case present bit will be 0 and vmalloc bit will be 1.
b. Accessing a swapped-out page as its present bit will be 0 and is_swapped will be 1. At this time, we place back the frame in FFS region.
c. Protection violation but we will not see it because we are in privileged mode whenever we do a privileged operation.
d. Accessing a page that was never malloced. We display a segmentation fault error message and halt the system.

# What operations should be performed by the page fault handler depending on the circumstances under which it is invoked?

At page fault, an error code is pushed onto the stack. We pop it before servicing the interrupt. We then enter kernel mode to perform the privileged operation i.e., page fault handling. For reasons (a) and (b) mentioned in the previous section, we do the following:
a. Attempt to get an ffs frame.
b. Check if the ffs frame allocation was a success.
    i) If it was, we simply copy the contents from the swapped location (if any) and map it with the ffs frame’s address onto the faulting address’s PTE.
    ii) If it wasn’t, we randomly choose an ffs frame to swap out to disk, update the PTEs corresponding to that random ffs frame, and swap the contents between the swap area and this random address page and map this ffs frame’s address to the faulting address’s PTE.

# Swapping Design
1. We have an is_swapped bit from one of the available PTE bits. This defaults to 0 (not swapped).
2. If an allocated page is accessed for the first time and ffs space is full, we create space for it by randomly evicting an ffs frame to disk/swap memory. Updates to PTEs follow (we set the is_swapped bit and put the address of the swap area frame here). We then swap the contents between the swap area and this random address’s page and map this ffs frame’s address to the faulting address’s PTE.
3. While handling page fault, if we come across a page for which is_swapped is set, we need to swap it back onto the ffs space. We will follow the steps mentioned in part (b) of previous section.
4. Hardware sets the dirty bit in the PTE of a page if a write to it happens. When this page is being swapped, contents are written if and only if the copy in FFS is not clean (with respect to swapped data). Note that the swap space is always allocated (except when there is no space and we need to evict someone who is in FFS as well). Whether the contents of ffs frame are written to it or not is decided by the dirty bit.

##### NOTE: 
Our implementation performs an in-place swap on accessing a swapped-out frame if swap if full with just swapped out frames and ffs is also full. Thus, the default configuration requires 2048 as the minimum possible size for swap memory to have a deadlock free implementation. 

# Handling Virtual Free List
In order to simplify our development, we did not implement a virtual free list (as spec did not force anything on us). We instead have a max page counter that increments at each malloc. Thus, we always return a new virtual address on vmalloc which causes lots of fragmentation. This might lead to PD/PT region being exhausted as we free PD/PT on kill. Also, if this page counter hits 4G address, we will go in syserr.

# Virtual Stack
Implementing virtual stack is not straightforward as local variables inside a function reside in stack.
A few differences in the virtual stack as compared to the virtual heap:
1. Address that is being returned from getvirtualstack function should be the last address of the page rather than the first address as in heaps.
2. Virtual stacks don't have lazy allocation as it is accessed in vcreate where we disable the interrupts. Hence, no pagefault_handler invocation

However, the biggest catch that we found was with the current working set of stack memory and virtual mappings. In user mode, we can't create/edit page mappings. For this, you need to use the kernel's page directory/tables. But, kernel directory/tables don't contain mappings corresponding to the virtual stack of the running process hence we encounter a general protection fault.

Effectively, we can't go into kernel mode by simply updating the PDBR register pointing to a kernel directory as you won't have the current working stack's mappings on it. 

In order to get this working, we need to create a new system process that performs the privileged task (like vmalloc, vfree, and getvirtualstack) for us. This way the current working stack will be a system stack and not a virtual stack during the operation. This sounds something similar to what modern operating systems might be doing where they do a context switch to kernel mode, perform the task, and context switch back to user mode.

We implemented the above-mentioned mechanis. In order to simplify our design, we had to make the
following assumptions:
1. There is a separate region after swap memory dedicated for the virtual stack (i.e., we are not using FSS region)
2. Virtual stack pages are not swapped out to disk

Even though calls like resched, kill, etc., are supposed to be kernel calls, current implementation of XINU does not allow us to do that (no boundary between kernel and user processes). Thus, for privileged operation in such calls, we have an auxiliary array defined in global memory which is used as a stack. We moved pdbr updates in ctxsw.S once the registers are pushed onto the stack and before setting the new stack pointer (we save the value of sp onto ebx as it was a function argument which again is a member of stack). Essentially, privileged operations are done by:

1. Creating a new system process with higher priority; wait for it to complete the task (eg., vmalloc, vfree)
2. Updating current stack to an auxiliary stack which belongs to the flat mapped memory, changing pdbr to null proc, performing the task and doing an inverse of the process. Also, we need to make sure that in privileged mode, we don’t use any local variable created in the function before entering the mode as that will not be a part of the auxiliary stack memory.


This is a VM running Linux used for software development -- it contains Xinu sources
and a text editor (e.g., vi) that can used to modify them.  It also contains a C
compiler/assembler/linker (gcc) used to produce a binary image.  It also runs a
DHCP server as well as a TFTP server that are both connected to an internal network.
Once an image has been produced, it is copied to /srv/tftp/xinu.boot and a backend
VM is powered on.  The backend VM is configured to use PXE boot, which means it
broadcasts a DHCP request that is answered by the server.  The response tells the
backend machine to download file "xinu.grub" -- a copy of the grub boot program set
up to boot Xinu.  When it runs, grub sets up the VM in 32-bit "protected" mode,
downloads Xinu, and branches to symbol "start".  Intel chips always boot in 16-bit
"real" mode (as if we were living in the 1980s), and changing to 32-bit mode is painful
because it involves CPU-specific steps.  So, we let grub handle the details.

Interestingly, although both VMs have access to the global Internet, there is no
problem with a TFTP or DHCP server because the servers only run on an "internal"
network (i.e., a virtual network that is only visible to VMs inside of Vbox).
To obtain Internet access, each VM is configured with two network adapters.  The
diagram below shows how various components in the VMs use the two networks.
Xinu only has a driver for an Intel 82545EM Ethernet device.  Therefore, it only
looks for such a device, and only finds Adapter 2 (i.e., it cannot send or
receive packets over the internal network).  Like a commerical NAT box (i.e., a
wireless router), the NAT box runs a DHCP server and provides IP addresses from a
private address space.  If more than one backend VM is powered on, they will all
obtain the same Xinu image as other backend VMs, but each will have a unique IP
address, so they can communicate with one another.


           This VM (runs Linux)                      Backend VM (runs Xinu)

 ----------------------------------------     ---------------------------------------
|                                        |   |                                       |
|  ---------       --------   --------   |   |                       ----------      |
| |  Other  |     |  DHCP  | |  TFTP  |  |   |                      |   Xinu   |     |
| |  Linux  |     | server | | server |  |   |                       ----------      |
| | network |      --------   --------   |   |  ----------               |           |
| |  comm.  |         |          |       |   | | PXE boot |              |           |
|  ---------          |          |       |   |  ----------               |           |
|     |                ----      |       |   |      |                    |           |
|     |                    |     |       |   |      |                    |           |
|  -----------          -----------      |   |  -----------          -----------     |
| | Adapter 2 |        | Adapter 1 |     |   | | Adapter 1 |        | Adapter 2 |    |
|  -----------          -----------      |   |  -----------          -----------     |
|      |                    |            |   |      |                    |           |
|      |                    |            |   |      |                    |           |
 ------|--------------------|------------     ------|--------------------|------------
       |                    |                       |                    |
       |                    |  internal network     |                    |
       |                 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                 |
       |                                                                 |
       |            Private addresses "natted" to the Internet           |
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                                          |
                                          |
                                       -------
                                      |  NAT  |
                                       -------
                                          |
                                          V
                                  To the global Internet


To try Xinu, navigate to xinu/compile, and run make, which will link an image and
copy the result to /srv/tftp/xinu.boot.  Then run:

	sudo minicom --color=on

Once minicom starts, power on the backend VM.  It will PXE-boot grub, which will
then boot xinu.boot.  All output from Xinu will appear in the minicom window, and
input typed to the minicom window will be sent to Xinu.

The software development process repeats the following steps:

	1. Change Xinu.  For example,  navigate to the system directory, and add
	   the following to main.c: 

		kprintf("Hello world. I'm trying something new.\n");

	2. Navigate to the compile directory and run make.

	3. Run minicom --color=on

	4. Double click on the backend VM.

	5. Once you are finished, power down the backend VM.

