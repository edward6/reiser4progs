/* Copyright (C) 2001, 2002 Hans Reiser.  All rights reserved.
 */

#ifndef __REISER4_TXNMGR_H__
#define __REISER4_TXNMGR_H__

#include "forward.h"
#include "spin_macros.h"
#include "dformat.h"
#include "tslist.h"

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>
#include <asm/semaphore.h>

/*****************************************************************************************
				        LIST TYPES
 *****************************************************************************************/

TS_LIST_DECLARE(atom);             /* The manager's list of atoms */
TS_LIST_DECLARE(txnh);             /* The atom's list of handles */ 

/* FIXME!!! Comments unclear, names even worse? */
TS_LIST_DECLARE(fwaitfor);         /* Each atom has one of these lists: one for its own handles */
TS_LIST_DECLARE(fwaiting);         /* waiting on another atom and one for reverse mapping.  Used
			            * to prevent deadlock in the ASTAGE_CAPTURE_WAIT state. */

TS_LIST_DECLARE(capture);          /* The transaction's list of captured znodes */

TS_LIST_DECLARE(blocknr_set);      /* Used for the transaction's delete set
				    * and wandered mapping. */

/****************************************************************************************
				    TYPE DECLARATIONS
 ****************************************************************************************/

/* This enumeration describes the possible types of a capture request (txn_try_capture).
 * A capture request dynamically assigns a block to the calling thread's transaction
 * handle. */
typedef enum
{
	/* A READ_ATOMIC request indicates that a block will be read and that the caller's
	 * atom should fuse in order to ensure that the block commits atomically with the
	 * caller. */
	TXN_CAPTURE_READ_ATOMIC   = (1 << 0),

	/* A READ_NONCOM request indicates that a block will be read and that the caller is
	 * willing to read a non-committed block without causing atoms to fuse. */
	TXN_CAPTURE_READ_NONCOM   = (1 << 1),

	/* A READ_MODIFY request indicates that a block will be read but that the caller
	 * wishes for the block to be captured as will be written.  This capture request
	 * mode is not currently used, but eventually it will be useful for preventing
	 * deadlock in read-modify-write cycles. */
	TXN_CAPTURE_READ_MODIFY   = (1 << 2),

	/* A WRITE capture request indicates that a block will be modified and that atoms
	 * should fuse to make the commit atomic. */
	TXN_CAPTURE_WRITE         = (1 << 3),

	/* CAPTURE_TYPES is a mask of the four above capture types, used to separate the
	 * exclusive type designation from extra bits that may be supplied -- see
	 * below. */
	TXN_CAPTURE_TYPES         = (TXN_CAPTURE_READ_ATOMIC |
				     TXN_CAPTURE_READ_NONCOM |
				     TXN_CAPTURE_READ_MODIFY |
				     TXN_CAPTURE_WRITE),

	/* A subset of CAPTURE_TYPES, CAPTURE_WTYPES is a mask of request types that
	 * indicate modification will occur. */
	TXN_CAPTURE_WTYPES        = (TXN_CAPTURE_READ_MODIFY |
				     TXN_CAPTURE_WRITE),

	/* An option to txn_try_capture, NONBLOCKING indicates that the caller would
	 * prefer not to sleep waiting for an aging atom to commit. */
	TXN_CAPTURE_NONBLOCKING   = (1 << 4),

	/* An option to txn_try_capture to prevent atom fusion, just simple capturing is allowed */
	TXN_CAPTURE_DONT_FUSE     = (1 << 5)

	/* This macro selects only the exclusive capture request types, stripping out any
	 * options that were supplied (i.e., NONBLOCKING). */
#define CAPTURE_TYPE(x) ((x) & TXN_CAPTURE_TYPES)

} txn_capture;

/* There are two kinds of transaction handle: WRITE_FUSING and READ_FUSING, the only
 * difference is in the handling of read requests.  A WRITE_FUSING transaction handle
 * defaults read capture requests to TXN_CAPTURE_READ_NONCOM whereas a READ_FUSIONG
 * transaction handle defaults to TXN_CAPTURE_READ_ATOMIC. */
