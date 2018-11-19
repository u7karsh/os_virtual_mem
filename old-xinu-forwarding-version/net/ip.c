/* ip.c - ip_in, ip_send, ip_local, ip_out, ip_route, ipcksum, ip_hton,	*/
/*		ip_ntoh, ipout, ip_enqueue				*/

#include <xinu.h>

struct	iqentry	ipoqueue;

/*------------------------------------------------------------------------
 * ip_in - handle an IP packet that has arrived over a network
 *------------------------------------------------------------------------
 */

void	ip_in(
	  struct netpacket *pktptr		/* ptr to a packet	*/
	)
{
	int32	iface;			/* index into interface table	*/
	struct	ifentry	*ifptr;		/* ptr to interface entry	*/
	int32	icmplen;		/* length of ICMP message	*/

	/* Verify checksum */

	if (ipcksum(pktptr) != 0) {
		kprintf("IP header checksum failed\n\r");
		freebuf((char *)pktptr);
		return;
	}

	/* Convert IP header fields to host order */

	ip_ntoh(pktptr);

	/* Insure version and length are valid */

	if (pktptr->net_ipvh != 0x45) {
		kprintf("IP version failed\n\r");
		freebuf((char *)pktptr);
		return;
	}

	/* Obtain interface from which packet arrived */

	iface = pktptr->net_iface;
	ifptr = &if_tab[iface];

	/* Discard packets on lab network sent to or from 192.168.0.0/16 */

	if (iface == 0) {
		if ( ( (pktptr->net_ipsrc & 0xffff0000) == 0xc0a80000) ||
		     ( (pktptr->net_ipdst & 0xffff0000) == 0xc0a80000) ) {
			freebuf((char *)pktptr);
			return;
		}
	}

	/* Verify encapsulated prototcol checksums and then convert	*/
	/*	the encapsulated headers to host byte order		*/

	switch (pktptr->net_ipproto) {

	    case IP_UDP:
		/* skipping UDP checksum for now */
		udp_ntoh(pktptr);
		break;

	    case IP_ICMP:
		icmplen = pktptr->net_iplen - IP_HDR_LEN;
		if (icmp_cksum((char *)&pktptr->net_ictype,icmplen) != 0){
			freebuf((char *)pktptr);
			return;
		}
		icmp_ntoh(pktptr);
		break;

	    default:
		break;
	}

	/* Deliver 255.255.255.255 to local stack */

	if (pktptr->net_ipdst == IP_BCAST) {
		ip_local(pktptr);
		return;
	}

	/* If interface does not yet have a valid address, accept on	*/
	/*	interface 0 (DHCP reply) and drop on others		*/

	if (!ifptr->if_ipvalid) {
		if (iface == 0) {
			ip_local(pktptr);
			return;
		} else {
			freebuf((char *)pktptr);
			return;
		}
	}

	/* For packets that arrive from the Internet, use NAT to	*/
	/*	determine whether they should be processed locally or	*/
	/*	translated via NAT and sent to an Othernet host		*/

	if (iface == 0) {
		nat_in(pktptr);
		return;
	}

	/* For packets that arrive from an Othernet, send to the local	*/
	/*	stack if they are destined for the machine's interface	*/
	/*	and send to NAT otherwise				*/

	if ( (pktptr->net_ipdst == ifptr->if_ipucast) ||
	     (pktptr->net_ipdst == ifptr->if_ipbcast)   ) {
		ip_local(pktptr);
		return;
	} else {

		/* Use NAT for Othernet packets sent to the Internet */
		nat_out(pktptr);
		return;
	}
}


/*------------------------------------------------------------------------
 * ip_send - send an outgoing IP datagram from the local stack
 *------------------------------------------------------------------------
 */

