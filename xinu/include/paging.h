/* paging.h */
#ifndef _PAGING_H_
#define _PAGING_H_

typedef unsigned int	 bsd_t;

/* Structure for a page directory entry */

typedef struct {
  unsigned int pd_pres	: 1;		/* page table present?		*/
  unsigned int pd_write : 1;		/* page is writable?		*/
  unsigned int pd_user	: 1;		/* is use level protection?	*/
  unsigned int pd_pwt	: 1;		/* write through cachine for pt?*/
  unsigned int pd_pcd	: 1;		/* cache disable for this pt?	*/
  unsigned int pd_acc	: 1;		/* page table was accessed?	*/
  unsigned int pd_mbz	: 1;		/* must be zero			*/
  unsigned int pd_fmb	: 1;		/* four MB pages?		*/
  unsigned int pd_global: 1;		/* global (ignored)		*/
  unsigned int pd_avail : 3;		/* for programmer's use		*/
  unsigned int pd_base	: 20;		/* location of page table?	*/
} pd_t;

/* Structure for a page table entry */

typedef struct {
  unsigned int pt_pres	          : 1;		/* page is present?		*/
  unsigned int pt_write           : 1;		/* page is writable?		*/
  unsigned int pt_user	          : 1;		/* is use level protection?	*/
  unsigned int pt_pwt	          : 1;		/* write through for this page? */
  unsigned int pt_pcd	          : 1;		/* cache disable for this page? */
  unsigned int pt_acc	          : 1;		/* page was accessed?		*/
  unsigned int pt_dirty           : 1;		/* page was written?		*/
  unsigned int pt_mbz	          : 1;		/* must be zero			*/
  unsigned int pt_global          : 1;		/* should be zero in 586	*/
  unsigned int pt_isvmalloc       : 1;		/* for programmer's use		*/
  unsigned int pt_isswapped       : 1;		/* for programmer's use		*/
  unsigned int pt_already_swapped  : 1;		/* for programmer's use		*/
  unsigned int pt_base	          : 20;		/* location of page?		*/
} pt_t;

typedef struct{
  unsigned int pg_offset : 12;		/* page offset			*/
  unsigned int pt_offset : 10;		/* page table offset		*/
  unsigned int pd_offset : 10;		/* page directory offset	*/
} virt_addr_t;

typedef struct{
  unsigned int fm_offset : 12;		/* frame offset			*/
  unsigned int fm_num : 20;		/* frame number			*/
} phy_addr_t;

typedef struct {
  unsigned int pdbr_mb1	: 1;		/* must be one */
  unsigned int pdbr_rsvd : 2;		/* reserved (=0) */
  unsigned int pdbr_pwt	: 1;		   /* write through cachine for pt?*/
  unsigned int pdbr_pcd	: 1;		   /* cache disable for this pt?	*/
  unsigned int pdbr_rsvd2	: 4;		/* reserved (=0) */
  unsigned int pdbr_avail : 3;		/* for programmer's use		*/
  unsigned int pdbr_base	: 20;		/* location of page directory?	*/
} pdbr_t;

extern uint32 n_static_pages;
extern uint32 n_free_vpages;
extern unsigned int error_code;

/* Macros */
#define PAGE_SIZE       4096    /* number of bytes per page		 		 */
#define PAGE_OFFSET_BITS 12
#define MAX_HEAP_SIZE   4096    /* max number of frames for virtual heap		 */
#define MAX_SWAP_SIZE   2049 /* size of swap space (in frames) 			 */
#define MAX_FSS_SIZE    2048    /* size of FSS space  (in frames)			 */
#define MAX_STACK_SIZE    2048    /* size of virtual stack space  (in frames)			 */
#define MAX_PT_SIZE	256	/* size of space used for page tables (in frames)	 */
#define N_PAGE_ENTRIES 1024

#define ASSERT( cond, msg, ... ) \
   if( (cond) == FALSE ) {\
      kprintf("SYSERR:: " msg, ##__VA_ARGS__); \
      halt(); \
   }

extern pt_t *ptmap[MAX_FSS_SIZE];
extern uint32 ffs2swapmap[MAX_FSS_SIZE];
extern pt_t *swap2ffsmap[MAX_SWAP_SIZE];

#endif