typedef enum
{
	TXN_WRITE_FUSING  = (1 << 0),
	TXN_READ_FUSING   = (1 << 1) | TXN_WRITE_FUSING, /* READ implies WRITE */
} txn_mode;

/* Every atom has a stage, which is one of these exclusive values: */
typedef enum
{
	/* Initially an atom is free. */
	ASTAGE_FREE            = 0,

	/* An atom begins by intering the CAPTURE_FUSE stage, where it proceeds to capture
	 * blocks and fuse with other atoms. */
	ASTAGE_CAPTURE_FUSE    = 1,

	/* When an atom reaches a certain age it must do all it can to commit.  An atom in
	 * the CAPTURE_WAIT stage refuses new transaction handles and prevents fusion from
	 * atoms in the CAPTURE_FUSE stage. */
	ASTAGE_CAPTURE_WAIT    = 2,

	/* Waiting for I/O before commit.  Copy-on-capture. */
	ASTAGE_PRE_COMMIT      = 3,

	/* Post-commit overwrite I/O.  Steal-on-capture. */
	ASTAGE_POST_COMMIT     = 4,

	/* Atom which waits removing of last reference to de deleted from memory  */
	ASTAGE_DONE            = 5,

	/* Post-fusion, invalid atom. */
	ASTAGE_FUSED           = 6,

} txn_stage;

/* Certain flags may be set in the txn_atom->flags field. */
typedef enum
{
	/* Indicates that the atom should commit as soon as possible. */
	ATOM_FORCE_COMMIT = (1 << 0)
} txn_flags;

/****************************************************************************************
				     TYPE DEFINITIONS
 ****************************************************************************************/

/* A note on lock ordering: the handle & jnode spinlock protects reading of their ->atom
 * fields, so typically an operation on the atom through either of these objects must (1)
 * lock the object, (2) read the atom pointer, (3) lock the atom.
 *
 * During atom fusion, the process holds locks on both atoms at once.  Then, it iterates
 * through the list of handles and pages held by the smaller of the two atoms.  For each
 * handle and page referencing the smaller atom, the fusing process must: (1) lock the
 * object, and (2) update the atom pointer.
 *
 * You can see that there is a conflict of lock ordering here, so the more-complex
 * procedure should have priority, i.e., the fusing process has priority so that it is
 * guaranteed to make progress and to avoid restarts.
 *
 * This decision, however, means additional complexity for aquiring the atom lock in the
 * first place.  The general procedure followed in the code is:
 *
 * TXN_OBJECT *obj = ...;
 * TXN_ATOM   *atom;
 *
 * spin_lock (& obj->_lock);
 *
 * atom = obj->_atom;
 *
 * if (! spin_trylock_atom (atom))
 *   {
 *     spin_unlock (& obj->_lock);
 *     RESTART OPERATION, THERE WAS A RACE;
 *   }
 *
 * ELSE YOU HAVE BOTH ATOM AND OBJ LOCKED
 *
 * See the getatom_locked() method for a common case.
 */

/* A block number set consists of only the list head. */
struct blocknr_set {
	blocknr_set_list_head entries;
};

TS_LIST_DECLARE (io_handles);
TS_LIST_DECLARE (fq);
TS_LIST_DECLARE (fq_prepared);

/* An atomic transaction: this is the underlying system representation
 * of a transaction, not the one seen by clients. */
struct txn_atom
{
	/* The spinlock protecting the atom, held during fusion and various other state
	 * changes. */
	spinlock_t             alock;

	/* Refcount: Initially an atom has a single reference which is decremented when
	 * the atom finishes.  The value is always modified under the above spinlock.
	 * Additional references are added by each transaction handle that joins the atom
	 * and by each waiting request in either a waitfor or waiting list. */
	__u32                  refcount;

	/* The atom_id identifies the atom in persistent records such as the log. */
	__u32                  atom_id;

