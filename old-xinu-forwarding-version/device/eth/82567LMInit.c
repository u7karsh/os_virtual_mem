/* 82567LMInit.c - _82567LMInit */

#include <xinu.h>

local 	status 	_82567LM_acquire_swflag(struct ether *ethptr);
local 	void 	_82567LM_release_swflag(struct ether *ethptr);
local 	status 	_82567LM_check_reset_block(struct ether *ethptr);
local 	status 	_82567LM_read_phy_reg_mdic(struct ether *ethptr, 
			uint32 offset, uint16 *data);
local 	status 	_82567LM_write_phy_reg_mdic(struct ether *ethptr, 
			uint32 offset, uint16 data);
local 	status 	_82567LM_read_phy_reg(struct ether *ethptr, 
			uint32 offset, uint16 *data);
local 	status 	_82567LM_write_phy_reg(struct ether *ethptr, 
			uint32 offset, uint16 data);
local 	status 	_82567LM_read_kmrn_reg(struct ether *ethptr, 
			uint32 offset, uint16 *data);
local 	status 	_82567LM_write_kmrn_reg(struct ether *ethptr, 
			uint32 offset, uint16 data);
local 	status 	_82567LM_init_hw(struct ether *ethptr);
local 	status 	_82567LM_reset_hw(struct ether *ethptr);
local 	void 	_82567LM_configure_rx(struct ether *ethptr);
local 	void 	_82567LM_configure_tx(struct ether *ethptr);

/*------------------------------------------------------------------------
 * _82567LMInit - initialize Intel Hub 10D/82567LM Ethernet NIC
 *------------------------------------------------------------------------
 */
