/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */
/*
 * Memory pressure hooks. Fake inodes handling.
 */
/*
 *
 * We store all file system meta data (and data, of course) in the page cache.
 *
 * What does this mean? In stead of using bread/brelse we create special
 * "fake" inode (one per super block) and store content of formatted nodes
 * into pages bound to this inode in the page cache. In newer kernels bread()
 * already uses inode attached to block device (bd_inode). Advantage of having
 * our own fake inode is that we can install appropriate methods in its
 * address_space operations. Such methods are called by VM on memory pressure
 * (or during background page flushing) and we can use them to react
 * appropriately.
 *
 * In initial version we only support one block per page. Support for multiple
 * blocks per page is complicated by relocation.
 *
 * To each page, used by reiser4, jnode is attached. jnode is analogous to
 * buffer head. Difference is that jnode is bound to the page permanently:
 * jnode cannot be removed from memory until its backing page is.
 *
 * jnode contain pointer to page (->pg field) and page contain pointer to
 * jnode in ->private field. Pointer from jnode to page is protected to by
 * jnode's spinlock and pointer from page to jnode is protected by page lock
 * (PG_locked bit). Lock ordering is: first take page lock, then jnode spin
 * lock. To go into reverse direction use jnode_lock_page() function that uses
 * standard try-lock-and-release device.
 *
 * Properties:
 *
 * 1. when jnode-to-page mapping is established (by jnode_attach_page()), page
 * reference counter is increased.
 *
 * 2. when jnode-to-page mapping is destroyed (by jnode_detach_page() and
 * page_detach_jnode()), page reference counter is decreased.
 *
 * 3. on jload() reference counter on jnode page is increased, page is
 * kmapped and `referenced'.
 *
 * 4. on jrelse() inverse operations are performed.
 *
 * 5. kmapping/kunmapping of unformatted pages is done by read/write methods.
 *
 *
 * DEADLOCKS RELATED TO MEMORY PRESSURE.
 *
 * [In the following discussion, `lock' invariably means long term lock on
 * znode.] (What about page locks?)
 *
 * There is some special class of deadlock possibilities related to memory
 * pressure. Locks acquired by other reiser4 threads are accounted for in
 * deadlock prevention mechanism (lock.c), but when ->vm_writeback() is
 * invoked additional hidden arc is added to the locking graph: thread that
 * tries to allocate memory waits for ->vm_writeback() to finish. If this
 * thread keeps lock and ->vm_writeback() tries to acquire this lock, deadlock
 * prevention is useless.
 *
 * Another related problem is possibility for ->vm_writeback() to run out of
 * memory itself. This is not a problem for ext2 and friends, because their
 * ->vm_writeback() don't allocate much memory, but reiser4 flush is
 * definitely able to allocate huge amounts of memory.
 *
 * It seems that there is no reliable way to cope with the problems above. In
 * stead it was decided that ->vm_writeback() (as invoked in the kswapd
 * context) wouldn't perform any flushing itself, but rather should just wake
 * up some auxiliary thread dedicated for this purpose (or, the same thread
 * that does periodic commit of old atoms (ktxnmgrd.c)).
 *
 * Details:
 *
 * 1. Page is called `reclaimable' against particular reiser4 mount F if this
 * page can be ultimately released by try_to_free_pages() under presumptions
 * that:
 *
 *  a. ->vm_writeback() for F is no-op, and
 *
 *  b. none of the threads accessing F are making any progress, and
 *
 *  c. other reiser4 mounts obey the same memory reservation protocol as F
 *  (described below).
 *
 * For example, clean un-pinned page, or page occupied by ext2 data are
 * reclaimable against any reiser4 mount. 
 *
 * When there is more than one reiser4 mount in a system, condition (c) makes
 * reclaim-ability not easily verifiable beyond trivial cases mentioned above.
 *
 *
 *
 *
 *
 *
 *
 *
 * THIS COMMENT IS VALID FOR "MANY BLOCKS ON PAGE" CASE
 *
 * Fake inode is used to bound formatted nodes and each node is indexed within
 * fake inode by its block number. If block size of smaller than page size, it
 * may so happen that block mapped to the page with formatted node is occupied
 * by unformatted node or is unallocated. This lead to some complications,
 * because flushing whole page can lead to an incorrect overwrite of
 * unformatted node that is moreover, can be cached in some other place as
 * part of the file body. To avoid this, buffers for unformatted nodes are
 * never marked dirty. Also pages in the fake are never marked dirty. This
 * rules out usage of ->writepage() as memory pressure hook. In stead
 * ->releasepage() is used.
 *
 * Josh is concerned that page->buffer is going to die. This should not pose
 * significant problem though, because we need to add some data structures to
 * the page anyway (jnode) and all necessary book keeping can be put there.
 *
 */