status	ip_send(
	  struct netpacket *pktptr	/* ptr to packet		*/
	)
{
	intmask	mask;			/* saved interrupt mask		*/
	int32	iface;			/* index into interface table	*/
	uint32	dest;			/* destination of the datagram	*/
	struct	ifentry	*ifptr;		/* ptr to interface entry	*/
	int32	retval;			/* return value from functions	*/
	uint32	nxthop;			/* next-hop address		*/

	mask = disable();

	if (ifprime < 0) {
		restore(mask);
		return SYSERR;		/* no interface is active	*/
	}

	/* Extract the interface number from the packet buffer */

	iface = pktptr->net_iface;
	if ( (iface<0) || (iface>NIFACES) ||
				(if_tab[iface].if_state !=IF_UP)) {
		panic("ip_send: bad interface number in packet\n");
	}
	ifptr = &if_tab[iface];

	/* Pick up the packet destination */

	dest = pktptr->net_ipdst;

	/* Loop back destination 127.0.0.0/8 */

	if ((dest&0xff000000) == 0x7f000000) {
		ip_local(pktptr);
		restore(mask);
		return OK;
	}

	/* Loop back the interface IP unicast address */

	if (dest == ifptr->if_ipucast) {
		ip_local(pktptr);
		restore(mask);
		return OK;
	}

	/* Broadcast destination 255.255.255.255 */

	if ( (dest == IP_BCAST) ||
	     (dest == ifptr->if_ipbcast) ) {
		memcpy(pktptr->net_ethdst, ifptr->if_macbcast,
							ETH_ADDR_LEN);
		retval = ip_out(iface, pktptr);
		restore(mask);
		return retval;
	}

	/* See if on local network and resolve the IP address */


	if ( (dest & ifptr->if_ipmask) == ifptr->if_ipprefix) {

		/* Next hop is the destination itself */
		nxthop = dest;

	} else {

		/* Next hop is default router on the network */
		nxthop = ifptr->if_iprouter;

	}

	if (nxthop == 0) {	/* dest invalid or no default route	*/
		freebuf((char *)pktptr);
		return SYSERR;
	}

	/* Resolve the next-hop address to get a MAC address */

	retval = arp_resolve(iface, nxthop, pktptr->net_ethdst);
	if (retval != OK) {
		freebuf((char *)pktptr);
		return SYSERR;
	}

	retval = ip_out(iface, pktptr);
	restore(mask);
	return retval;
}


/*------------------------------------------------------------------------
 * ip_local - deliver an IP datagram to the local stack
 *------------------------------------------------------------------------
 */
void	ip_local(
	  struct netpacket *pktptr	/* ptr to packet		*/
	)
{
	switch (pktptr->net_ipproto) {

	    case IP_UDP:
		udp_in(pktptr);
		return;

	    case IP_ICMP:
		icmp_in(pktptr);
		return;

	    default:
		freebuf((char *)pktptr);
		return;
	}
}


/*------------------------------------------------------------------------
 *  ip_out - transmit an outgoing IP datagram
 *------------------------------------------------------------------------
 */
status	ip_out(
	  int32  iface,			/* interface to use		*/
	  struct netpacket *pktptr	/* ptr to packet		*/
	)
{
	struct	ifentry	*ifptr;		/* ptr to interface entry	*/
	uint16	cksum;			/* checksum in host byte order	*/
	int32	len;			/* length of ICMP message	*/	
	int32	pktlen;			/* length of entire packet	*/
	int32	retval;			/* value returned by write	*/

	ifptr = &if_tab[iface];
	if (ifptr->if_state != IF_UP) {
		freebuf((char *)pktptr);
		return SYSERR;
	}

	/* Compute total packet length */

	pktlen = pktptr->net_iplen + ETH_HDR_LEN;

	/* Convert encapsulated protocol to network byte order */

	switch (pktptr->net_ipproto) {

	    case IP_UDP:

			pktptr->net_udpcksum = 0;
			udp_hton(pktptr);

			/* ...skipping UDP checksum computation */

			break;

	    case IP_ICMP:
			icmp_hton(pktptr);

			/* Compute ICMP checksum */

			pktptr->net_iccksum = 0;
			len = pktptr->net_iplen-IP_HDR_LEN;
			cksum = icmp_cksum((char *)&pktptr->net_ictype,
								len);
			pktptr->net_iccksum = 0xffff & htons(cksum);
			break;

	    default:
			break;
	}

	/* Convert IP fields to network byte order */

	ip_hton(pktptr);

	/* Compute IP header checksum */

	pktptr->net_ipcksum = 0;
	cksum = ipcksum(pktptr);
	pktptr->net_ipcksum = 0xffff & htons(cksum);

	/* Convert Ethernet fields to network byte order */

	eth_hton(pktptr);

	/* Send packet over the Ethernet */

	retval = write(ifptr->if_dev, (char*)pktptr, pktlen);
	freebuf((char *)pktptr);

	if (retval == SYSERR) {
		return SYSERR;
	} else {
		return OK;
	}
}


