/* Copyright 2002 by Hans Reiser */

/*
 * Reiser4 Wandering Log
 */

/*
 * You should read www.namesys.com/txn-mgr.html before trying to read this
 * file.

 * That describes how filesystem operations are performed as atomic
 * transactions, and how we try to arrange it so that we can write most of the
 * data only once while performing the operation atomically.

 * For the purposes of this code, it is enough for it to understand that it
 * has been told a given block should be written either once, or twice (if
 * twice then once to the wandered location and once to the real location).

 * This code guarantees that those blocks that are defined to be part of an
 * atom either all take effect or none of them take effect.

 * Relocate set nodes are submitted to write by the jnode_flush() routine, and
 * the overwrite set is submitted by reiser4_write_log().  This is because
 * with the overwrite set we seek to optimize writes, and with the relocate
 * set we seek to cause disk order to correlate with the parent first preorder.


 * reiser4_write_log() allocates and writes wandered blocks and maintains
 * additional on-disk structures of the atom as log records (each log record
 * occupies one block) for storing of the "wandered map" (a table which
 * contains a relation between wandered and real block numbers) and other
 * information which might be needed at transaction recovery time.

 * The log records are unidirectionally linked into a circle: each log record
 * contains a block number of the next log record, the last log records points
 * to the first one.

 * One log record (named "tx head" in this file) has a format which is
 * different from the other log records. The "tx head" has a reference to the
 * "tx head" block of the previously committed atom.  Also, "tx head" contains
 * fs information (the free blocks counter, and the oid allocator state) which
 * is logged in a special way .

 * There are two journal control blocks, named journal header and journal
 * footer which have fixed on-disk locations.  The journal header has a
 * reference to the "tx head" block of the last committed atom.  The journal
 * footer points to the "tx head" of the last flushed atom.  The atom is
 * "played" when all blocks from its overwrite set are written to disk the
 * second time (i.e. written to their real locations).

 * The atom commit process is the following: 
 *
 * 1. The overwrite set is taken from atom's clean list, and its size is counted.
 *
 * 2. The number of necessary log records (including tx head) is calculated,
 *    and the log record blocks are allocated.
 *
 * 3. Allocate wandered blocks and populate log records by wandered map.
 * 
 * 4. submit write requests for log records and wandered blocks.
 *
 * 5. wait until submitted write requests complete.
 * 
 * 6. update journal header: change the pointer to the block number of just
 * written tx head, submit an i/o for modified journal header block and wait
 * for i/o completion.

 * NOTE: The special logging for bitmap blocks and some reiser4 super block
 * fields makes processes of atom commit, flush and recovering a bit more
 * complex (see comments in the source code for details).

 * The atom flush process (the term "flush" is used here in a different
 * meaning than in flush.c, it means writing block of atom's overwrite set
 * in-place, i.e. to their real locations) follows atom commit.

ZAM-FIXME-HANS: use term "play" and define it too;-)

 * Atom flush:
 *
 * 1. Write atom's overwrite set in-place
 *
 * 2. Wait on i/o.
 *
 * 3. Update journal footer: change the pointer to block number of tx head
 * block of the atom we currently flushing, submit an i/o, wait on i/o
 * completion.
 *
 * 4. Free disk space which was used for wandered blocks and log records.
 *
 * After the freeing of wandered blocks and log records we have that journal
 * footer points to the on-disk structure which might be overwritten soon.
 * Neither the log writer nor the journal recovery procedure use that pointer
 * for accessing the data.  When the journal recovery procedure finds the
 * oldest transaction it compares the journal footer pointer value with the
 * "prev_tx" pointer value in tx head, if values are equal the oldest not
 * flushed transaction is found.
 */

/*
 * Special logging of reiser4 super block fields.
 */

/* There are some reiser4 super block fields (free block count and OID allocator
 * state (number of files and next free OID) which are logged separately from
 * super block to avoid unnecessary atom fusion.
 *
 * So, the reiser4 super block can be not captured by a transaction with
 * allocates/deallocates disk blocks or create/delete file objects.  Moreover,
 * the reiser4 on-disk super block is not touched when such a transaction is
 * committed and flushed.  Those "counters logged specially" are logged in "tx
 * head" blocks and in the journal footer block.
 *
 * The step-by-step description of special logging is the following:
 *
 * 0. The per-atom information about deleted or created files and allocated or
 * freed blocks is collected during the transaction.  The atom's
 * ->nr_objects_created and ->nr_objects_deleted are for object
 * deletion/creation tracking, the numbers of allocated and freed blocks are
 * calculated using atom's delete set and atom's capture list -- all new and
 * relocated nodes should be on atom's clean list and should have JNODE_RELOC
 * bit set.
 * 
 * 1. The "logged specially" reiser4 super block fields have their "committed"
 * versions in the reiser4 in-memory super block.  They get modified only at
 * atom commit time.  The atom's commit thread has an exclusive access to that
 * "committed" fields because the log writer implementation supports only one
 * atom commit a time (it is done by using of per-fs "commit" semaphore).  At
 * that time "committed" counters are modified using per-atom information
 * collected during the transaction. These counters are stored on disk as a
 * part of tx head block when atom is committed.
 * 
 * 2. When the atom is flushed the value of free block counter and OID allocator
 * state get written to the journal footer block.  A special journal procedure
 * (journal_recover_sb_data()) takes those values from the journal footer and
 * updates the reiser4 in-memory super block.
 *
 * NOTE: That means free block count and OID allocator state are logged
 * separately from the reiser4 super block regardless of the fact that the
 * reiser4 super block has fields to store both the free block counter and the
 * OID allocator.
 *
 * The reason for that is that for the writing of the reiser4 super block we
 * need to know the actual values of all of its fields.  For example if we
 * have a transaction which has not captured the super block we cannot
 * re-write the super block when such a transaction is committed because we do
 * not know the actual value of the tree root pointer. Knowing that requires
 * write locking of the super block and implies some dependency between atoms
 * which we wanted to avoid. So, the simplest solution seems to be the
 * implemented one, in which the data logged in different ways are written in
 * different blocks.
 */