/*
 * Life cycle of pages/nodes.
 *
 * jnode contains reference to page and page contains reference back to
 * jnode. This reference is counted in page ->count. Thus, page bound to jnode
 * cannot be released back into free pool.
 *
 *  1. Formatted nodes.
 *
 *    1. formatted node is represented by znode. When new znode is created its
 *    ->pg pointer is NULL initially.
 *
 *    2. when node content is loaded into znode (by call to zload()) for the
 *    first time following happens (in call to ->read_node() or
 *    ->allocate_node()):
 *
 *      1. new page is added to the page cache.
 *
 *      2. this page is attached to znode and its ->count is increased.
 *
 *      3. page is kmapped.
 *
 *    3. if more calls to zload() follow (without corresponding zrelses), page
 *    counter is left intact and in its stead ->d_count is increased in znode.
 *
 *    4. each call to zrelse decreases ->d_count. When ->d_count drops to zero
 *    ->release_node() is called and page is kunmapped as result.
 *
 *    5. at some moment node can be captured by a transaction. Its ->x_count
 *    is then increased by transaction manager.
 *
 *    6. if node is removed from the tree (empty node with JNODE_HEARD_BANSHEE
 *    bit set) following will happen (also see comment at the top of znode.c):
 *
 *      1. when last lock is released, node will be uncaptured from
 *      transaction. This released reference that transaction manager acquired
 *      at the step 5.
 *
 *      2. when last reference is released, zput() detects that node is
 *      actually deleted and calls ->delete_node()
 *      operation. page_cache_delete_node() implementation detaches jnode from
 *      page and releases page.
 *
 *    7. otherwise (node wasn't removed from the tree), last reference to
 *    znode will be released after transaction manager committed transaction
 *    node was in. This implies squallocing of this node (see
 *    flush.c). Nothing special happens at this point. Znode is still in the
 *    hash table and page is still attached to it.
 *
 *    8. znode is actually removed from the memory because of the memory
 *    pressure, or during umount (znodes_tree_done()). Anyway, znode is
 *    removed by the call to zdrop(). At this moment, page is detached from
 *    znode and removed from the inode address space.
 *
 */

#include "reiser4.h"

static struct bio *page_bio( struct page *page, jnode *node, int rw, int gfp );

static struct address_space_operations formatted_fake_as_ops;

#define jprivate( page ) ( ( jnode * ) ( page ) -> private )

static const oid_t fake_ino = 0x1;

/**
 * one-time initialisation of fake inodes handling functions.
 */
int init_fakes()
{
	return 0;
}

/**
 * initialise fake inode to which formatted nodes are bound in the page cache.
 */
int init_formatted_fake( struct super_block *super )
{
	struct inode *fake;

	assert( "nikita-1703", super != NULL );

	fake = iget_locked( super, oid_to_ino( fake_ino ) );

	if( fake ) {
		assert( "nikita-2168", fake -> i_state & I_NEW );
		fake -> i_mapping -> a_ops = &formatted_fake_as_ops;
		fake -> i_blkbits = super -> s_blocksize_bits;
		fake -> i_size    = ~0ull;
		fake -> i_rdev    = to_kdev_t( super -> s_bdev -> bd_dev );
		fake -> i_bdev    = super -> s_bdev;
		get_super_private( super ) -> fake = fake;
		/*
		 * FIXME-NIKITA something else?
		 */
		unlock_new_inode( fake );
		return 0;
	} else
		return -ENOMEM;
}