/*------------------------------------------------------------------------
 * ip_route - choose an interface for a given IP address
 *------------------------------------------------------------------------
 */
int32	ip_route(
	  uint32 dest			/* destination IP address	*/
	)
{
	struct	ifentry	*ifptr;		/* ptr to interface entry	*/
	int32	iface;			/* index into interface table	*/

	/* Verify that at least one interface is up */

	if (ifprime < 0) {
		return SYSERR;
	}

	/* For 255.255.255.255 return the prime interface */

	if (dest == IP_BCAST) {
		return ifprime;
	}

	/* For 127.0.0.0/8 return the prime interface */

	if ( (dest & 0xff000000) == 0x7f000000) {
		return ifprime;
	}

	/* Search interface table to see if dest matched a unicast,	*/
	/*	network brodcast, or network prefix			*/

	for (iface=0; iface<NIFACES; iface++) {
		ifptr = &if_tab[iface];

		/* verify that interface is up */

		if (ifptr->if_state != IF_UP) {
			continue;
		}

		/* See if dest matches network prefix */

		if ( (dest & ifptr->if_ipmask) == ifptr->if_ipprefix) {
			return iface;
		}
	}

	/* If no default router, report error */

	if (if_tab[ifprime].if_iprouter == 0) {
		return SYSERR;
	}

	/* Report prime interface */

	return ifprime;
}


/*------------------------------------------------------------------------
 * ipcksum - compute the IP header checksum for a datagram
 *------------------------------------------------------------------------
 */

uint16	ipcksum(
	 struct  netpacket *pkt		/* ptr to a packet		*/
	)
{
	uint16	*hptr;			/* ptr to 16-bit header values	*/
	int32	i;			/* counts 16-bit values in hdr	*/
	uint16	word;			/* one 16-bit word		*/
	uint32	cksum;			/* computed value of checksum	*/

	hptr= (uint16 *) &pkt->net_ipvh;
	cksum = 0;
	for (i=0; i<10; i++) {
		word = *hptr++;
		cksum += (uint32) htons(word);
	}
	cksum += (cksum >> 16);
	cksum = 0xffff & ~cksum;
	if (cksum == 0xffff) {
		cksum = 0;
	}
	return (uint16) (0xffff & cksum);
}


/*------------------------------------------------------------------------
 * ip_ntoh - convert IP header fields to host byte order
 *------------------------------------------------------------------------
 */
void 	ip_ntoh(
	  struct netpacket *pktptr
	)
{
	pktptr->net_iplen = ntohs(pktptr->net_iplen);
	pktptr->net_ipid = ntohs(pktptr->net_ipid);
	pktptr->net_ipfrag = ntohs(pktptr->net_ipfrag);
	pktptr->net_ipsrc = ntohl(pktptr->net_ipsrc);
	pktptr->net_ipdst = ntohl(pktptr->net_ipdst);
}

/*------------------------------------------------------------------------
 * ip_hton - convert IP header fields to network byte order
 *------------------------------------------------------------------------
 */
void 	ip_hton(
	  struct netpacket *pktptr
	)
{
	pktptr->net_iplen = htons(pktptr->net_iplen);
	pktptr->net_ipid = htons(pktptr->net_ipid);
	pktptr->net_ipfrag = htons(pktptr->net_ipfrag);
	pktptr->net_ipsrc = htonl(pktptr->net_ipsrc);
	pktptr->net_ipdst = htonl(pktptr->net_ipdst);
}