#include "reiser4.h"

int WRITE_LOG = 1;		/* journal is written by default  */


static int submit_write (jnode*, int, const reiser4_block_nr *, int use_io_handle);

/* The commit_handle is a container for objects needed at atom commit time  */
struct commit_handle {
	/* atom's overwrite set is temporary put here */
	capture_list_head     overwrite_set;
	/* atom's overwrite set size */
	int                   overwrite_set_size;
	/* jnodes for log record blocks */
	capture_list_head     tx_list;
	/* number of log records */
	int                   tx_size;
	/* 'committed' sb counters are saved here until atom is completely
	 * flushed  */
	__u64                 free_blocks;
	__u64                 nr_files;
	__u64                 next_oid;
	/* the atom, currently is being committed */
	txn_atom             *atom;
	/* current super block */
	struct super_block   *super;
};


static void init_commit_handle (struct commit_handle * ch, txn_atom * atom)
{
	xmemset (ch, 0, sizeof (struct commit_handle));
	capture_list_init (&ch->overwrite_set);
	capture_list_init (&ch->tx_list);

	ch->atom  = atom;
	ch->super = reiser4_get_current_sb ();
}

static void done_commit_handle (struct commit_handle * ch)
{
	assert ("zam-689", capture_list_empty (&ch->overwrite_set));
	assert ("zam-690", capture_list_empty (&ch->tx_list)); 
}

/**
 * fill journal header block data 
 */
static void format_journal_header (struct commit_handle * ch)
{
	struct reiser4_super_info_data * private;
	struct journal_header * h;
	jnode * txhead;

	private = get_super_private(ch->super);
	assert ("zam-479", private != NULL);
	assert ("zam-480", private->journal_header != NULL);

	txhead = capture_list_front(&ch->tx_list);

	jload (private->journal_header);

	h = (struct journal_header*)jdata(private->journal_header);
	assert ("zam-484", h != NULL);

	cputod64(*jnode_get_block(txhead), &h->last_committed_tx);

	jrelse (private->journal_header);
}

/**
 * fill journal footer block data
 */
static void format_journal_footer (struct commit_handle * ch)
{
	struct reiser4_super_info_data * private;
	struct journal_footer * F;

	jnode * tx_head;

	private = get_super_private (ch->super);

	tx_head = capture_list_front (&ch->tx_list);

	assert ("zam-493", private != NULL);
	assert ("zam-494", private->journal_header != NULL);

	check_me ("zam-691", jload (private->journal_footer) == 0);

	F = (struct journal_footer*)jdata(private->journal_footer);
	assert ("zam-495", F != NULL);

	cputod64(*jnode_get_block(tx_head), &F->last_flushed_tx);
	cputod64(ch->free_blocks, &F->free_blocks);

	cputod64(ch->nr_files, &F->nr_files);
	cputod64(ch->next_oid, &F->next_oid);

	jrelse (private->journal_footer);
}

/* log record capacity depends on current block size */
static int log_record_capacity (const struct super_block * super)
{
	return (super->s_blocksize - sizeof (struct log_record_header)) /
		sizeof (struct log_entry);
}

/**
 * fill first log record (tx head) in accordance with supplied given data
 */
static void format_tx_head (struct commit_handle * ch)
{
	jnode * tx_head;
	jnode * next;
	struct tx_header * TH;

	tx_head = capture_list_front (&ch->tx_list);
	assert ("zam-692", !capture_list_end (&ch->tx_list, tx_head));

	next  = capture_list_next (tx_head);
	if (capture_list_end (&ch->tx_list, next))
		next = tx_head;

	TH = (struct tx_header*)jdata(tx_head);

	assert ("zam-460", TH != NULL);
	assert ("zam-462", ch->super->s_blocksize >= sizeof (struct tx_header));

	xmemset (jdata(tx_head), 0, (size_t)ch->super->s_blocksize);
	xmemcpy (jdata(tx_head), TX_HEADER_MAGIC, TX_HEADER_MAGIC_SIZE); 

	cputod32((__u32)ch->tx_size, & TH->total);
	cputod64(get_super_private(ch->super)->last_committed_tx, & TH->prev_tx);
	cputod64(*jnode_get_block(next), & TH->next_block );

	cputod64(ch->free_blocks, & TH->free_blocks);
	cputod64(ch->nr_files   , & TH->nr_files);
	cputod64(ch->next_oid   , & TH->next_oid);
}

/**
 * prepare ordinary log record block (fill all service fields)
 */