/** release fake inode for @super */
int done_formatted_fake( struct super_block *super )
{
	struct inode *fake;

	fake = get_super_private_nocheck( super ) -> fake;
	iput( fake );
	return 0;
}

/** 
 * check amount of available for allocation memory, and kick ktxnmgrd is it
 * is low. NEITHER FINISHED NOR USED.
 */
void reiser4_check_mem( reiser4_context *ctx )
{
	reiser4_super_info_data *info;

	unsigned int total;
	unsigned int free;
	unsigned int ratio;

	if( ctx == NULL || ctx -> super == NULL )
		return;

	info = get_super_private( ctx -> super );
	if( info == NULL )
		return;

	total = nr_free_pagecache_pages();
	free  = nr_free_pages();

	/*
	 * we don't care about overflows here, because this is only hint
	 * anyway.
	 */
	ratio = free * 100 / total;
	if( ratio <= info -> txnmgr.low_memory ) {
		ktxnmgrd_context *daemon;

		daemon = info -> tmgr.daemon;
		if( daemon != NULL ) {
			int kick_it;

			/* 
			 * we are first to note low free memory. Wake up
			 * ktxnmgrd 
			 */
			kick_it = !atomic_read( &daemon -> pressure );
			atomic_inc( &daemon -> pressure );
			if( kick_it )
				ktxnmgrd_kick( daemon, LOW_MEMORY );
		}
	}
}

#if 0
/** 
 * helper function to find-and-lock page in a page cache and do additional
 * checks 
 */
struct page *reiser4_lock_page( struct address_space *mapping, 
				unsigned long index )
{
	struct page *page;

	assert( "nikita-2408", mapping != NULL );
	ON_DEBUG_CONTEXT( assert( "nikita-2409", 
				  lock_counters() -> spin_locked == 0 ) );
	ON_DEBUG_CONTEXT( assert( "nikita-2655", 
				  lock_counters() -> long_term_locked_znode == 0 ) );
	page = find_lock_page( mapping, index );
	if( page ) {
		ON_DEBUG_CONTEXT( ++ lock_counters() -> page_locked );
	}
	return page;
}
#endif

/** return tree @page is in */
reiser4_tree *tree_by_page( const struct page *page /* page to query */ )
{
	assert( "nikita-2461", page != NULL );
	return &get_super_private( page -> mapping -> host -> i_sb ) -> tree;
}

#if REISER4_DEBUG_MEMCPY

/*
 * Our own versions of memcpy, memmove, and memset used to profile shifts of
 * tree node content. Coded to avoid inlining.
 */

struct mem_ops_table {
	void * ( *cpy ) ( void *dest, const void *src, size_t n );
	void * ( *move )( void *dest, const void *src, size_t n );
	void * ( *set ) ( void *s, int c, size_t n );
};

void *xxmemcpy( void *dest, const void *src, size_t n )
{
	return memcpy( dest, src, n );
}

void *xxmemmove( void *dest, const void *src, size_t n )
{
	return memmove( dest, src, n );
}

void *xxmemset( void *s, int c, size_t n )
{
	return memset( s, c, n );
}

struct mem_ops_table std_mem_ops = {
	.cpy  = xxmemcpy,
	.move = xxmemmove,
	.set  = xxmemset
};

struct mem_ops_table *mem_ops = &std_mem_ops;

void *xmemcpy( void *dest, const void *src, size_t n )
{
	return mem_ops -> cpy( dest, src, n );
}

void *xmemmove( void *dest, const void *src, size_t n )
{
	return mem_ops -> move( dest, src, n );
}

void *xmemset( void *s, int c, size_t n )
{
	return mem_ops -> set( s, c, n );
}

#endif


/** 
 * completion handler for single page bio-based read. 
 *
 * mpage_end_io_read() would also do. But it's static.
 *
 */
