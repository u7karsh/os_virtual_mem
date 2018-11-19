/* in file addargs.c */
extern	status	addargs(pid32, int32, int32[], int32,char *, void *);

/* in file arp.c */

extern	void	arp_init(int32);
extern	status	arp_resolve(int32, uint32, byte[]);
extern	void	arp_in(int32, struct arppacket *);
extern	int32	arp_alloc(int32);
extern	void	arp_ntoh(struct arppacket *);
extern	void	arp_hton(struct arppacket *);

/* in file ascdate.c */

extern	status	ascdate(uint32, char *);

/* in file bufinit.c */

extern	status	bufinit(void);

/* in file chprio.c */

extern	pri16	chprio(pid32, pri16);

/* in file clkupdate.S */

extern	uint32	clkcount(void);

/* in file clkhandler.c */

extern	interrupt clkhandler(void);

/* in file clkinit.c */

extern	void	clkinit(void);

/* in file clkint.S */

extern	void	clkint(void);

/* in file close.c */

extern	syscall	close(did32);

/* in file control.c */

extern	syscall	control(did32, int32, int32, int32);

/* in file create.c */

extern	pid32	create(void *, uint32, pri16, char *, uint32, ...);

/* in file ctxsw.S */

extern	void	ctxsw(void *, void *);

/* in file dhcp.c */

extern	uint32	getlocalip(void);

/* in file dot2ip.c */

extern	uint32	dot2ip(char *, uint32 *);

/* in file queue.c */

extern	pid32	enqueue(pid32, qid16);

/* in file intutils.S */

extern	intmask	disable(void);

/* in file intutils.S */

extern	void	enable(void);

/* in file e1000Control.c */
extern 	devcall e1000Control(struct dentry *, int32, int32, int32);
extern 	void 	e1000IrqEnable(struct ether *);
extern 	void 	e1000IrqDisable(struct ether *);

/* in file 82545EMInit.c */
extern 	status 	_82545EMInit(struct ether *);

/* in file 82567LMInit.c */
extern 	status 	_82567LMInit(struct ether *ethptr);

/* in file e1000Interrupt.c */
extern  interrupt 	e1000Interrupt(void);

/* in file e1000Read.c */
extern  devcall e1000Read(struct dentry *, void *, uint32);

/* in file e1000Write.c */
extern 	devcall e1000Write(struct dentry *, void *, uint32);

/* in file e1000Dispatch.S */
extern	interrupt	e1000Dispatch(void);

/* in file ethInit.c */
extern	devcall	ethInit(struct dentry *);

/* in file evec.c */
extern	int32	initevec(void);
extern	int32	set_evec(uint32, uint32);
extern	void	trap(int32);

/* in file exception.c */
extern  void exception(int32, int32*);

/* in file freebuf.c */
extern	syscall	freebuf(char *);

/* in file freemem.c */
extern	syscall	freemem(char *, uint32);

/* in file getbuf.c */
extern	char	*getbuf(bpid32);

/* in file getc.c */
extern	syscall	getc(did32);

/* in file getitem.c */
extern	pid32	getfirst(qid16);

/* in file getmem.c */
extern	char	*getmem(uint32);

/* in file getpid.c */
extern	pid32	getpid(void);

/* in file getprio.c */
extern	syscall	getprio(pid32);

/* in file getstk.c */
extern	char	*getstk(uint32);

/* in file gettime.c */
extern	status	gettime(uint32 *);

/* in file getutime.c */
extern	status	getutime(uint32 *);

/* in file halt.S */
extern	void	halt(void);

/* in file icmp.c */

extern	void	icmp_init(void);
extern	void	icmp_in(struct netpacket *);
extern	int32	icmp_register(uint32);
extern	int32	icmp_recv(int32, char *, int32, uint32);
extern	status	icmp_send(int32, uint32, uint16, uint16, uint16, char *, int32);
extern	struct	netpacket *icmp_mkpkt(int32, uint32, uint16, uint16, uint16, char *, int32);
extern	status	icmp_release(int32);
extern	uint16	icmp_cksum(char *, int32);
extern	void	icmp_hton(struct netpacket *);
extern	void	icmp_ntoh(struct netpacket *);

/* in file init.c */
extern	syscall	init(did32);

/* in file initialize.c */
extern	int32	sizmem(void);

/* in file insert.c */
extern	status	insert(pid32, qid16, int32);

/* in file insertd.c */
extern	status	insertd(pid32, qid16, int32);

/* in file intr.S */
extern	uint16	getirmask(void);

/* in file ioerr.c */
extern	devcall	ioerr(void);

/* in file ionull.c */
extern	devcall	ionull(void);

/* in file ip.c */