static void format_log_record (struct commit_handle * ch, jnode * node, int serial)
{
	struct log_record_header * LRH;
	jnode * next;

	assert ("zam-464", node != NULL);

	LRH = (struct log_record_header*) jdata (node); 
	next = capture_list_next (node);

	if (capture_list_end (&ch->tx_list, next))
		next = capture_list_front (&ch->tx_list);

	assert ("zam-465", LRH != NULL);
	assert ("zam-463", ch->super->s_blocksize > sizeof (struct log_record_header));

	xmemset (jdata(node), 0, (size_t)ch->super->s_blocksize);
	xmemcpy (jdata(node), LOG_RECORD_MAGIC, LOG_RECORD_MAGIC_SIZE);

//	cputod64((__u64)reiser4_trans_id(super), &h->id);
	cputod32((__u32)ch->tx_size,            & LRH->total     );
	cputod32((__u32)serial,                 & LRH->serial    );
	cputod64((__u64)*jnode_get_block(next), & LRH->next_block);
}

/* add one wandered map entry to formatted log record */
static void store_entry (jnode * node, 
			 int index,
			 const reiser4_block_nr * a,
			 const reiser4_block_nr *b)
{
	char * data;
	struct log_entry * pairs;

	data = jdata (node);
	assert ("zam-451", data != NULL);

	pairs = (struct log_entry *)(data + sizeof (struct log_record_header));

	cputod64(*a, & pairs[index].original);
	cputod64(*b, & pairs[index].wandered);
}


/* currently, log records contains contain only wandered map, which depend on
 * overwrite set size */
static void get_tx_size (struct commit_handle * ch)
{
	assert ("zam-440", ch->overwrite_set_size != 0);
	assert ("zam-695", ch->tx_size == 0);

	/* count all ordinary log records 
	 * (<overwrite_set_size> - 1) / <log_record_capacity> + 1 and add one
	 * for tx head block */
	ch->tx_size = (ch->overwrite_set_size - 1) / log_record_capacity (ch->super) + 2;
}

/* A special structure for using in store_wmap_actor() for saving its state
 * between calls */
struct store_wmap_params {
	jnode * cur;		/* jnode of current log record to fill */
	int     idx;		/* free element index in log record  */
	int     capacity;	/* capacity  */

#if REISER4_DEBUG
	capture_list_head* tx_list;
#endif
};

/* an actor for use in blocknr_set_iterator routine which populates the list
 * of pre-formatted log records by wandered map info */
static int store_wmap_actor (txn_atom * atom UNUSED_ARG,
			     const reiser4_block_nr * a,
			     const reiser4_block_nr * b,
			     void * data)
{
	struct store_wmap_params * params = data;

	if (params->idx >= params->capacity) {
		/* a new log record should be taken from the tx_list */
		params->cur = capture_list_next (params->cur);
		assert ("zam-454", !capture_list_end(params->tx_list, params->cur));

		params->idx = 0;
	}

	store_entry (params->cur, params->idx, a, b);
	params->idx ++;

	return 0;
}

/* This function is called after Relocate set gets written to disk, Overwrite
 * set is written to wandered locations and all log records are written
 * also. Updated journal header blocks contains a pointer (block number) to
 * first log record of the just written transaction */
static int update_journal_header (struct commit_handle * ch)
{
	struct reiser4_super_info_data * private = get_super_private (ch->super);

	jnode * jh = private->journal_header;
	jnode * head = capture_list_front (&ch->tx_list);

	int ret;

	format_journal_header (ch);

	ret = submit_write(jh, 1, jnode_get_block(jh), 0);

	if (ret) return ret;

	blk_run_queues();

	ret = jwait_io (jh, WRITE);

	if (ret) return ret;

	private->last_committed_tx = *jnode_get_block(head);

	return 0;
}
	
/* This function is called after write-back is finished. We update journal
 * footer block and free blocks which were occupied by wandered blocks and
 * transaction log records */
static int update_journal_footer (struct commit_handle * ch)
{
	reiser4_super_info_data * private = get_super_private(ch->super);

	jnode * jf = private->journal_footer;

	int ret;

	format_journal_footer (ch);

	ret = submit_write (jf, 1, jnode_get_block(jf), 0);
	if (ret) return ret;

	blk_run_queues();

	ret = jwait_io (jf, WRITE);
	if (ret) return ret;

	return 0;
}


/* free block numbers of log records of already written in place transaction */
static void dealloc_tx_list (struct commit_handle * ch) 
{
	while (!capture_list_empty (&ch->tx_list)) {
		jnode * cur = capture_list_pop_front(&ch->tx_list);

		reiser4_dealloc_block (jnode_get_block (cur), 0, BLOCK_NOT_COUNTED);

		unpin_jnode_data (cur);
		drop_io_head (cur);
	}
}

/* An actor for use in block_nr_iterator() routine which frees wandered blocks
 * from atom's overwrite set. */
static int dealloc_wmap_actor (
	txn_atom               * atom,
	const reiser4_block_nr * a UNUSED_ARG, 
	const reiser4_block_nr * b,
	void                   * data UNUSED_ARG)
{

	assert ("zam-499", b != NULL);
	assert ("zam-500", *b != 0);
	assert ("zam-501", !blocknr_is_fake(b));

	spin_unlock_atom(atom);
	reiser4_dealloc_block (b, 0, BLOCK_NOT_COUNTED);
	spin_lock_atom(atom);

	return 0;
}

/* free wandered block locations of already written in place transaction */
static void dealloc_wmap (struct commit_handle * ch)
{
	assert ("zam-696", ch->atom != NULL);

	spin_lock_atom (ch->atom);
	blocknr_set_iterator (ch->atom, &ch->atom->wandered_map, dealloc_wmap_actor, NULL, 1);
	spin_unlock_atom(ch->atom);
}


/* helper function for alloc wandered blocks, which refill set of block
 * numbers needed for wandered blocks  */