	/* Flags holding any of the txn_flags enumerated values (e.g.,
	 * ATOM_FORCE_COMMIT). */
	__u32                  flags;

	/* Number of open handles. */
	__u32                  txnh_count;

	/* The number of znodes captured by this atom.  Equal to the sum of lengths of the
	 * dirty_znodes[level] and clean_znodes lists. */
	__u32                  capture_count;

	/* Current transaction stage. */
	txn_stage              stage;

	/* Start time. */
	unsigned long          start_time;

	/* The atom's delete set. */
	blocknr_set            delete_set;

	/* The atom's wandered_block mapping. */
	blocknr_set            wandered_map;
	
	/* The transaction's list of dirty captured nodes--per level.  Index
	 * by (level). dirty_nodes[0] is for znode-above-root */
	capture_list_head      dirty_nodes[REAL_MAX_ZTREE_HEIGHT + 1];

	/* The transaction's list of clean captured nodes. */
	capture_list_head      clean_nodes;

	/* List of handles associated with this atom. */
	txnh_list_head               txnh_list;

	/* Transaction list link: list of atoms in the transaction manager. */
	atom_list_link               atom_link;

	/* List of handles waiting FOR this atom: see 'capture_fuse_wait' comment. */
	fwaitfor_list_head           fwaitfor_list;   

	/* List of this atom's handles that are waiting: see 'capture_fuse_wait' comment. */
	fwaiting_list_head           fwaiting_list;

	/* Numbers of objects which were deleted/created in this transaction
	 * thereby numbers of objects IDs where released/deallocated. */
	/* FIXME-ZAM: It looks like a temporary solution until logical logging
	 * is implemented */
	int  nr_objects_deleted;
	int  nr_objects_created;

	int num_queued;		/* number of jnodes which were removed from
				 * atom's lists and put on flush_queue */
	/* ZAM-FIXME-HANS unclear: all flush queue objects that flush this atom are on this list  */
	fq_list_head      flush_queues; 

	/* active i/o requests accounting */
	io_handles_list_head        io_handles;

	/* number of threads who waits this atom commit completion */
	int nr_waiters;
};

/* A transaction handle: the client obtains and commits this handle which is assigned by
 * the system to a txn_atom. */
struct txn_handle
{
	/* Spinlock protecting ->atom pointer */
	spinlock_t             hlock;

	/* Whether it is READ_FUSING or WRITE_FUSING. */
	txn_mode               mode;

	/* If assigned, the atom it is part of. */
	txn_atom              *atom;

	/* Transaction list link. */
	txnh_list_link         txnh_link;
};

TS_LIST_DECLARE(txn_mgrs);

/* The transaction manager: one is contained in the reiser4_super_info_data */
struct txn_mgr
{
	/* A spinlock protecting the atom list, id_count. */
	spinlock_t             tmgr_lock;

	/* List of atoms. */
	atom_list_head         atoms_list;

	/* Number of atoms. */
	int                    atom_count;

	/* A counter used to assign atom->atom_id values. */
	__u32                  id_count;

	/* a semaphore object for commit serialization */
	struct semaphore       commit_semaphore;

	/* a list of all txnmrgs served by particular daemon. */
	txn_mgrs_list_link     linkage;

	/* description of daemon for this txnmgr */
	ktxnmgrd_context      *daemon;
};

TS_LIST_DEFINE(txn_mgrs, txn_mgr, linkage);

/****************************************************************************************
				  FUNCTION DECLARATIONS
 ****************************************************************************************/

/* These are the externally (within Reiser4) visible transaction functions, therefore they
 * are prefixed with "txn_".  For comments, see txnmgr.c. */
   
extern int          txn_init_static       (void);
extern void         txn_mgr_init          (txn_mgr            *mgr);

extern int          txn_done_static       (void);
extern int          txn_mgr_done          (txn_mgr            *mgr);

extern int          txn_reserve           (int                 reserved);