status 	_82567LMInit(
	struct 	ether *ethptr
	)
{
	struct	e1000_tx_desc* txRingptr;
	struct	e1000_rx_desc*	rxRingptr;
	uint16  command;
	int32	i;
	uint32  rar_low, rar_high, bufptr;

	/* Read PCI configuration information */
	/* Read I/O base address */

	pci_bios_read_config_dword(ethptr->pcidev, E1000_PCI_IOBASE,
			(uint32 *)&ethptr->iobase);
	ethptr->iobase &= ~1;
	ethptr->iobase &= 0xffff; /* the low bit is set to indicate I/O */

	/* Read interrupt line number */

	pci_bios_read_config_byte (ethptr->pcidev, E1000_PCI_IRQ,
			(byte *)&(ethptr->dev->dvirq));

	/* Enable PCI bus master, memory access and I/O access */

	pci_bios_read_config_word(ethptr->pcidev, E1000_PCI_COMMAND, 
			&command);
	command |= E1000_PCI_CMD_MASK;
	pci_bios_write_config_word(ethptr->pcidev, E1000_PCI_COMMAND, 
			command);

	/* Read the MAC address */
	
	rar_low = e1000_io_readl(ethptr->iobase, E1000_RAL(0));
	rar_high = e1000_io_readl(ethptr->iobase, E1000_RAH(0));

	for (i = 0; i < E1000_RAL_MAC_ADDR_LEN; i++) 
		ethptr->devAddress[i] = (byte)(rar_low >> (i*8));
	for (i = 0; i < E1000_RAH_MAC_ADDR_LEN; i++)
		ethptr->devAddress[i + 4] = (byte)(rar_high >> (i*8));

	kprintf("MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
			0xff&ethptr->devAddress[0],
			0xff&ethptr->devAddress[1],
			0xff&ethptr->devAddress[2],
			0xff&ethptr->devAddress[3],
			0xff&ethptr->devAddress[4],
			0xff&ethptr->devAddress[5]);

	/* Initialize structure pointers */

	ethptr->rxRingSize = E1000_RX_RING_SIZE;
	ethptr->txRingSize = E1000_TX_RING_SIZE;
	ethptr->isem = semcreate(0);
	ethptr->osem = semcreate(ethptr->txRingSize);

	/* Rings must be aligned on a 16-byte boundary */
	
	ethptr->rxRing = (void *)getmem((ethptr->rxRingSize + 1)
			* E1000_RDSIZE);
	ethptr->txRing = (void *)getmem((ethptr->txRingSize + 1)
			* E1000_TDSIZE);
	ethptr->rxRing = (void *)(((uint32)ethptr->rxRing + 0xf) & ~0xf);
	ethptr->txRing = (void *)(((uint32)ethptr->txRing + 0xf) & ~0xf);
	
	/* Buffers are highly recommended to be allocated on cache-line */
	/* 	size (64-byte for E8400) 				*/
	
	ethptr->rxBufs = (void *)getmem((ethptr->rxRingSize + 1) 
			* ETH_BUF_SIZE);
	ethptr->txBufs = (void *)getmem((ethptr->txRingSize + 1) 
			* ETH_BUF_SIZE);
	ethptr->rxBufs = (void *)(((uint32)ethptr->rxBufs + 0x3f) 
			& ~0x3f);
	ethptr->txBufs = (void *)(((uint32)ethptr->txBufs + 0x3f) 
			& ~0x3f);

	if ( (SYSERR == (uint32)ethptr->rxBufs) || 
	     (SYSERR == (uint32)ethptr->txBufs) ) {
		return SYSERR;
	}

	/* Set buffer pointers and rings to zero */
	
	memset(ethptr->rxBufs, '\0', ethptr->rxRingSize * ETH_BUF_SIZE);
	memset(ethptr->txBufs, '\0', ethptr->txRingSize * ETH_BUF_SIZE);
	memset(ethptr->rxRing, '\0', E1000_RDSIZE * ethptr->rxRingSize);
	memset(ethptr->txRing, '\0', E1000_TDSIZE * ethptr->txRingSize);

	/* Insert the buffer into descriptor ring */
	
	rxRingptr = (struct e1000_rx_desc *)ethptr->rxRing;
	bufptr = (uint32)ethptr->rxBufs;
	for (i = 0; i < ethptr->rxRingSize; i++) {
		rxRingptr->buffer_addr = (uint64)bufptr;
		rxRingptr++;
		bufptr += ETH_BUF_SIZE;
	}

	txRingptr = (struct e1000_tx_desc *)ethptr->txRing;
	bufptr = (uint32)ethptr->txBufs;
	for (i = 0; i < ethptr->txRingSize; i++) {
		txRingptr->buffer_addr = (uint64)bufptr;
		txRingptr++;
		bufptr += ETH_BUF_SIZE;
	}

	/* Reset Packet Buffer Allocation to default */

	e1000_io_writel(ethptr->iobase, E1000_PBA, E1000_PBA_10K);

	/* Reset the NIC to bring it into a known state and initialize it */

	if (_82567LM_reset_hw(ethptr) != OK)
		return SYSERR;

	/* Initialize the hardware */

	if (_82567LM_init_hw(ethptr) != OK)
		return SYSERR;

	/* Configure the TX */

	_82567LM_configure_rx(ethptr);

	/* Configure the RX */

	_82567LM_configure_tx(ethptr);

	/* Enable interrupt */
	
	set_evec(ethptr->dev->dvirq + IRQBASE, (uint32)e1000Dispatch);
	e1000IrqEnable(ethptr);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_acquire_swflag - Acquire software control flag
 *------------------------------------------------------------------------
 */
local status _82567LM_acquire_swflag(
	struct 	ether *ethptr
	)
{
	uint32 extcnf_ctrl, timeout = E1000_PHY_CFG_TIMEOUT;

	while (timeout) {
		extcnf_ctrl = e1000_io_readl(ethptr->iobase, 
					     E1000_EXTCNF_CTRL);
		if (!(extcnf_ctrl & E1000_EXTCNF_CTRL_SWFLAG))
			break;

		MDELAY(1);
		timeout--;
	}

	if (!timeout) {
		return SYSERR;
	}

	timeout = E1000_SW_FLAG_TIMEOUT;

	extcnf_ctrl |= E1000_EXTCNF_CTRL_SWFLAG;
	e1000_io_writel(ethptr->iobase, E1000_EXTCNF_CTRL, extcnf_ctrl);

	while (timeout) {
		extcnf_ctrl = e1000_io_readl(ethptr->iobase, 
					     E1000_EXTCNF_CTRL);
		if (extcnf_ctrl & E1000_EXTCNF_CTRL_SWFLAG)
			break;

		MDELAY(1)
			timeout--;
	}

	if (!timeout) {
		extcnf_ctrl &= ~E1000_EXTCNF_CTRL_SWFLAG;
		e1000_io_writel(ethptr->iobase, E1000_EXTCNF_CTRL, 
				extcnf_ctrl);
		return SYSERR;
	}

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_release_swflag - Release software control flag
 *------------------------------------------------------------------------
 */
local void _82567LM_release_swflag(
	struct 	ether *ethptr
	)
{
	uint32 extcnf_ctrl;

	extcnf_ctrl = e1000_io_readl(ethptr->iobase, E1000_EXTCNF_CTRL);
	extcnf_ctrl &= ~E1000_EXTCNF_CTRL_SWFLAG;
	e1000_io_writel(ethptr->iobase, E1000_EXTCNF_CTRL, extcnf_ctrl);

	return;
}

/*------------------------------------------------------------------------
 * _82567LM_check_reset_block - Check if PHY reset is blocked
 *------------------------------------------------------------------------
 */
local status _82567LM_check_reset_block(
	struct 	ether *ethptr
	)
{
	uint32 fwsm;

	fwsm = e1000_io_readl(ethptr->iobase, E1000_FWSM);

	return (fwsm & E1000_ICH_FWSM_RSPCIPHY) ? OK
		: SYSERR;
}

/*------------------------------------------------------------------------
 * _82567LM_read_phy_reg_mdic - Read MDI control register
 *------------------------------------------------------------------------
 */
local status _82567LM_read_phy_reg_mdic(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	*data
	)
{
	uint32 i, mdic = 0;

	if (offset > E1000_MAX_PHY_REG_ADDRESS) {
		return SYSERR;
	}

	mdic = ((offset << E1000_MDIC_REG_SHIFT) |
		(E1000_82567LM_MDIC_PHY_ADDR << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_READ));

	e1000_io_writel(ethptr->iobase, E1000_MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		DELAY(50);
		mdic = e1000_io_readl(ethptr->iobase, E1000_MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		return SYSERR;
	}
	if (mdic & E1000_MDIC_ERROR) {
		return SYSERR;
	}
	*data = (uint16) mdic;

	return OK;
}

/*------------------------------------------------------------------------
 *  _82567LM_write_phy_reg_mdic - Write MDI control register
 *------------------------------------------------------------------------
 */
local status _82567LM_write_phy_reg_mdic(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	data
	)
{
	uint32 i, mdic = 0;

	if (offset > E1000_MAX_PHY_REG_ADDRESS) {
		return SYSERR;
	}

	mdic = ( ((uint32)data) |
		 (offset << E1000_MDIC_REG_SHIFT) |
		 (E1000_82567LM_MDIC_PHY_ADDR << E1000_MDIC_PHY_SHIFT) |
		 (E1000_MDIC_OP_WRITE) );

	e1000_io_writel(ethptr->iobase, E1000_MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		DELAY(50);
		mdic = e1000_io_readl(ethptr->iobase, E1000_MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		return SYSERR;
	}
	if (mdic & E1000_MDIC_ERROR) {
		return SYSERR;
	}

	return OK;
}

/*------------------------------------------------------------------------
 *  _82567LM_read_phy_reg - Read BM PHY register
 *------------------------------------------------------------------------
 */
local status _82567LM_read_phy_reg(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	*data
	)
{
	uint32 page = offset >> E1000_PHY_PAGE_SHIFT;

	if (_82567LM_acquire_swflag(ethptr) != OK)
		return SYSERR;

	if (offset > E1000_MAX_PHY_MULTI_PAGE_REG)
		if (_82567LM_write_phy_reg_mdic(ethptr, 
					E1000_BM_PHY_PAGE_SELECT, page)
				!= OK)
			return SYSERR;

	if (_82567LM_read_phy_reg_mdic(ethptr,
				E1000_MAX_PHY_REG_ADDRESS & offset, data)
			!= OK)
		return SYSERR;

	_82567LM_release_swflag(ethptr);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_write_phy_reg - Write BM PHY register
 *------------------------------------------------------------------------
 */
local status _82567LM_write_phy_reg(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	data
	)
{
	uint32 page = offset >> E1000_PHY_PAGE_SHIFT;

	if (_82567LM_acquire_swflag(ethptr) != OK)
		return SYSERR;

	if (offset > E1000_MAX_PHY_MULTI_PAGE_REG)
		if (_82567LM_write_phy_reg_mdic(ethptr,
					E1000_PHY_PAGE_SELECT, page)
				!= OK)
			return SYSERR;

	if (_82567LM_write_phy_reg_mdic(ethptr,
				E1000_MAX_PHY_REG_ADDRESS & offset, data)
			!= OK)
		return SYSERR;

	_82567LM_release_swflag(ethptr);
	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_read_kmrn_reg - Read kumeran register
 *------------------------------------------------------------------------
 */
local status _82567LM_read_kmrn_reg(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	*data
	)
{
	uint32 kmrnctrlsta;

	if (_82567LM_acquire_swflag(ethptr) != OK)
		return SYSERR;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
			E1000_KMRNCTRLSTA_OFFSET) | E1000_KMRNCTRLSTA_REN;
	e1000_io_writel(ethptr->iobase, E1000_KMRNCTRLSTA, kmrnctrlsta);

	DELAY(2);

	kmrnctrlsta = e1000_io_readl(ethptr->iobase, E1000_KMRNCTRLSTA);
	*data = (uint16)kmrnctrlsta;

	_82567LM_release_swflag(ethptr);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_write_kmrn_reg - Write kumeran register
 *------------------------------------------------------------------------
 */
local status _82567LM_write_kmrn_reg(
	struct 	ether *ethptr,
	uint32 	offset,
	uint16 	data
	)
{
	uint32 kmrnctrlsta;

	if (_82567LM_acquire_swflag(ethptr) != OK)
		return SYSERR;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
			E1000_KMRNCTRLSTA_OFFSET) | data;
	e1000_io_writel(ethptr->iobase, E1000_KMRNCTRLSTA, kmrnctrlsta);

	DELAY(2);

	_82567LM_release_swflag(ethptr);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_reset_hw - Reset the hardware 
 *------------------------------------------------------------------------
 */
local status _82567LM_reset_hw(
	struct 	ether *ethptr
	)
{
	uint32 ctrl, kab;
	uint32 dev_status;
	uint32 data, loop = E1000_ICH_LAN_INIT_TIMEOUT;
	int32 timeout = E1000_MASTER_DISABLE_TIMEOUT;

	/* Disables PCI_express master access */

	ctrl = e1000_io_readl(ethptr->iobase, E1000_CTRL);
	ctrl |= E1000_CTRL_GIO_MASTER_DISABLE;
	e1000_io_writel(ethptr->iobase, E1000_CTRL, ctrl);

	while (timeout) {
		if (!(e1000_io_readl(ethptr->iobase, E1000_STATUS) &
		      E1000_STATUS_GIO_MASTER_ENABLE))
			break;
		DELAY(100);
		timeout--;
	}

	if (!timeout) {
		return SYSERR;
	}

	/* Masking off all interrupts */

	e1000_io_writel(ethptr->iobase, E1000_IMC, 0xffffffff);

	/* Disable the Transmit and Receive units. */

	e1000_io_writel(ethptr->iobase, E1000_RCTL, 0);
	e1000_io_writel(ethptr->iobase, E1000_TCTL, 0);
	e1000_io_flush(ethptr->iobase);

	MDELAY(10);

	ctrl = e1000_io_readl(ethptr->iobase, E1000_CTRL);

	if (_82567LM_check_reset_block(ethptr) == OK)
		ctrl |= E1000_CTRL_PHY_RST;

	if (_82567LM_acquire_swflag(ethptr) != OK)
		return SYSERR;

	/* Issuing a global reset */

	e1000_io_writel(ethptr->iobase, E1000_CTRL, 
			(ctrl | E1000_CTRL_RST));
	MDELAY(20);

	_82567LM_release_swflag(ethptr);

	if (ctrl & E1000_CTRL_PHY_RST) {
		MDELAY(10);

		do {
			data = e1000_io_readl(ethptr->iobase, E1000_STATUS);
		    	data &= E1000_STATUS_LAN_INIT_DONE;
		    	DELAY(100);
		} while ((!data) && --loop);

		data = e1000_io_readl(ethptr->iobase, E1000_STATUS);
		data &= ~E1000_STATUS_LAN_INIT_DONE;
		e1000_io_writel(ethptr->iobase, E1000_STATUS, data);

		dev_status = e1000_io_readl(ethptr->iobase, E1000_STATUS);
		if (dev_status & E1000_STATUS_PHYRA)
			e1000_io_writel(ethptr->iobase, E1000_STATUS, 
					dev_status & ~E1000_STATUS_PHYRA);

		MDELAY(10);
	}

	e1000_io_writel(ethptr->iobase, E1000_IMC, 0xffffffff);
	e1000_io_readl(ethptr->iobase, E1000_ICR);

	kab = e1000_io_readl(ethptr->iobase, E1000_KABGTXD);
	kab |= E1000_KABGTXD_BGSQLBIAS;
	e1000_io_writel(ethptr->iobase, E1000_KABGTXD, kab);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_init_hw - Initialize the hardware
 *------------------------------------------------------------------------
 */
local status _82567LM_init_hw(
	struct 	ether *ethptr
	)
{
	uint16 	i;
	uint32 	rar_low, rar_high;
	uint32 	ctrl, gcr, reg;
	uint16 	kmrn_data;
	uint16 	mii_autoneg_adv_reg, mii_1000t_ctrl_reg;
	uint16 	phy_data, phy_ctrl, phy_status;

	/* Initialize required hardware bits */

	{
		reg = e1000_io_readl(ethptr->iobase, E1000_CTRL_EXT);
		reg |= (1 << 22);
		e1000_io_writel(ethptr->iobase, E1000_CTRL_EXT, reg);
	    
		reg = e1000_io_readl(ethptr->iobase, E1000_TXDCTL(0));
		reg |= (1 << 22);
		e1000_io_writel(ethptr->iobase, E1000_TXDCTL(0), reg);
	
		reg = e1000_io_readl(ethptr->iobase, E1000_TXDCTL(1));
		reg |= (1 << 22);
		e1000_io_writel(ethptr->iobase, E1000_TXDCTL(1), reg);
	    
		reg = e1000_io_readl(ethptr->iobase, E1000_TARC(0));
		reg |= (1 << 23) | (1 << 24) | (1 << 26) | (1 << 27);
		e1000_io_writel(ethptr->iobase, E1000_TARC(0), reg);

		reg = e1000_io_readl(ethptr->iobase, E1000_TARC(1));
		reg &= ~(1 << 28);
		reg |= (1 << 24) | (1 << 26) | (1 << 30);
		e1000_io_writel(ethptr->iobase, E1000_TARC(1), reg);

		reg = e1000_io_readl(ethptr->iobase, E1000_RFCTL);
		reg |= (E1000_RFCTL_NFSW_DIS | E1000_RFCTL_NFSR_DIS);
		e1000_io_writel(ethptr->iobase, E1000_RFCTL, reg);
	}

	/* Setup the receive address */

	rar_low = rar_high = 0;
	for (i = 1; i < E1000_82567LM_RAR_ENTRIES; i++) {
		e1000_io_writel(ethptr->iobase, E1000_RAL(i), rar_low);
	    	e1000_io_flush(ethptr->iobase);
	    	e1000_io_writel(ethptr->iobase, E1000_RAH(i), rar_high);
	    	e1000_io_flush(ethptr->iobase);
	}

	/* Zero out the Multicast HASH table */

	for (i = 0; i < E1000_82567LM_MTA_ENTRIES; i++)
		e1000_io_writel(ethptr->iobase, E1000_MTA + (i << 2), 0);

	/* Setup link and flow control */

	{
		kprintf("Setup link...\n");
		if (_82567LM_check_reset_block(ethptr) != OK)
			return SYSERR;

		ctrl = e1000_io_readl(ethptr->iobase, E1000_CTRL);
		ctrl |= E1000_CTRL_SLU;
		ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
		e1000_io_writel(ethptr->iobase, E1000_CTRL, ctrl);

		if (_82567LM_write_kmrn_reg(ethptr, 
			E1000_KMRNCTRLSTA_TIMEOUTS, 0xFFFF) != OK)
			return SYSERR;
	    
		if (_82567LM_read_kmrn_reg(ethptr, 
			E1000_KMRNCTRLSTA_INBAND_PARAM, &kmrn_data) != OK)
			return SYSERR;

		kmrn_data |= 0x3F;

		if (_82567LM_write_kmrn_reg(ethptr, 
			E1000_KMRNCTRLSTA_INBAND_PARAM, kmrn_data) != OK)
			return SYSERR;

	    	if (_82567LM_read_phy_reg(ethptr, 
			M88E1000_PHY_SPEC_CTRL, &phy_data) != OK)
			return SYSERR;

		phy_data |= M88E1000_PSCR_AUTO_X_MODE;	
		phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
		phy_data |= E1000_BM_PSCR_ENABLE_DOWNSHIFT;

		if (_82567LM_write_phy_reg(ethptr, 
			M88E1000_PHY_SPEC_CTRL, phy_data) != OK)
			return SYSERR;

	    	if (_82567LM_read_phy_reg(ethptr,
			E1000_PHY_CONTROL, &phy_ctrl) != OK)
			return SYSERR;
		

		phy_ctrl |= E1000_MII_CR_RESET;

		if (_82567LM_write_phy_reg(ethptr, 
			E1000_PHY_CONTROL, phy_ctrl) != OK)
			return SYSERR;
		

		DELAY(1);

		if (_82567LM_read_phy_reg(ethptr,
			E1000_PHY_AUTONEG_ADV, &mii_autoneg_adv_reg) != OK)
			return SYSERR;

	    	if (_82567LM_read_phy_reg(ethptr, 
			E1000_PHY_1000T_CTRL, &mii_1000t_ctrl_reg) != OK)
			return SYSERR;

		mii_autoneg_adv_reg |= (E1000_NWAY_AR_100TX_FD_CAPS |
					E1000_NWAY_AR_100TX_HD_CAPS |
					E1000_NWAY_AR_10T_FD_CAPS   |
					E1000_NWAY_AR_10T_HD_CAPS);
	    
		mii_1000t_ctrl_reg &= ~E1000_CR_1000T_HD_CAPS;
		mii_1000t_ctrl_reg |= E1000_CR_1000T_FD_CAPS;

		mii_autoneg_adv_reg &= ~(E1000_NWAY_AR_ASM_DIR | 
					 E1000_NWAY_AR_PAUSE);

		if (_82567LM_write_phy_reg(ethptr, 
			E1000_PHY_AUTONEG_ADV, mii_autoneg_adv_reg) != OK)
			return SYSERR;

		if (_82567LM_write_phy_reg(ethptr, 
			E1000_PHY_1000T_CTRL, mii_1000t_ctrl_reg) != OK)
			return SYSERR;

		if (_82567LM_read_phy_reg(ethptr, 
			E1000_PHY_CONTROL, &phy_ctrl) != OK)
			return SYSERR;

	    	phy_ctrl |= (E1000_MII_CR_AUTO_NEG_EN | 
			     E1000_MII_CR_RESTART_AUTO_NEG);

		if (_82567LM_write_phy_reg(ethptr,
			E1000_PHY_CONTROL, phy_ctrl) != OK)
			return SYSERR;

		for (;;) {
			if (_82567LM_read_phy_reg(ethptr, 
			    	E1000_PHY_STATUS, &phy_status) != OK)
			DELAY(10);
		    
			if (_82567LM_read_phy_reg(ethptr,
			    	E1000_PHY_STATUS, &phy_status) != OK)
				return SYSERR;
		
			if ((phy_status & E1000_MII_SR_LINK_STATUS) && 
			    (phy_status & E1000_MII_SR_AUTONEG_COMPLETE))
				break;
		
			MDELAY(100);
		}

		ctrl = e1000_io_readl(ethptr->iobase, E1000_CTRL);
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		e1000_io_writel(ethptr->iobase, E1000_CTRL, ctrl);
	}

	/* Set PCI-express capabilities */

	gcr = e1000_io_readl(ethptr->iobase, E1000_GCR);
	gcr &= ~(E1000_PCIE_NO_SNOOP_ALL);
	gcr |= ~(E1000_PCIE_NO_SNOOP_ALL);
	e1000_io_writel(ethptr->iobase, E1000_GCR, gcr);

	return OK;
}

/*------------------------------------------------------------------------
 * _82567LM_configure_rx - Configure Receive Unit after Reset
 *------------------------------------------------------------------------
 */
local void _82567LM_configure_rx(
	struct 	ether *ethptr
	)
{
	uint32 rctl, rxcsum;

	/* Program MC offset vector base */

	rctl = e1000_io_readl(ethptr->iobase, E1000_RCTL);
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | 
		E1000_RCTL_BAM |
		E1000_RCTL_LBM_NO |
		E1000_RCTL_RDMTS_HALF;

	/* Do not Store bad packets, do not pass MAC control frame, 	*/
	/* 	disable long packet receive and CRC strip 		*/
	
	rctl &= ~(E1000_RCTL_SBP |
		  E1000_RCTL_LPE |
		  E1000_RCTL_SECRC |
		  E1000_RCTL_PMCF);
	
	/* Use Legacy description type */
	
	rctl &= ~E1000_RCTL_DTYP_MASK;

	/* Setup buffer sizes */

	rctl &= ~(E1000_RCTL_BSEX |
		  E1000_RCTL_SZ_4096 |
		  E1000_RCTL_FLXBUF_MASK);
	rctl |= E1000_RCTL_SZ_2048;

	/* Set the Receive Delay Timer Register, let driver be notified */
	/* 	immediately each time a new packet has been stored in 	*/
	/* 	memory 							*/

	e1000_io_writel(ethptr->iobase, E1000_RDTR, E1000_RDTR_DEFAULT);
	e1000_io_writel(ethptr->iobase, E1000_RADV, E1000_RADV_DEFAULT);

	/* IRQ moderation */

	e1000_io_writel(ethptr->iobase, E1000_ITR, 
			1000000000 / (E1000_ITR_DEFAULT * 256));

	/* Setup the HW Rx Head and Tail Descriptor Pointers, the Base 	*/
	/* 	and Length of the Rx Descriptor Ring 			*/

	e1000_io_writel(ethptr->iobase, E1000_RDBAL(0), 
			(uint32)ethptr->rxRing);
	e1000_io_writel(ethptr->iobase, E1000_RDBAH(0), 0);
	e1000_io_writel(ethptr->iobase, E1000_RDLEN(0), 
			E1000_RDSIZE * ethptr->rxRingSize);
	e1000_io_writel(ethptr->iobase, E1000_RDH(0), 0);
	e1000_io_writel(ethptr->iobase, E1000_RDT(0), 
			ethptr->rxRingSize - E1000_RING_BOUNDARY);

	/* Disable Receive Checksum Offload for TCP and UDP */

	rxcsum = e1000_io_readl(ethptr->iobase, E1000_RXCSUM);
	rxcsum &= ~E1000_RXCSUM_TUOFL;
	e1000_io_writel(ethptr->iobase, E1000_RXCSUM, rxcsum);

	e1000_io_writel(ethptr->iobase, E1000_RCTL, rctl);
}

/*------------------------------------------------------------------------
 * e1000_configure_tx - Configure Transmit Unit after Reset
 *------------------------------------------------------------------------
 */
local void _82567LM_configure_tx(
	struct 	ether *ethptr
	)
{
	uint32 	tctl, tipg, txdctl;
	uint32 	ipgr1, ipgr2;

	/* Set the transmit descriptor write-back policy for both queues */

	txdctl = e1000_io_readl(ethptr->iobase, E1000_TXDCTL(0));
	txdctl &= ~E1000_TXDCTL_WTHRESH;
	txdctl |= E1000_TXDCTL_GRAN;
	e1000_io_writel(ethptr->iobase, E1000_TXDCTL(0), txdctl);
	txdctl = e1000_io_readl(ethptr->iobase, E1000_TXDCTL(1));
	txdctl &= ~E1000_TXDCTL_WTHRESH;
	txdctl |= E1000_TXDCTL_GRAN;
	e1000_io_writel(ethptr->iobase, E1000_TXDCTL(1), txdctl);

	/* Program the Transmit Control Register */
	
	tctl = e1000_io_readl(ethptr->iobase, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_RTLC |
		E1000_TCTL_EN |
		E1000_TCTL_PSP |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

	/* Set the default values for the Tx Inter Packet Gap timer */
	
	tipg = E1000_TIPG_IPGT_COPPER_DEFAULT; 	/*  8  */
	ipgr1 = E1000_TIPG_IPGR1_DEFAULT;	/*  8  */
	ipgr2 = E1000_TIPG_IPGR2_DEFAULT;	/*  6  */
	tipg |= ipgr1 << E1000_TIPG_IPGR1_SHIFT;
	tipg |= ipgr2 << E1000_TIPG_IPGR2_SHIFT;
	e1000_io_writel(ethptr->iobase, E1000_TIPG, tipg);

	/* Set the Tx Interrupt Delay register */
	
	e1000_io_writel(ethptr->iobase, E1000_TIDV, E1000_TIDV_DEFAULT);
	e1000_io_writel(ethptr->iobase, E1000_TADV, E1000_TADV_DEFAULT);

	/* Setup the HW Tx Head and Tail descriptor pointers */
	
	e1000_io_writel(ethptr->iobase, E1000_TDBAL(0), 
			(uint32)ethptr->txRing);
	e1000_io_writel(ethptr->iobase, E1000_TDBAH(0), 0);
	e1000_io_writel(ethptr->iobase, E1000_TDLEN(0), 
			E1000_TDSIZE * ethptr->txRingSize);
	e1000_io_writel(ethptr->iobase, E1000_TDH(0), 0);
	e1000_io_writel(ethptr->iobase, E1000_TDT(0), 0);

	e1000_io_writel(ethptr->iobase, E1000_TCTL, tctl);
}