static int end_bio_single_page_read( struct bio *bio, 
				     unsigned int bytes_done, int err )
{
	struct page *page;

	if( bio -> bi_size != 0 )
		return 1;

	page = bio -> bi_io_vec[ 0 ].bv_page;

	if( test_bit( BIO_UPTODATE, &bio -> bi_flags ) )
		SetPageUptodate( page );
	else {
		ClearPageUptodate( page );
		SetPageError( page );
	}
	unlock_page( page );
	bio_put( bio );
	return 0;
}

/** 
 * completion handler for single page bio-based write. 
 *
 * mpage_end_io_write() would also do. But it's static.
 *
 */
static int end_bio_single_page_write( struct bio *bio, 
				      unsigned int bytes_done, int err )
{
	struct page *page;

	if( bio -> bi_size != 0 )
		return 1;

	page = bio -> bi_io_vec[ 0 ].bv_page;

	if( !test_bit( BIO_UPTODATE, &bio -> bi_flags ) )
		SetPageError( page );
	end_page_writeback( page );
	bio_put( bio );
	return 0;
}

/** ->readpage() method for formatted nodes */
static int formatted_readpage( struct file *f UNUSED_ARG,
			       struct page *page /* page to read */ )
{
	assert( "nikita-2412", PagePrivate( page ) && jprivate( page ) );
	return page_io( page, jprivate( page ), READ, GFP_KERNEL );
}

/** ->writepage() method for formatted nodes */
static int formatted_writepage( struct page *page /* page to write */ )
{
	assert( "nikita-2632", PagePrivate( page ) && jprivate( page ) );
	return page_io( page, jprivate( page ), WRITE, GFP_NOFS | __GFP_HIGH );
}

/** submit single-page bio request */
int page_io( struct page *page /* page to perform io for */, 
	     jnode *node /* jnode of page */,
	     int rw /* read or write */, int gfp /* GFP mask */ )
{
	struct bio *bio;
	int         result;

	assert( "nikita-2094", page != NULL );
	assert( "nikita-2226", PageLocked( page ) );
	assert( "nikita-2634", node != NULL );

	jnode_ops( node ) -> io_hook( node, page, rw );

	bio = page_bio( page, node, rw, gfp );
	if( !IS_ERR( bio ) ) {
		if( rw == WRITE ) {
			SetPageWriteback( page );
			unlock_page(page);
		}
		submit_bio( rw, bio );
		result = 0;
	} else
		result = PTR_ERR( bio );
	return result;
}


/** helper function to construct bio for page */
static struct bio *page_bio( struct page *page, jnode *node, int rw, int gfp )
{
	struct bio *bio;
	assert( "nikita-2092", page != NULL );
	assert( "nikita-2633", node != NULL );

	/*
	 * Simple implemenation in the assumption that blocksize == pagesize.
	 *
	 * We only have to submit one block, but submit_bh() will allocate bio
	 * anyway, so lets use all the bells-and-whistles of bio code.
	 *
	 * This is roughly equivalent to mpage_readpage() for one
	 * page. mpage_readpage() is not used, because it depends on
	 * get_block() to obtain block number and get_block() gets everything,
	 * but page---and we need page to obtain block number from jnode. One
	 * line change to mpage_readpage() (bh.b_page = page;) and it can be
	 * used. Other problem is the do_mpage_readpage() checks
	 * page_has_buffers().
	 *
	 */

	bio = bio_alloc( gfp, 1 );
	if( bio != NULL ) {
		int                 blksz;
		struct super_block *super;
		reiser4_block_nr    blocknr;

		super = page -> mapping -> host -> i_sb;
		assert( "nikita-2029", super != NULL );
		blksz = super -> s_blocksize;
		assert( "nikita-2028", blksz == ( int ) PAGE_CACHE_SIZE );

		blocknr = *jnode_get_block( node );
		assert( "nikita-2275", blocknr != ( reiser4_block_nr ) 0 );
		assert( "nikita-2276", !blocknr_is_fake( &blocknr ) );

		bio -> bi_sector = blocknr * ( blksz >> 9 );
		bio -> bi_bdev   = super -> s_bdev;
		bio -> bi_io_vec[ 0 ].bv_page   = page;
		bio -> bi_io_vec[ 0 ].bv_len    = blksz;
		bio -> bi_io_vec[ 0 ].bv_offset = 0;

		bio -> bi_vcnt = 1;
		/* bio -> bi_idx is filled by bio_init() */
		bio -> bi_size = blksz;

		bio -> bi_end_io = ( rw == READ ) ? 
			end_bio_single_page_read : end_bio_single_page_write;

		return bio;
	} else
		return ERR_PTR( -ENOMEM );
}