static int get_more_wandered_blocks (int count, reiser4_block_nr * start, int *len)
{
	reiser4_blocknr_hint hint;
	int ret;

	reiser4_block_nr wide_len = count;

	/* FIXME-ZAM: A special policy needed for allocation of wandered
	 * blocks */
	blocknr_hint_init (&hint);
	hint.block_stage = BLOCK_GRABBED;
	
	ret = reiser4_alloc_blocks (&hint, start, &wide_len, 1/*not unformatted*/);

	blocknr_hint_done (&hint);

	*len = (int)wide_len;

	return ret;
}

/* count overwrite set size and place overwrite set on a separate list  */
static int get_overwrite_set (struct commit_handle * ch)
{
	capture_list_head * head;
	jnode * cur;

	assert ("zam-697", ch->overwrite_set_size == 0);

	head = &ch->atom->clean_nodes;
	cur = capture_list_front(head);

	while (!capture_list_end (head, cur)) {
		jnode * next = capture_list_next(cur);

		if (jnode_is_znode(cur) && znode_above_root(JZNODE(cur))) {
			trace_on (TRACE_LOG, "fake znode found , WANDER=(%d)\n", JF_ISSET(cur, JNODE_WANDER));
		}

		assert ("nikita-2591", !jnode_check_dirty (cur));
		if (0 && jnode_page (cur) && 
		    PageDirty (jnode_page (cur)) && !JF_ISSET(cur, JNODE_WANDER))
			rpanic( "nikita-2590", "Wow!" );

		if (JF_ISSET(cur, JNODE_WANDER)) { 
			capture_list_remove_clean (cur);

			if (jnode_is_znode(cur) && znode_above_root(JZNODE(cur))) {
				/* we replace fake znode by another (real)
				 * znode which is suggested by disk_layout
				 * plugin */

				/* FIXME: it looks like fake znode should be
				 * replaced by jnode supplied by
				 * disk_layout. */

				struct super_block * s = reiser4_get_current_sb();
				reiser4_super_info_data * private = get_current_super_private();

				if (private->df_plug->log_super) {
					jnode *sj = private->df_plug->log_super(s);

					assert ("zam-593", sj != NULL);

					if (IS_ERR(sj)) return PTR_ERR(sj);

					spin_lock_jnode (sj);

					jnode_set_wander(sj);
					capture_list_push_back (&ch->overwrite_set, sj);
					sj->atom = ch->atom;
					jref(sj);

					spin_unlock_jnode(sj);

					ch->overwrite_set_size ++;
				} else {
					/* the fake znode was removed from
					 * overwrite set and from capture
					 * list, no jnode was added to
					 * transaction -- we decrement atom's
					 * capture count */
					ch->atom->capture_count --;
				}

				spin_lock_jnode(cur);

				cur->atom = NULL;
				JF_CLR(cur, JNODE_WANDER);

				spin_unlock_jnode(cur);
				jput(cur);

			} else {
				capture_list_push_back (&ch->overwrite_set, cur);
				ch->overwrite_set_size ++;
			}
		}

		cur = next;
	}

	return ch->overwrite_set_size;
}

/* overwrite set nodes IO completion handler */
static int wander_end_io (struct bio * bio, unsigned int bytes_done, int err)
{
	int i;

	if (bio->bi_size != 0)
		return 1;

	for (i = 0; i < bio->bi_vcnt; i += 1) {
		struct page *pg = bio->bi_io_vec[i].bv_page;

		if (! test_bit (BIO_UPTODATE, & bio->bi_flags))
			SetPageError (pg);

		end_page_writeback (pg);
		page_cache_release (pg);
	}

	io_handle_end_io(bio);

	bio_put(bio);
	return 0;
}

/** Submit a write request for @nr jnodes beginning from the @first, other
 * jnodes are after the @first on the double-linked "capture" list.  All
 * jnodes will be written to the disk region of @nr blocks starting with
 * @block_p block number.  If @use_io_handle option is set (!= 0) it means
 * that i/o handle object is allocated and it will be used for this i/o
 * request synchronization.*/
static int submit_write (jnode * first, int nr, 
			 const reiser4_block_nr * block_p,
			 int use_io_handle)
{
	struct super_block * super = reiser4_get_current_sb();
	int max_blocks;
	jnode * cur = first;
	reiser4_block_nr block;

	assert ("zam-571", first != NULL);
	assert ("zam-572", block_p != NULL);
	assert ("zam-570", nr > 0);

	block = *block_p;

#if REISER4_USER_LEVEL_SIMULATION
	max_blocks = nr;
#else
	max_blocks = bdev_get_queue (super->s_bdev)->max_sectors >> (super->s_blocksize_bits - 9);
#endif

	while (nr > 0) {
		struct bio * bio;
		int nr_blocks = min (nr, max_blocks);
		int i;
		int ret;

		bio = bio_alloc(GFP_NOIO, nr_blocks);
		if (!bio)
			return -ENOMEM;

		assert ("zam-574", jnode_page (first) != NULL);

		bio->bi_sector = block * (super->s_blocksize >>9);
		bio->bi_bdev   = super->s_bdev;
		bio->bi_vcnt   = nr_blocks;
		bio->bi_size   = super->s_blocksize * nr_blocks;
		bio->bi_end_io = wander_end_io;

		for (i = 0; i < nr_blocks; i++) {
			struct page * pg;

			pg = jnode_page(cur);
			assert ("zam-573", pg != NULL);

			page_cache_get (pg);
			lock_page (pg);

			assert ("zam-605", !PageWriteback(pg));
			SetPageWriteback (pg);
			set_page_clean_nolock(pg);

			unlock_page (pg);

			/*
			 * prepare node to being written
			 */
			jnode_ops (cur)->io_hook (cur, pg, WRITE);

			bio->bi_io_vec[i].bv_page   = pg;
			bio->bi_io_vec[i].bv_len    = super->s_blocksize;
			bio->bi_io_vec[i].bv_offset = 0;

			cur = capture_list_next (cur);
		}

		if (use_io_handle) {
			ret = current_atom_add_bio(bio);
			if (ret) {
				bio_put(bio);
				return ret;
			}
		} else {
			bio->bi_private = NULL;
		}

		submit_bio(WRITE, bio);

		nr -= nr_blocks;

		block += nr_blocks - 1;
		reiser4_update_last_written_location(super, &block);
		block += 1;
	}

	return 0;
}