extern void         txn_begin             (reiser4_context    *context);
extern int          txn_end               (reiser4_context    *context);

extern int          txn_mgr_force_commit_all (struct super_block *super);
extern int          txn_commit_some       (txn_mgr *mgr);
extern int          txn_same_atom_dirty   (jnode              *base,
					   jnode              *check,
					   int                 alloc_check,
					   int                 alloc_value);

extern int          txn_try_capture       (jnode              *node,
					   znode_lock_mode     mode,
					   txn_capture         flags);

extern int          txn_try_capture_page  (struct page        *pg,
					   znode_lock_mode     mode,
					   int                 non_blocking);

extern int          txn_attach_txnh_to_node (txn_handle *txnh, jnode *node, txn_flags flags);

extern void         txn_delete_page       (struct page        *pg);

extern txn_atom*    atom_get_locked_with_txnh_locked (txn_handle       *txnh);
extern txn_atom*    get_current_atom_locked (void);
extern txn_atom*    atom_get_locked_by_jnode (jnode *);

extern void         atom_send_event       (txn_atom *);

extern void         txn_insert_into_clean_list (txn_atom * atom, jnode * node);
extern int          capture_super_block   (struct super_block * s);

extern int txn_jnodes_of_one_atom (jnode *, jnode *);

/* See the comment on the function blocknrset.c:blocknr_set_add for the
 * calling convention of these three routines. */
extern void         blocknr_set_init       (blocknr_set             *bset);
extern void         blocknr_set_destroy    (blocknr_set             *bset);
extern void         blocknr_set_merge      (blocknr_set             *from,
					    blocknr_set             *into);
extern int          blocknr_set_add_extent (txn_atom                *atom,
					    blocknr_set             *bset,
					    blocknr_set_entry      **new_bsep,
					    const reiser4_block_nr  *start,
					    const reiser4_block_nr  *len);
extern int          blocknr_set_add_block  (txn_atom                *atom,
					    blocknr_set             *bset,
					    blocknr_set_entry      **new_bsep,
					    const reiser4_block_nr  *block);
extern int          blocknr_set_add_pair   (txn_atom                *atom,
					    blocknr_set             *bset,
					    blocknr_set_entry      **new_bsep,
					    const reiser4_block_nr  *a,
					    const reiser4_block_nr  *b);

typedef int (*blocknr_set_actor_f) (txn_atom*, const reiser4_block_nr*, const reiser4_block_nr*, void*);
					    
extern int          blocknr_set_iterator   (txn_atom                *atom,
					    blocknr_set             *bset,
					    blocknr_set_actor_f     actor,
					    void                    *data,
					    int                      delete);

/* reiser4 I/O handle, see io_handle.c for details */
struct reiser4_io_handle {
	/* 
	 * this semaphore is upped when all submitted i/o requests complete */
	struct semaphore      io_sema;
	/*
	 * number of submitted BIOs (struct bio) */
	atomic_t              nr_submitted;
	/*
	 * number of i/o errors */
	atomic_t              nr_errors;
	/*
	 * per-atom list of i/o handles */
	io_handles_list_link  linkage;
};

extern void io_handle_end_io (struct bio                *bio);

extern int  atom_add_bio     (txn_atom                  *atom,
			      struct bio                *bio,
			      struct reiser4_io_handle **io);

extern int  atom_wait_on_io (txn_atom                  *atom,
			      int                       *error_count);

extern void atom_fuse_io     (txn_atom                 *to,
			      txn_atom                 *from);

extern void atom_init_io     (txn_atom                 *atom);
extern void atom_done_io     (txn_atom                 *atom);

extern int  current_atom_add_bio (struct bio*);
extern int  current_atom_wait_on_io (void);

/*
 * these are needed to move to PAGE_CACHE_SIZE > blocksize
 */
jnode *             nth_jnode              (struct page *           page, int block);
jnode *             next_jnode             (jnode *                 node);

/* flush code takes care about how to fuse flush queues */
extern void flush_init_atom   (txn_atom *atom);
extern void flush_fuse_queues (txn_atom *large, txn_atom *small);