extern	void	ip_in(struct netpacket *);
extern	status	ip_send(struct netpacket *);
extern	void	ip_local(struct netpacket *);
extern	status	ip_out(int32, struct netpacket *);
extern	int32	ip_route(uint32);
extern	uint16	ipcksum(struct netpacket *);
extern	void	ip_ntoh(struct netpacket *);
extern	void	ip_hton(struct netpacket *);
extern	process	ipout(void);
extern	status	ip_enqueue(struct netpacket *);

/* in file nat.c */

extern	void	nat_init(void);
extern	void	nat_in(struct netpacket *);
extern	void	nat_out(struct netpacket *);
extern	void	nat_in_udp(struct netpacket *);
extern	void	nat_out_udp(struct netpacket *);
extern	uint16	nat_genuport(struct nuentry *);
extern	void	nat_in_icmp(struct netpacket *);
extern	void	nat_out_icmp(struct netpacket *);
extern	uint16	nat_genicmpid(struct nientry *);

/* in file net.c */

extern	void	net_init(void);
extern	process	netin(int32);
extern	process	netout(void);
extern	process	rawin(void);
extern	void	eth_hton(struct netpacket *);
extern	void	eth_ntoh(struct netpacket *);

/* in file netiface.c */

extern	void	netiface_init(void);

/* in file netstart.c */

extern	void	netstart(void);

/* in file kill.c */

extern	syscall	kill(pid32);

/* in file lexan.c */

extern	int32	lexan(char *, int32, char *, int32 *, int32 [], int32 []);

/* in file lfibclear.c */

extern	void	lfibclear(struct lfiblk *, int32);

/* in file lfibget.c */

extern	void	lfibget(did32, ibid32, struct lfiblk *);

/* in file lfibput.c */
extern	status	lfibput(did32, ibid32, struct lfiblk *);

/* in file lfdbfree.c */
extern	status	lfdbfree(did32, dbid32);

/* in file lfdballoc.c */
extern	dbid32	lfdballoc(struct lfdbfree *);

/* in file lfflush.c */
extern	status	lfflush(struct lflcblk *);

/* in file lfgetmode.c */
extern	int32	lfgetmode(char *);

/* in file lfiballoc.c */
extern	ibid32	lfiballoc(void);

/* in file lflClose.c */
extern	devcall	lflClose(struct dentry *);

/* in file lflControl.c */
extern	devcall	lflControl(struct dentry *, int32, int32, int32);

/* in file lflGetc.c */
extern	devcall	lflGetc(struct dentry *);

/* in file lflInit.c */
extern	devcall	lflInit(struct dentry *);

/* in file lflPutc.c */
extern	devcall	lflPutc(struct dentry *, char);

/* in file lflRead.c */
extern	devcall	lflRead(struct dentry *, char *, int32);

/* in file lflSeek.c */
extern	devcall	lflSeek(struct dentry *, uint32);

/* in file lflWrite.c */
extern	devcall	lflWrite(struct dentry *, char *, int32);

/* in file lfscreate.c */
extern  status  lfscreate(did32, ibid32, uint32);

/* in file lfsInit.c */
extern	devcall	lfsInit(struct dentry *);

/* in file lfsOpen.c */
extern	devcall	lfsOpen(struct dentry *, char *, char *);

/* in file lfsetup.c */
extern	status	lfsetup(struct lflcblk *);

/* in file lftruncate.c */
extern	status	lftruncate(struct lflcblk *);

/* in file lpgetc.c */
extern	devcall	lpgetc(struct dentry *);

/* in file lpinit.c */
extern	devcall	lpinit(struct dentry *);

/* in file lpopen.c */
extern	devcall	lpopen(struct dentry *, char *, char *);

/* in file lpputc.c */
extern	devcall	lpputc(struct dentry *, char);

/* in file lpread.c */
extern	devcall	lpread(struct dentry *, char *, int32);

/* in file lpwrite.c */
extern	devcall	lpwrite(struct dentry *, char *, int32);

/* in file mark.c */
extern	void	_mkinit(void);

/* in file memcpy.c */
extern	void	*memcpy(void *, const void *, int32);

/* in file memcpy.c */
extern	int32	*memcmp(void *, const void *, int32);

/* in file memset.c */
extern  void    *memset(void *, const int, int32);

/* in file mkbufpool.c */
extern	bpid32	mkbufpool(int32, int32);

/* in file mount.c */
extern	syscall	mount(char *, char *, did32);
extern	int32	namlen(char *, int32);

/* in file namInit.c */
extern	status	namInit(void);

/* in file nammap.c */
extern	devcall	nammap(char *, char[], did32);
extern	did32	namrepl(char *, char[]);
extern	status	namcpy(char *, char *, int32);

/* in file namOpen.c */
extern	devcall	namOpen(struct dentry *, char *, char *);