/*------------------------------------------------------------------------
 *  ipout - process that transmits IP packets from the IP output queue
 *------------------------------------------------------------------------
 */

process	ipout(void)
{
	int32	iface;			/* interface to use		*/
	struct	ifentry	  *ifptr;	/* ptr to interface entry	*/
	struct	netpacket *pktptr;	/* ptr to next packet		*/
	struct	iqentry   *ipqptr;	/* ptr to IP output queue	*/
	uint32	destip;			/* destination IP address	*/
	uint32	nxthop;			/* next hop IP address		*/
	int32	retval;			/* value returned by functions	*/

	ipqptr = &ipoqueue;

	while(1) {

		/* Obtain next packet from the IP output queue */

		wait(ipqptr->iqsem);
		pktptr = ipqptr->iqbuf[ipqptr->iqhead++];
		if (ipqptr->iqhead >= IP_OQSIZ) {
			ipqptr->iqhead= 0;
		}

		/* Extract the interface number from the buffer */

		iface = pktptr->net_iface;
		if ( (iface<0) || (iface>NIFACES) ) {
			panic("ipout: bad interface number in packet\n");
		}
		ifptr = &if_tab[iface];

		/* Fill in the MAC source address */

		memcpy(pktptr->net_ethsrc, if_tab[0].if_macucast,
							ETH_ADDR_LEN);

		/* Extract destination address from packet */

		destip = pktptr->net_ipdst;


		/* Sanity check: packets sent to ioout should *not*	*/
		/*	contain	a broadcast address.			*/

		if ((destip == IP_BCAST)||(destip == ifptr->if_ipbcast)) {
			kprintf("ipout: encountered a broadcast\n");
			freebuf((char *)pktptr);
			continue;
		}

		/* Check whether destination is the local interface */

		if (destip == ifptr->if_ipucast) {
			ip_local(pktptr);
			continue;
		}

		/* Check whether destination is on the local net */

		if ( (destip & ifptr->if_ipmask) == ifptr->if_ipprefix) {

			/* Next hop is the destination itself */

			nxthop = destip;
		} else {

			/* Next hop is default router on the network */

			nxthop = ifptr->if_iprouter;
		}

		if (nxthop == 0) {  /* dest invalid or no default route */
			freebuf((char *)pktptr);
			continue;
		}

		retval = arp_resolve(iface, nxthop, pktptr->net_ethdst);
		if (retval != OK) {
			freebuf((char *)pktptr);
			continue;
		}

		ip_out(iface, pktptr);
	}
}


/*------------------------------------------------------------------------
 *  ip_enqueue - deposit an outgoing IP datagram on the IP output queue
 *------------------------------------------------------------------------
 */
status	ip_enqueue(
	  struct netpacket *pktptr	/* ptr to packet		*/
	)
{
	intmask	mask;			/* saved interrupt mask		*/
	int32	iface;			/* interface to use		*/
	struct	ifentry	*ifptr;		/* ptr to interface entry	*/
	struct	iqentry	*iptr;		/* ptr to network output queue	*/
	
	iface = pktptr->net_iface;

	if ( (iface<0) || (iface >= NIFACES) ) {
		kprintf("ip_enqueue encounters invalid interface\n");
		freebuf((char *)pktptr);
		return SYSERR;
	}
	ifptr = &if_tab[iface];
	if (ifptr->if_state != IF_UP) {
		kprintf("ip_enqueue: specified interface is down\n");
		freebuf((char *)pktptr);
		return SYSERR;
	}

	/* Insure only one process accesses output queue at a time */

	mask = disable();

	/* Enqueue packet on network output queue */

	iptr = &ipoqueue;
	if (semcount(iptr->iqsem) >= IP_OQSIZ) {
		kprintf("ipout: output queue overflow\n");
		freebuf((char *)pktptr);
		restore(mask);
		return SYSERR;
	}
	iptr->iqbuf[iptr->iqtail++] = pktptr;
	if (iptr->iqtail >= IP_OQSIZ) {
		iptr->iqtail = 0;
	}
	signal(iptr->iqsem);
	restore(mask);
	return OK;	
}
