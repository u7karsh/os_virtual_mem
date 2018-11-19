/* ethInit.c - ethInit */

#include <xinu.h>

struct	ether	ethertab[Neth];		/* Ethernet control blocks 	*/

/*------------------------------------------------------------------------
 * ethInit - Initialize Ethernet device structures
 *------------------------------------------------------------------------
 */
devcall	ethInit (
	  struct dentry *devptr
	)
{
	struct	ether 	*ethptr;
	int32	dinfo;			/* device information		*/

	/* Initialize structure pointers */

	ethptr = &ethertab[devptr->dvminor];
	
	memset(ethptr, '\0', sizeof(struct ether));
	ethptr->dev = devptr;
	ethptr->csr = devptr->dvcsr;
	ethptr->state = ETH_STATE_DOWN;
	ethptr->mtu = ETH_MTU;
	ethptr->errors = 0;
	ethptr->addrLen = ETH_ADDR_LEN;
	ethptr->rxHead = ethptr->rxTail = 0;
	ethptr->txHead = ethptr->txTail = 0;

	if ((dinfo = find_pci_device(INTEL_82545EM_DEVICE_ID,
				     INTEL_VENDOR_ID, 0))
		   != SYSERR) {
		kprintf("Found Intel 82545EM Ethernet NIC\n");

		ethptr->type = NIC_TYPE_82545EM;
		ethptr->pcidev = dinfo;

		/* Initialize function pointers */
		
		devtab[ETHER0].dvread = (void *)e1000Read;
		devtab[ETHER0].dvwrite = (void *)e1000Write;
		devtab[ETHER0].dvcntl = (void *)e1000Control;
		devtab[ETHER0].dvintr = (void *)e1000Dispatch;

		if (_82545EMInit(ethptr) == SYSERR) {
			kprintf("Failed to initialize Intel 82545EM NIC\n");
			return SYSERR;
		}
	} else if ((dinfo = find_pci_device(INTEL_82567LM_DEVICE_ID,
					    INTEL_VENDOR_ID, 0))
			  != SYSERR) {
		kprintf("Found Intel 82567LM Ethernet NIC\n");

		ethptr->type = NIC_TYPE_82567LM;
		ethptr->pcidev = dinfo;

		/* Initialize function pointers */
		
		devtab[ETHER0].dvread = (void *)e1000Read;
		devtab[ETHER0].dvwrite = (void *)e1000Write;
		devtab[ETHER0].dvcntl = (void *)e1000Control;
		devtab[ETHER0].dvintr = (void *)e1000Dispatch;

		if (_82567LMInit(ethptr) == SYSERR) {
			kprintf("Failed to initialize Intel 82567LM NIC\n");
			return SYSERR;
		}

	} else {
		kprintf("No recognized PCI Ethernet NIC found\n");
		return SYSERR;
	}
	
	/* Ethernet interface is active from here */

	ethptr->state = ETH_STATE_UP;

	return OK;
}