/* in file newqueue.c */
extern	qid16	newqueue(void);

/* in file open.c */
extern	syscall	open(did32, char *, char *);

/* in file panic.c */
extern	void	panic(char *);

/* in file pci.c */
extern	int32	pci_init(void);

/* in file pdump.c */
extern	void	pdump(struct netpacket *);
extern	void	pdumph(struct netpacket *);

/* in file ptclear.c */
extern	void	_ptclear(struct ptentry *, uint16, int32);

/* in file ptcount.c */
extern	int32	ptcount(int32);

/* in file ptcreate.c */
extern	syscall	ptcreate(int32);

/* in file ptdelete.c */
extern	syscall	ptdelete(int32, int32);

/* in file ptinit.c */
extern	syscall	ptinit(int32);

/* in file ptrecv.c */
extern	uint32	ptrecv(int32);

/* in file ptreset.c */
extern	syscall	ptreset(int32, int32);

/* in file ptsend.c */
extern	syscall	ptsend(int32, umsg32);

/* in file putc.c */
extern	syscall	putc(did32, char);

/* in file ramClose.c */
extern	devcall	ramClose(struct dentry *);

/* in file ramInit.c */
extern	devcall	ramInit(struct dentry *);

/* in file ramOpen.c */

extern	devcall	ramOpen(struct dentry *, char *, char *);

/* in file ramRead.c */
extern	devcall	ramRead(struct dentry *, char *, int32);

/* in file ramWrite.c */
extern	devcall	ramWrite(struct dentry *, char *, int32);

/* in file rdsClose.c */
extern	devcall	rdsClose(struct dentry *);

/* in file rdsControl.c */
extern	devcall	rdsControl(struct dentry *, int32, int32, int32);

/* in file rdsInit.c */
extern	devcall	rdsInit(struct dentry *);

/* in file rdsOpen.c */
extern	devcall	rdsOpen(struct dentry *, char *, char *);

/* in file rdsRead.c */
extern	devcall	rdsRead(struct dentry *, char *, int32);

/* in file rdsWrite.c */
extern	devcall	rdsWrite(struct dentry *, char *, int32);

/* in file rdsbufalloc.c */
extern	struct	rdbuff * rdsbufalloc(struct rdscblk *);

/* in file rdscomm.c */
extern	status	rdscomm(struct rd_msg_hdr *, int32, struct rd_msg_hdr *,
		int32, struct rdscblk *);

/* in file rdsprocess.c */
extern	void	rdsprocess(struct rdscblk *);

/* in file read.c */
extern	syscall	read(did32, char *, uint32);

/* in file ready.c */
extern	status	ready(pid32, bool8);

/* in file receive.c */
extern	umsg32	receive(void);

/* in file recvclr.c */
extern	umsg32	recvclr(void);

/* in file recvtime.c */
extern	umsg32	recvtime(int32);

/* in file resched.c */
extern	void	resched(void);

/* in file intutils.S */
extern	void	restore(intmask);

/* in file resume.c */
extern	pri16	resume(pid32);

/* in file rfsgetmode.c */
extern	int32	rfsgetmode(char * );

/* in file rflClose.c */
extern	devcall	rflClose(struct dentry *);

/* in file rfsControl.c */
extern	devcall	rfsControl(struct dentry *, int32, int32, int32);

/* in file rflGetc.c */
extern	devcall	rflGetc(struct dentry *);

/* in file rflInit.c */
extern	devcall	rflInit(struct dentry *);

/* in file rflPutc.c */
extern	devcall	rflPutc(struct dentry *, char );

/* in file rflRead.c */
extern	devcall	rflRead(struct dentry *, char *, int32 );

/* in file rflSeek.c */
extern	devcall	rflSeek(struct dentry *, uint32 );

/* in file rflWrite.c */
extern	devcall	rflWrite(struct dentry *, char *, int32 );

/* in file rfsndmsg.c */
extern	status	rfsndmsg(uint16, char *);

/* in file rfsInit.c */
extern	devcall	rfsInit(struct dentry *);

/* in file rfsOpen.c */
extern	devcall	rfsOpen(struct dentry  *devptr, char *, char *);

/* in file rfscomm.c */
extern	int32	rfscomm(struct rf_msg_hdr *, int32, struct rf_msg_hdr *, int32);

/* in file rdsClose.c */
extern	devcall	rdsClose(struct dentry *);

/* in file rdsControl.c */
extern	devcall	rdsControl(struct dentry *, int32, int32, int32);

/* in file rdsInit.c */
extern	devcall	rdsInit(struct dentry *);

/* in file rdsOpen.c */
extern	devcall	rdsOpen(struct dentry *, char *, char *);

/* in file rdsRead.c */
extern	devcall	rdsRead(struct dentry *, char *, int32);