/*****************************************************************************************
				     INLINE FUNCTIONS
 *****************************************************************************************/

#define spin_ordering_pred_atom(atom)				\
	( ( lock_counters() -> spin_locked_txnh == 0 ) &&	\
	  ( lock_counters() -> spin_locked_jnode == 0 ) &&	\
	  ( lock_counters() -> spin_locked_dk == 0 ) &&		\
	  ( lock_counters() -> spin_locked_tree == 0 ) )

#define spin_ordering_pred_txnh(txnh)				\
	( ( lock_counters() -> spin_locked_dk == 0 ) &&		\
	  ( lock_counters() -> spin_locked_tree == 0 ) )

#define spin_ordering_pred_txnmgr(tmgr) 			\
	( ( lock_counters() -> spin_locked_atom == 0 ) &&	\
	  ( lock_counters() -> spin_locked_txnh == 0 ) &&	\
	  ( lock_counters() -> spin_locked_jnode == 0 ) &&	\
	  ( lock_counters() -> spin_locked_dk == 0 ) &&		\
	  ( lock_counters() -> spin_locked_tree == 0 ) )

SPIN_LOCK_FUNCTIONS(atom,txn_atom,alock);
SPIN_LOCK_FUNCTIONS(txnh,txn_handle,hlock);
SPIN_LOCK_FUNCTIONS(txnmgr,txn_mgr,tmgr_lock);

extern spinlock_t _jnode_ptr_lock;

typedef enum { 
	FQ_IN_USE    = 0x1,
	FQ_CONNECTED = 0x2
} flush_queue_state_t;

typedef struct flush_queue flush_queue_t;

/* This is an accumulator for jnodes prepared for writing to disk. A flush queue is filled by the jnode_flush()
 * routine, and written to disk under memory pressure or at atom commit time. */
struct flush_queue {
	/* 
	 * linkage element is the first in this structure to make debugging
	 * easier.  See field in atom struct for description of list. */
	fq_list_link        alink;

	fq_prepared_list_link mlink;
	/*
	 * A spinlock to protect state changes.  Acquire before modifying all fields in this struct except atomic
	 * fields. */
	spinlock_t          guard;
	/*
	 * flush_handle state: empty, active, */
	flush_queue_state_t state;
	/*
	 * list of not yet submitted to disk nodes */
	capture_list_head   prepped;
	/* 
	 * list of already submitted to disk nodes (more precisely, sent or just about to be sent, see
	 * fq_prepare_node_for_write() details */
	capture_list_head   sent;
	/*
	 * total number of queued nodes */
	int                 nr_queued;
	/*
	 * number of submitted i/o requests */
	atomic_t            nr_submitted;
	/*
	 * number of i/o errors */
	atomic_t            nr_errors;
	/*
	 * An atom this flush handle is attached to */
	txn_atom          * atom;
	/*
	 * A semaphore for waiting on i/o completion */
	struct semaphore    sema;

	/*
	 * A link field for single-linked list of fq which are collected for
	 * writing to disk */
	flush_queue_t     * next_to_write;
};

extern int  fq_get         (txn_atom *, flush_queue_t **);
extern void fq_put         (flush_queue_t *);
extern void fq_fuse        (txn_atom * to, txn_atom *from);
extern void fq_queue_node  (flush_queue_t *, jnode *);
extern int  fq_write       (flush_queue_t *, int);
extern int  finish_all_fq  (txn_atom *);
extern void fq_init_atom   (txn_atom *);

extern int  fq_mem_pressure (struct super_block *, jnode *, int);

/* Debugging */
#if REISER4_DEBUG_OUTPUT
void          print_atom   (const char *prefix, txn_atom *atom);
void          info_atom    (const char *prefix, txn_atom *atom);
#else
#define       print_atom(p,a) noop
#define       ifno_atom(p,a) noop
#endif

# endif /* __REISER4_TXNMGR_H__ */

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