/* This is a procedure which recovers a contiguous sequences of disk block
 * numbers in the given list of j-nodes and submits write requests on this
 * per-sequence basis */
static int submit_batched_write (capture_list_head * head, int use_io_handle)
{
	int ret;
	jnode * beg = capture_list_front(head);

	while (!capture_list_end(head, beg)) {
		int nr = 1;
		jnode * cur = capture_list_next(beg);

		while (!capture_list_end(head, cur)) {
			if (*jnode_get_block(cur) != *jnode_get_block(beg) + nr) break;
			++ nr;
			cur = capture_list_next(cur);
		}

		ret = submit_write(beg, nr, jnode_get_block(beg), use_io_handle);
		if (ret) return ret;

		beg = cur;
	}

	return 0;
}

/* allocate given number of nodes over the journal area and link them into a
 * list, return pinter to the first jnode in the list */
static int alloc_tx (struct commit_handle * ch)
{
	reiser4_blocknr_hint  hint;

	reiser4_block_nr allocated = 0;
	reiser4_block_nr first, len;

	jnode * cur;
	jnode * txhead;
	int ret;

	assert ("zam-698", ch->tx_size > 0);
	assert ("zam-699", capture_list_empty (&ch->tx_list));

	while (allocated < (unsigned)ch->tx_size) {
		len  = (ch->tx_size - allocated);

		blocknr_hint_init (&hint);

		hint.block_stage = BLOCK_GRABBED;

		/* FIXME: there should be some block allocation policy for
		 * nodes which contain log records */
		ret = reiser4_alloc_blocks (&hint, &first, &len, 1/*not unformatted*/);

		blocknr_hint_done (&hint);

		if (ret) return ret;

		allocated += len;

		/* create jnodes for all log records */
		while (len --) {
			cur = alloc_io_head (&first);

			if (cur == NULL) {
				ret = -ENOMEM;
				goto free_not_assigned;
			}

			ret = jinit_new(cur);

			if (ret != 0) {
				jfree (cur);
				goto free_not_assigned;
			}

			pin_jnode_data (cur);

			capture_list_push_back (&ch->tx_list, cur);

			first ++;
		}
	}

	{ /* format a on-disk linked list of log records */
		int serial = 1;

		txhead = capture_list_front(&ch->tx_list);
		format_tx_head(ch);

		cur = capture_list_next (txhead);
		while (!capture_list_end (&ch->tx_list, cur)) {
			format_log_record (ch, cur, serial ++);
			cur = capture_list_next (cur);
		}

	}

	{ /* Fill log records with Wandered Set */
		struct store_wmap_params params;
		txn_atom * atom;

		params.cur = capture_list_next (txhead);

		params.idx = 0;
		params.capacity = log_record_capacity (reiser4_get_current_sb());

		atom = get_current_atom_locked ();
		blocknr_set_iterator (atom, &atom->wandered_map, &store_wmap_actor, &params , 0);
		spin_unlock_atom (atom);
	}

	{ /* relse all jnodes from tx_list*/
		cur = capture_list_front (&ch->tx_list);
		while (!capture_list_end (&ch->tx_list, cur)) {
			jrelse (cur);
			cur = capture_list_next (cur);
		}
	}

	ret = submit_batched_write(&ch->tx_list, 1);

	return ret;

 free_not_assigned:
	/* We deallocate blocks not yet assigned to jnodes on tx_list. The
	 * caller takes care about invalidating of tx list  */
	reiser4_dealloc_blocks(&first, &len, 0, BLOCK_GRABBED);

	return ret;
}

/* add given wandered mapping to atom's wandered map */
static int add_region_to_wmap (jnode * cur,
			       int len,
			       const reiser4_block_nr *block_p)
{
	int ret;
	blocknr_set_entry *new_bsep = NULL;
	reiser4_block_nr block;

	txn_atom * atom;

	assert ("zam-568", block_p != NULL);
	block = *block_p;
	assert ("zam-569", len > 0);

	while ((len --) > 0) {  
		do {
			atom = get_current_atom_locked();
			assert ("zam-536", !blocknr_is_fake (jnode_get_block(cur)));
			ret = blocknr_set_add_pair (
				atom, &atom->wandered_map, &new_bsep, jnode_get_block(cur), &block );
		} while (ret == -EAGAIN);

		if (ret) {
			/* deallocate blocks which were not added to wandered
			 * map */
			reiser4_block_nr wide_len = len;

			reiser4_dealloc_blocks(&block, &wide_len, 0, BLOCK_NOT_COUNTED); 
			return ret;
		}

		spin_unlock_atom(atom);

		cur = capture_list_next(cur);
		++ block;
	}

	return 0;
}