/* in file rdsWrite.c */
extern	devcall	rdsWrite(struct dentry *, char *, int32);

/* in file rdsbufalloc.c */
extern	struct	rdbuff * rdsbufalloc(struct rdscblk *);

/* in file rdscomm.c */
extern	status	rdscomm(struct rd_msg_hdr *, int32, struct rd_msg_hdr *, int32, struct rdscblk *);

/* in file rdsprocess.c */
extern	void	rdsprocess(struct rdscblk *);

/* in file sched_cntl.c */
extern	status	sched_cntl(int32);

/* in file seek.c */
extern	syscall	seek(did32, uint32);

/* in file semcount.c */
extern	syscall	semcount(sid32);

/* in file semcreate.c */
extern	sid32	semcreate(int32);

/* in file semdelete.c */
extern	syscall	semdelete(sid32);

/* in file semreset.c */
extern	syscall	semreset(sid32, int32);

/* in file send.c */
extern	syscall	send(pid32, umsg32);

/* in file shell.c */
extern 	process shell(did32);

/* in file signal.c */
extern	syscall	signal(sid32);

/* in file signaln.c */
extern	syscall	signaln(sid32, int32);

/* in file sleep.c */
extern	syscall	sleepms(uint32);
extern	syscall	sleep(uint32);

/* in file start.S */
extern	int32	inb(int32);
extern	int32	inw(int32);
extern	int32	inl(int32);
extern	int32	outb(int32, int32);
extern	int32	outw(int32, int32);
extern	int32	outl(int32, int32);
extern	int32	outsw(int32, int32, int32);
extern	int32	insw(int32, int32 ,int32);

/* in file suspend.c */
extern	syscall	suspend(pid32);

/* in file ttyControl.c */
extern	devcall	ttyControl(struct dentry *, int32, int32, int32);

/* in file ttyDispatch.c */
extern	interrupt	ttyDispatch(void);

/* in file ttyGetc.c */
extern	devcall	ttyGetc(struct dentry *);

/* in file ttyInter_in.c */
extern	void	ttyInter_in(struct ttycblk *, struct uart_csreg *);

/* in file ttyInter_out.c */
extern	void	ttyInter_out(struct ttycblk *, struct uart_csreg *);

/* in file ttyInterrupt.c */
extern	void	ttyInterrupt(void);

/* in file ttyInit.c */
extern	devcall	ttyInit(struct dentry *);

/* in file ttyKickOut.c */
extern	void	ttyKickOut(struct ttycblk *, struct uart_csreg *);

/* in file ttyPutc.c */
extern	devcall	ttyPutc(struct dentry *, char);

/* in file ttyRead.c */
extern	devcall	ttyRead(struct dentry *, char *, int32);

/* in file ttyWrite.c */
extern	devcall	ttyWrite(struct dentry *, char *, int32);

/* in file udp.c */

extern	void	udp_init(void);
extern	void	udp_in(struct netpacket *);
extern	uid32	udp_register(int32, uint32, uint16, uint16);
extern	int32	udp_recv(uid32, char *, int32, uint32);
extern	int32	udp_recvaddr(uid32, uint32 *, uint16 *, char *, int32, uint32);
extern	status	udp_send(uid32, char *, int32);
extern	status	udp_sendto(uid32, uint32, uint16, char *, int32);
extern	status	udp_release(uid32);
extern	void	udp_ntoh(struct netpacket *);
extern	void	udp_hton(struct netpacket *);


/* in file unsleep.c */
extern	syscall	unsleep(pid32);

/* in file userret.c */
extern	void	userret(void);

/* in file wait.c */
extern	syscall	wait(sid32);

/* in file wakeup.c */
extern	void	wakeup(void);

/* in file write.c */
extern	syscall	write(did32, char *, uint32);

/* in file xdone.c */
extern	void	xdone(void);

/* in file yield.c */
extern	syscall	yield(void);

/* NETWORK BYTE ORDER CONVERSION NOT NEEDED ON A BIG-ENDIAN COMPUTER */
#define	htons(x)   ( ( 0xff & ((x)>>8) ) | ( (0xff & (x)) << 8 ) )
#define	htonl(x)   (  (((x)>>24) & 0x000000ff) | (((x)>> 8) & 0x0000ff00) | \
		      (((x)<< 8) & 0x00ff0000) | (((x)<<24) & 0xff000000) )
#define	ntohs(x)   ( ( 0xff & ((x)>>8) ) | ( (0xff & (x)) << 8 ) )
#define	ntohl(x)   (  (((x)>>24) & 0x000000ff) | (((x)>> 8) & 0x0000ff00) | \
		      (((x)<< 8) & 0x00ff0000) | (((x)<<24) & 0xff000000) )