/**
 * ->vm_writeback() callback for formatted page. Called from shrink_cache()
 * (or however it will be called by the time you read this).
 */
static int formatted_vm_writeback( struct page *page /* page to start
						      * writeback from */, 
				   int *nr_to_write /* number of pages VM asks
						     * us to submit. We should
						     * try to stay reasonable
						     * close. */ )
{
	return page_common_writeback( page, nr_to_write, 
				      JNODE_FLUSH_MEMORY_FORMATTED);
}

/**
 * Common memory pressure notification.
 *
 * This is called from our ->vm_writeback() methods: formatted_vm_writeback()
 * and reiser4_vm_writeback().
 *
 * Initial design was that this function would perform all flush and IO
 * submitting directly (that is, in the context of kswapd, or caller of
 * balance_classzone()). It was found, though, that problems with deadlocks
 * and flush running of memory are very hard if at all possible to overcome.
 *
 * It was then decided, that this function should in stead just wake up some
 * worker thread (disguised under fancy name of `ent') to perform actual flush
 * and submit IO requests. Currently very simple scheme is implemented. There
 * is not much sense in elaborating it now when VM is in such a flux.
 *
 */
int page_common_writeback( struct page *page /* page to start writeback from */,
			   int *nr_to_write /* number of pages to write */, 
			   int flush_flags /* Additional hint. Seems to be
					    * unused currently. */ )
{
	int result;
	jnode *node;
	reiser4_context *ctx;
	txn_handle *txnh;
	REISER4_ENTRY( page -> mapping -> host -> i_sb );

	assert( "vs-828", PageLocked( page ) );

	node = jfind( page );
	assert( "nikita-2419", node != NULL );

	unlock_page( page );

	ctx  = get_current_context ();
	txnh = ctx->trans;

	if (! spin_trylock_txnh (txnh)) {
		jput (node);
		REISER4_EXIT (0);
	}

	if (txnh->atom != NULL || ! lock_stack_isclean( & ctx->stack )) {
		/*
		 * Good Lord, we are called synchronously! What a shame.
		 *
		 * we got here by
		 * __alloc_pages->balance_classzone->...->shrink_cache
		 *
		 * no chance of working in such situation.
		 */
		spin_unlock_txnh (txnh);
		jput (node);
		REISER4_EXIT (0);
	}

	spin_unlock_txnh (txnh);

	/* Attach the txn handle to this node, preventing the atom from
	 * committing while this flush occurs.
	 *
	 * Note: This ATOM_FORCE_COMMIT causes the atom to commit right
	 * away... except the no_commit_thread() check in txnmgr.c may disable
	 * it if called from memory pressure.
	 */
	result = txn_attach_txnh_to_node (txnh, node, ATOM_FORCE_COMMIT);

	if( 1 ) {
		/*
		 * this is for case when all flushing is done outside of
		 * kswapd context (in "ent" thread).
		 */
		ktxnmgrd_kick (get_super_private (ctx->super)->tmgr.daemon, 
			       MEMORY_PRESSURE);
		jput (node);
		REISER4_EXIT (0);
	}

	if (result == -ENOENT) {

		/* Txn committed during attach, jnode has no atom. */
		result = 0;

	} else if (result == 0) {
		/* And flush it... */
		result = jnode_flush (node, nr_to_write, flush_flags);
	}

	jput (node);
	REISER4_EXIT (result);
}

/** ->set_page_dirty() method of formatted address_space */
static int formatted_set_page_dirty( struct page *page /* page to mark
							* dirty */ )
{
	assert( "nikita-2173", page != NULL );
	return __set_page_dirty_nobuffers( page );
}