/* Allocate wandered blocks for current atom's OVERWRITE SET and immediately
 * submit IO for allocated blocks.  We assume that current atom is in a stage
 * when any atom fusion is impossible and atom is unlocked and it is safe. */
int alloc_wandered_blocks (struct commit_handle * ch)
{
	reiser4_block_nr block;

	int     rest;
	int     len;
	int     ret;

	jnode * cur;

	assert ("zam-534", ch->overwrite_set_size > 0);

	rest = ch->overwrite_set_size;

	cur = capture_list_front(&ch->overwrite_set);
	while (!capture_list_end(&ch->overwrite_set, cur)) {
		assert ("zam-567", JF_ISSET(cur, JNODE_WANDER));

		ret = get_more_wandered_blocks (rest, &block, &len); 
		if (ret) return ret;

		rest -= len;

		ret = add_region_to_wmap(cur, len, &block);
		if (ret) return ret;

		ret = submit_write (cur, len, &block, 1);
		if (ret) return ret;

		while ((len --) > 0) {
			assert ("zam-604", !capture_list_end(&ch->overwrite_set, cur));
			cur = capture_list_next(cur);
		}
	}

	return 0;
}

/* We assume that at this moment all captured blocks are marked as RELOC or
 * WANDER (belong to Relocate o Overwrite set), all nodes from Relocate set
 * are submitted to write.*/
int reiser4_write_logs (void)
{
	txn_atom        * atom;

	struct super_block * super = reiser4_get_current_sb();
	reiser4_super_info_data * private = get_super_private(super);

	struct commit_handle ch;

	int ret;

	/* isolate critical code path which should be executed by only one
 	 * thread using tmgr semaphore */
	down (&private->tmgr.commit_semaphore);

	/* block allocator may add j-nodes to the clean_list */
	pre_commit_hook();

	atom = get_current_atom_locked();

	private->nr_files_committed += (unsigned)atom->nr_objects_created;
	private->nr_files_committed -= (unsigned)atom->nr_objects_deleted;

	spin_unlock_atom(atom);

	init_commit_handle (&ch, atom);

	ch.free_blocks = private->blocks_free_committed;
	ch.nr_files    = private->nr_files_committed;
	ch.next_oid    = oid_next ();

	/* count overwrite set and place it in a separate list */
	ret =  get_overwrite_set (&ch);

	if (ret <= 0) {
		/* It is possible that overwrite set is empty here, it means
		 * all captured nodes are clean */
		/* FIXME: an extra check for empty RELOC set should be here */
		goto up_and_ret;
	}

	/* count all records needed for storing of the wandered set */
	get_tx_size (&ch);

	if ((ret = reiser4_grab_space_exact((__u64)(ch.overwrite_set_size + ch.tx_size))))
		goto up_and_ret;

	if ((ret = alloc_wandered_blocks (&ch)))
		goto up_and_ret;

	if ((ret = alloc_tx (&ch)))
		goto up_and_ret;

	ret = current_atom_wait_on_io();
	if (ret) goto up_and_ret;

	trace_on (TRACE_LOG, "overwrite set written to wandered locations\n");

	if ((ret = update_journal_header(&ch)))
		goto up_and_ret;

	trace_on (TRACE_LOG,
		  "journal header updated (tx head at block %llX)\n",
		  (unsigned long long int)(*jnode_get_block(capture_list_front(&ch.tx_list))));

	post_commit_hook();

	/* force j-nodes write back */
	if ((ret = submit_batched_write(&ch.overwrite_set, 1)))
		goto up_and_ret;

	ret = current_atom_wait_on_io();
	if (ret) goto up_and_ret;


	trace_on (TRACE_LOG, "overwrite set written in place\n");


	if ((ret = update_journal_footer(&ch)))
		goto up_and_ret;

	trace_on (TRACE_LOG,
		  "journal footer updated (tx head at block %llX)\n",
		  (unsigned long long int)(*jnode_get_block(capture_list_front(&ch.tx_list))));

	post_write_back_hook();

 up_and_ret:
	up(&private->tmgr.commit_semaphore);

	/* free blocks of flushed transaction */

	dealloc_tx_list(&ch);
	dealloc_wmap (&ch);

	/*reiser4_release_all_grabbed_space();*/
	all_grabbed2free();

	capture_list_splice (&ch.atom->clean_nodes, &ch.overwrite_set);

	done_commit_handle (&ch);

	return ret;
}

/* consistency checks for journal data/control blocks: header, footer, log
 * records, transactions head blocks. All functions return zero on success. */

static int check_journal_header (const jnode * node UNUSED_ARG)
{
	/* FIXME: journal header has no magic field yet. */
	return 0;
}

/* wait for write completion for all jnodes from given list */
static int wait_on_jnode_list (capture_list_head * head)
{
	jnode *scan;
	int ret = 0;

	for (scan = capture_list_front (head);
	     ! capture_list_end   (head, scan);
	     scan = capture_list_next  (scan))
	{
		struct page * pg = scan->pg;

		if (pg) {
			if (PageWriteback (pg))
				wait_on_page_writeback (scan->pg);

			if (PageError (pg))
				ret ++;
		}
	}

	return ret;
}

static int check_journal_footer (const jnode * node UNUSED_ARG)
{
	/* FIXME: journal footer has no magic field yet. */
	return 0;
}

static int check_tx_head (const jnode * node)
{
	struct tx_header * TH = (struct tx_header*) jdata(node);

	if (memcmp(&TH->magic, TX_HEADER_MAGIC, TX_HEADER_MAGIC_SIZE) != 0) {
		warning ("zam-627", "tx head at block %llX corrupted\n",
			 (unsigned long long int)(*jnode_get_block(node)));
		return -EIO;
	}

	return 0;
}

static int check_log_record (const jnode * node)
{
	struct log_record_header *RH = (struct log_record_header *)jdata(node);

	if (memcmp(&RH->magic, LOG_RECORD_MAGIC, LOG_RECORD_MAGIC_SIZE) != 0) {
		warning ("zam-628", "log record at block %llX corrupted\n",
			 (unsigned long long int)(*jnode_get_block(node)));
		return -EIO;
	}

	return 0;
}

/* fill commit_handler structure by everything what is needed for update_journal_footer */
static int restore_commit_handle (struct commit_handle * ch, jnode * tx_head)
{
	struct tx_header * TXH;
	int ret;

	ret = jload (tx_head);

	if (ret) return ret;

	TXH = (struct tx_header*)jdata (tx_head);

	ch->free_blocks  = d64tocpu (&TXH->free_blocks);
	ch->nr_files     = d64tocpu (&TXH->nr_files);
	ch->next_oid     = d64tocpu (&TXH->next_oid);

	jrelse (tx_head);

	capture_list_push_front (&ch->tx_list, tx_head);

	return 0;
} 

/** replay one transaction: restore and write overwrite set in place */
static int replay_transaction (const struct super_block * s,
			       jnode * tx_head,
			       const reiser4_block_nr * log_rec_block_p, 
			       const reiser4_block_nr * end_block,
			       unsigned int nr_log_records)
{
	reiser4_block_nr log_rec_block = *log_rec_block_p;
	struct commit_handle ch;
	jnode * log;
	int ret;

	init_commit_handle (&ch, NULL);

	ret = restore_commit_handle (&ch, tx_head);

	while (log_rec_block != *end_block) {
		struct log_record_header * LH;
		struct log_entry * LE;

		int i;

		if (nr_log_records == 0) { 
			warning ("zam-631", "number of log records in the linked list"
				 " greater than number stored in tx head.\n");
			ret =  -EIO;
			goto free_ow_set;
		}

		log = alloc_io_head(&log_rec_block);
		if (log == NULL) return -ENOMEM; 

		ret = jload(log);
		if (ret < 0) { drop_io_head(log); return ret; }

		ret= check_log_record(log);
		if (ret) { jrelse(log); drop_io_head(log); return ret; }

		LH = (struct log_record_header *)jdata(log);
		log_rec_block = d64tocpu(&LH->next_block);

		LE = (struct log_entry *)(LH + 1);

		/* restore overwrite set from log record content */
		for (i = 0; i < log_record_capacity(s); i++) {
			reiser4_block_nr block;
			jnode * node;

			block = d64tocpu(&LE->wandered);

			if (block == 0) break;

			node = alloc_io_head(&block);
			if (node == NULL) { ret = -ENOMEM; goto free_ow_set; }

			ret = jload(node);

			if (ret < 0) { drop_io_head (node); goto free_ow_set; }

			block = d64tocpu(&LE->original);

			assert ("zam-603", block != 0);

			jnode_set_block(node, &block);

			capture_list_push_back (&ch.overwrite_set, node);

			++ LE;
		}

		jrelse(log);
		drop_io_head(log);

		-- nr_log_records;
	}

	if (nr_log_records != 0) {
		warning ("zam-632", "number of log records in the linked list"
			 " less than number stored in tx head.\n");
		ret = -EIO;
		goto free_ow_set;
	} 

	{       /* write wandered set in place */
		submit_batched_write(&ch.overwrite_set, 0);
		ret = wait_on_jnode_list (&ch.overwrite_set);

		if (ret) {
			ret = -EIO;
			goto free_ow_set;
		}
	}

	ret = update_journal_footer(&ch);

 free_ow_set:

	while (!capture_list_empty(&ch.overwrite_set)) {
		jnode * cur = capture_list_pop_front(&ch.overwrite_set);
		jrelse(cur);
		drop_io_head(cur);
	}

	done_commit_handle (&ch);

	return ret;
}


/* find oldest not flushed transaction and flush it */
static int replay_oldest_transaction(struct super_block * s)
{
	reiser4_super_info_data * private = get_super_private(s);
	jnode *jf = private->journal_footer;
	unsigned int total;
	struct journal_footer * F;
	struct tx_header * T;

	reiser4_block_nr prev_tx;
	reiser4_block_nr last_flushed_tx;
	reiser4_block_nr log_rec_block = 0;

	jnode * tx_head;

	int ret;

	if ((ret = jload(jf)) < 0) return ret;

	F = (struct journal_footer*)jdata(jf);

	last_flushed_tx = d64tocpu(&F->last_flushed_tx);

	jrelse(jf);

	if(private->last_committed_tx == last_flushed_tx) {
		/* all transactions are replayed */
		return 0;
	}

	trace_on(TRACE_REPLAY, "not flushed transactions found.");

	prev_tx = private->last_committed_tx;

	/* searching for oldest not flushed transaction */
	while (1) {
		tx_head = alloc_io_head (&prev_tx);
		if (!tx_head) return -ENOMEM;

		ret = jload(tx_head);
		if (ret < 0 ) { drop_io_head (tx_head); return ret;}

		ret = check_tx_head(tx_head);
		if (ret) { jrelse(tx_head); drop_io_head(tx_head); return ret; }

		T = (struct tx_header*)jdata(tx_head);

		prev_tx = d64tocpu(&T->prev_tx);

		if (prev_tx == last_flushed_tx)
			break;

		jrelse(tx_head);
		drop_io_head(tx_head);
	}

	total = d32tocpu(&T->total);
	log_rec_block = d64tocpu(&T->next_block);

	trace_on(TRACE_REPLAY, "not flushed transaction found (head block %llX, %u log records)\n",
		 (unsigned long long)(*jnode_get_block(tx_head)), total);

	pin_jnode_data(tx_head);
	jrelse(tx_head);

	ret = replay_transaction(s, tx_head, &log_rec_block, jnode_get_block(tx_head), total - 1);

	unpin_jnode_data(tx_head);
	drop_io_head(tx_head);

	if (ret) return ret;
	return -EAGAIN;
}