/** place holders for methods that don't make sense for the fake inode */

define_never_ever_op( readpages )
define_never_ever_op( prepare_write )
define_never_ever_op( commit_write )
define_never_ever_op( bmap )
define_never_ever_op( direct_IO )

#define V( func ) ( ( void * ) ( func ) )

/**
 * address space operations for the fake inode
 */
static struct address_space_operations formatted_fake_as_ops = {
	.writepage      = formatted_writepage,
	/* this is called to read formatted node */
	.readpage       = formatted_readpage,
	/**
	 * ->sync_page() method of fake inode address space operations. Called
	 * from wait_on_page() and lock_page().
	 *
	 * This is most annoyingly misnomered method. Actually it is called
	 * from wait_on_page_bit() and lock_page() and its purpose is to
	 * actually start io by jabbing device drivers.
	 */
	.sync_page      = block_sync_page,
	/* Write back some dirty pages from this mapping. Called from sync.
	   called during sync (pdflush) */
	.writepages     = reiser4_writepages,
	/* Perform a writeback as a memory-freeing operation. */
	.vm_writeback   = formatted_vm_writeback,
	/* Set a page dirty */
	.set_page_dirty = formatted_set_page_dirty,
	/* used for read-ahead. Not applicable */
	.readpages      = V( never_ever_readpages ),
	.prepare_write  = V( never_ever_prepare_write ),
	.commit_write   = V( never_ever_commit_write ),
	.bmap           = V( never_ever_bmap ),
	/* called just before page is being detached from inode mapping and
	 * removed from memory. Called on truncate, cut/squeeze, and
	 * umount. */
	.invalidatepage = reiser4_invalidatepage,
	/**
	 * this is called by shrink_cache() so that file system can try to
	 * release objects (jnodes, buffers, journal heads) attached to page
	 * and, may be made page itself free-able.
	 */
	.releasepage    = reiser4_releasepage,
	.direct_IO      = V( never_ever_direct_IO )
};

/* clear page's dirty bit, set writeback bit and remove the page from inode's
 * dirty pages list. it is assumed that page is already locked */
void set_page_clean_nolock (struct page * pg)
{
	assert ("zam-669", PageLocked(pg));

	ClearPageDirty (pg);

	write_lock(&pg->mapping->page_lock);

	list_del(&pg->list);
	list_add(&pg->list, &pg->mapping->clean_pages);

	write_unlock(&pg->mapping->page_lock);
} 


#if REISER4_DEBUG

#define page_flag_name( page, flag )			\
	( test_bit( ( flag ), &( page ) -> flags ) ? ((#flag "|")+3) : "" )

void print_page( const char *prefix, struct page *page )
{
	if( page == NULL ) {
		info( "null page\n" );
		return;
	}
	info( "%s: page index: %lu mapping: %p count: %i private: %lx\n",
	      prefix, page -> index, page -> mapping, 
	      atomic_read( &page -> count ), page -> private );
	info( "\tflags: %s%s%s%s %s%s%s%s %s%s%s%s %s%s%s\n",
	      page_flag_name( page,  PG_locked ),
	      page_flag_name( page,  PG_error ),
	      page_flag_name( page,  PG_referenced ),
	      page_flag_name( page,  PG_uptodate ),

	      page_flag_name( page,  PG_dirty ),
	      page_flag_name( page,  PG_lru ),
	      page_flag_name( page,  PG_active ),
	      page_flag_name( page,  PG_slab ),

	      page_flag_name( page,  PG_highmem ),
	      page_flag_name( page,  PG_checked ),
	      page_flag_name( page,  PG_arch_1 ),
	      page_flag_name( page,  PG_reserved ),

	      page_flag_name( page,  PG_private ),
	      page_flag_name( page,  PG_writeback ),
	      page_flag_name( page,  PG_nosave ) );
	if( jprivate( page ) != NULL ) {
		info_znode( "\tpage jnode", ( znode * ) jprivate( page ) );
		info( "\n" );
	}
}


#endif

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * scroll-step: 1
 * End:
 */