/* The reiser4 journal current implementation was optimized to not to capture
 * super block if certain super blocks fields are modified. Currently, the set
 * is (<free block count>, <OID allocator>). These fields are logged by
 * special way which includes storing them in each transaction head block at
 * atom commit time and writing that information to journal footer block at
 * atom flush time.  For getting info from journal footer block to the
 * in-memory super block there is a special function
 * reiser4_journal_recover_sb_data() which should be called after disk format
 * plugin re-reads super block after journal replaying.
 */

/* get the information from journal footer in-memory super block */
int reiser4_journal_recover_sb_data (struct super_block * s)
{
	reiser4_super_info_data * private = get_super_private (s);
	struct journal_footer * JF;
	int ret;


	assert ("zam-673", private->journal_footer != NULL);

	if ((ret = jload (private->journal_footer)))
		return ret;

	if ((ret = check_journal_footer (private->journal_footer)))
		goto out;

	JF = (struct journal_footer *)jdata(private->journal_footer);

	/* was there at least one flushed transaction?  */
	if (d64tocpu(&JF->last_flushed_tx)) {

		/* restore free block counter logged in this transaction */
		reiser4_set_free_blocks (s, d64tocpu(&JF->free_blocks));

		/* restore oid allocator state */
		oid_init_allocator (s, d64tocpu(&JF->nr_files), d64tocpu(&JF->next_oid));
	}
 out:
	jrelse (private->journal_footer);
	return ret;
}

/* reiser4 replay journal procedure */
int reiser4_journal_replay (struct super_block * s)
{
	/* FIXME: it is a limited version of journal replaying which just
	 * takes saved free block counter from journal footer, searches for
	 * not flushed transaction and prints a warning, if those transactions
	 * found.*/

	reiser4_super_info_data * private = get_super_private(s);
	jnode *jh, *jf;

	struct journal_header * H;
	int nr_tx_replayed = 0;

	int ret;

	assert ("zam-582", private != NULL);

	jh = private->journal_header;
	jf = private->journal_footer;

	if (!jh || !jf) {
		/* it is possible that disk layout does not support journal
		 * structures, we just warn about this */
		warning ("zam-583", "journal control blocks were not loaded by disk layout plugin.  "
			 "journal replaying is not possible.\n");
		return 0;
	}

	/* Take free block count from journal footer block. The free block
	 * counter value corresponds the last flushed transaction state */
	ret = jload(jf);
	if (ret < 0) return ret;

	ret = check_journal_footer(jf);
	if (ret) { jrelse(jf); return ret; }

	jrelse(jf);

	/* store last committed transaction info in reiser4 in-memory super
	 * block */
	ret = jload(jh);
	if (ret < 0) return ret;

	ret = check_journal_header(jh);
	if (ret) { jrelse(jh); return ret; }

	H = (struct journal_header *)jdata(jh);
	private->last_committed_tx = d64tocpu(&H->last_committed_tx);

	jrelse(jh);

	/* replay committed transactions */
	while ((ret = replay_oldest_transaction(s)) == -EAGAIN) 
		nr_tx_replayed ++;

	trace_on(TRACE_REPLAY, "%d transactions replayed ret = %d", nr_tx_replayed, ret);

	return ret;
}

/* load journal header or footer */
static int load_journal_control_block (jnode ** node,  const reiser4_block_nr * block)
{
	int ret;

	*node = alloc_io_head (block);
	if (!(*node)) return -ENOMEM;

	ret = jload(*node);

	if (ret) { drop_io_head(*node); *node = NULL; return ret;}

	pin_jnode_data(*node);
	jrelse(*node);

	return 0;
}

/* unload journal header or footer and free jnode */
static void unload_journal_control_block (jnode ** node)
{
	if (*node) {
		unpin_jnode_data(*node);
		drop_io_head(*node);
		*node = NULL;
	}
}

/* release journal control blocks */
void done_journal_info (struct super_block *s)
{
	reiser4_super_info_data * private = get_super_private (s);

	assert ("zam-476", private != NULL);

	unload_journal_control_block(&private->journal_header);
	unload_journal_control_block(&private->journal_footer);
}

/* load journal control blocks */
int init_journal_info (struct super_block * s, 
		       const reiser4_block_nr * header_block,
		       const reiser4_block_nr * footer_block)
{
	reiser4_super_info_data * private = get_super_private(s);
	int ret;

	assert ("zam-650", header_block != NULL);
	assert ("zam-651", footer_block != NULL);
	assert ("zam-652", *header_block != 0);
	assert ("zam-653", *footer_block != 0);

	ret = load_journal_control_block (&private->journal_header, header_block);

	if (ret) return ret;

	ret = load_journal_control_block (&private->journal_footer, footer_block);

	if (ret) {
		unload_journal_control_block(&private->journal_header);
	}

	return ret;
}

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 78
 * End:
 */
