/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40.c -- reiser4 journal plugin. */

#ifndef ENABLE_MINIMAL

#include "journal40.h"
#include "journal40_repair.h"

static uint32_t journal40_get_state(reiser4_journal_ent_t *entity) {
	aal_assert("umka-2081", entity != NULL);
	return PLUG_ENT(entity)->state;
}

static void journal40_set_state(reiser4_journal_ent_t *entity,
				uint32_t state)
{
	aal_assert("umka-2082", entity != NULL);
	PLUG_ENT(entity)->state = state;
}

static errno_t journal40_valid(reiser4_journal_ent_t *entity) {
	aal_assert("umka-965", entity != NULL);
	return 0;
}

/* Journal enumerator function. */
static errno_t journal40_layout(reiser4_journal_ent_t *entity,
				region_func_t region_func,
				void *data)
{
	blk_t blk;
	
	aal_assert("umka-1040", entity != NULL);
	aal_assert("umka-1041", region_func != NULL);
	
	blk = JOURNAL40_BLOCKNR(PLUG_ENT(entity)->blksize);
	return region_func(blk, 2, data);
}

aal_device_t *journal40_device(reiser4_journal_ent_t *entity) {
	aal_assert("vpf-455", entity != NULL);
	return PLUG_ENT(entity)->device;
}

/* Helper function fetching journal footer and header. */
static errno_t cb_fetch_journal(blk_t start, count_t width, void *data) {
	journal40_t *journal = (journal40_t *)data;

	/* Load journal header. */
	if (!(journal->header = aal_block_load(journal->device,
					       journal->blksize,
					       start)))
	{
		aal_error("Can't read journal header from block "
			  "%llu. %s.",
			  (unsigned long long)start,
			  journal->device->error);
		return -EIO;
	}
	
	/* Load journal footer. */
	if (!(journal->footer = aal_block_load(journal->device,
					       journal->blksize,
					       start + 1)))
	{
		aal_error("Can't read journal footer from block %llu. %s.",
			  (unsigned long long)(start + 1),
			  journal->device->error);

		aal_block_free(journal->header);
		return -EIO;
	}
    
	return 0;
}

/* Open journal on passed @format entity, @start and @blocks. Uses passed @desc
   for getting device journal is working on and fs block size. */
static reiser4_journal_ent_t *journal40_open(aal_device_t *device, 
					     uint32_t blksize, 
					     reiser4_format_ent_t *format, 
					     reiser4_oid_ent_t *oid, 
					     uint64_t start, uint64_t blocks)
{
	journal40_t *journal;

	aal_assert("umka-409", device != NULL);
	aal_assert("umka-1692", format != NULL);

	/* Initializign journal entity. */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->state = 0;
	journal->format = format;
	journal->oid = oid;
	journal->device = device;
	journal->plug = &journal40_plug;
	journal->blksize = blksize;

	journal->area.len = blocks;
	journal->area.start = start;

	/* Calling journal enumerator in order to fetch journal header and
	   footer. */
	if (journal40_layout((reiser4_journal_ent_t *)journal,
			     cb_fetch_journal, journal))
	{
		aal_error("Can't open journal header/footer.");
		goto error_free_journal;
	}

	return (reiser4_journal_ent_t *)journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Helper function for creating empty journal header and footer. Used in journal
   create time. */
static errno_t cb_alloc_journal(blk_t start, count_t width, void *data) {
	journal40_t *journal = (journal40_t *)data;
	
	if (!(journal->header = aal_block_alloc(journal->device,
						journal->blksize,
						start)))
	{
		aal_error("Can't alloc journal header on "
			  "block %llu.", (unsigned long long)start);
		return -ENOMEM;
	}

	if (!(journal->footer = aal_block_alloc(journal->device,
						journal->blksize,
						start + 1)))
	{
		aal_error("Can't alloc journal footer "
			  "on block %llu.", (unsigned long long)(start + 1));
		
		aal_block_free(journal->header);
		return -ENOMEM;
	}

	aal_block_fill(journal->header, 0);
	aal_block_fill(journal->footer, 0);
	
	return 0;
}

/* Create journal entity on passed params. Return create instance to caller. */
static reiser4_journal_ent_t *journal40_create(aal_device_t *device, 
					       uint32_t blksize, 
					       reiser4_format_ent_t *format,
					       reiser4_oid_ent_t *oid, 
					       uint64_t start, uint64_t blocks)
{
	journal40_t *journal;
    
	aal_assert("umka-1057", device != NULL);
	aal_assert("umka-1691", format != NULL);

	/* Initializing journal entity. Making it dirty. Setting up all
	   fields. */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->format = format;
	journal->oid = oid;
	journal->area.len = blocks;
	journal->area.start = start;
	journal->device = device;
	journal->plug = &journal40_plug;
	journal->blksize = blksize;
	journal40_mkdirty(journal);

	/* Create journal header and footer. */
	if (journal40_layout((reiser4_journal_ent_t *)journal,
			     cb_alloc_journal, journal))
	{
		aal_error("Can't create journal header/footer.");
		goto error_free_journal;
	}
    
	return (reiser4_journal_ent_t *)journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Helper function for save jopurnal header and footer to device journal is
   working on. */
static errno_t cb_sync_journal(blk_t start, count_t width, void *data) {
	journal40_t *journal = (journal40_t *)data;
	
	if (aal_block_write(journal->header)) {
		aal_error("Can't write journal header. %s.",
			  journal->device->error);
		return -EIO;
	}
	
	if (aal_block_write(journal->footer)) {
		aal_error("Can't write journal footer. %s.",
			  journal->device->error);
		return -EIO;
	}

	return 0;
}

/* Save journal metadata to device. */
static errno_t journal40_sync(reiser4_journal_ent_t *entity) {
	errno_t res;

	aal_assert("umka-410", entity != NULL);

	if ((res = journal40_layout(entity, cb_sync_journal, entity)))
		return res;
	
	journal40_mkclean(entity);
	return 0;
}

/* Update header/footer fields. Used from journal40_replay() and from fsck
   related stuff.  */
static errno_t journal40_update(journal40_t *journal) {
	errno_t res = 0;
	aal_device_t *device;
	aal_block_t *tx_block;
	journal40_footer_t *footer;
	journal40_header_t *header;
	
	journal40_tx_header_t *tx_header;
	uint64_t last_commited_tx, last_flushed_tx;

	aal_assert("vpf-450", journal != NULL);
	aal_assert("vpf-451", journal->footer != NULL);
	aal_assert("vpf-452", journal->footer->data != NULL);
	aal_assert("vpf-453", journal->header != NULL);
	aal_assert("vpf-504", journal->header->data != NULL);
	aal_assert("vpf-454", journal->device != NULL);

	footer = JFOOTER(journal->footer);
	header = JHEADER(journal->header);
	
	last_commited_tx = get_jh_last_commited(header);
	last_flushed_tx = get_jf_last_flushed(footer);

	if (last_flushed_tx == last_commited_tx)
		return 0;

	device = journal->device;

	if (!(tx_block = aal_block_load(device, journal->blksize,
					last_commited_tx)))
	{
		aal_error("Can't read block %llu while updating "
			  "the journal. %s.",
			  (unsigned long long)last_commited_tx,
			  device->error);
		return -EIO;
	}

	tx_header = (journal40_tx_header_t *)tx_block->data;
	
	if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
		aal_error("Invalid transaction header has been detected.");
		res = -EINVAL;
		goto error_free_tx_block;
	}
	
	/* Updating journal footer */
	set_jf_last_flushed(footer, last_commited_tx);
	set_jf_free_blocks(footer, get_th_free_blocks(tx_header));
	set_jf_used_oids(footer, get_th_used_oids(tx_header));
	set_jf_next_oid(footer, get_th_next_oid(tx_header));

	journal40_mkdirty(journal);

 error_free_tx_block:
	aal_block_free(tx_block);
	return res;
}

static errno_t journal40_update_format(journal40_t *journal) {
	journal40_footer_t *footer;
	
	aal_assert("vpf-1582", journal != NULL);
	aal_assert("vpf-1583", journal->format != NULL);
	aal_assert("vpf-1584", journal->footer != NULL);
	aal_assert("vpf-1585", journal->footer->data != NULL);

	footer = JFOOTER(journal->footer);
	
	/* If there is no valid info, return. */
	if (!get_jf_last_flushed(footer))
		return 0;

	/* Some transaction passed, update format accordingly to the 
	   footer info. */
	entcall(journal->format, set_free, get_jf_free_blocks(footer));
	entcall(journal->oid, set_next, get_jf_next_oid(footer));
	entcall(journal->oid, set_used, get_jf_used_oids(footer));

	return 0;
}

/* Traverses one journal transaction. This is used for transactions replaying,
   checking, etc. */
errno_t journal40_traverse_trans(
	reiser4_journal_ent_t *entity,          /* journal object to be traversed */
	aal_block_t *tx_block,                  /* trans header of a transaction */
	journal40_han_func_t han_func,          /* wandered/original pair callback */
	journal40_sec_func_t sec_func,          /* secondary blocks callback */
	void *data) 
{
	errno_t res;
	uint64_t log_blk;
	uint32_t i, capacity;
	aal_device_t *device;
	journal40_t *journal;

	journal40_lr_entry_t *entry;
	aal_block_t *log_block = NULL;
	aal_block_t *wan_block = NULL;	
	journal40_lr_header_t *lr_header;
	
	journal = (journal40_t *)entity;
	device = journal->device;
	log_blk = get_th_next_block((journal40_tx_header_t *)tx_block->data);
	
	while (log_blk != tx_block->nr) {
		
		/* FIXME-VITALY->UMKA: There should be a check that the log_blk
		   is not one of the LGR's of the same transaction. return 1. */
		if (sec_func && (res = sec_func(entity, tx_block, log_blk, 
						JB_LGR,	data)))
		{
			return res;
		}

		/* Loading log record block. */
		if (!(log_block = aal_block_load(device, journal->blksize,
						 log_blk)))
		{
			aal_error("Can't read block %llu while "
				  "traversing the journal. %s.", 
				  (unsigned long long)log_blk, device->error);
			return -EIO;
		}

		/* Checking it for validness, that is check magic, etc. */
		lr_header = (journal40_lr_header_t *)log_block->data;
		log_blk = get_lh_next_block(lr_header);

		if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
			aal_error("Invalid log record header has been detected.");
			res = -ESTRUCT;
			goto error_free_log_block;
		}

		entry = (journal40_lr_entry_t *)(lr_header + 1);
		capacity = (journal->blksize - sizeof(journal40_lr_header_t)) /
			sizeof(journal40_lr_entry_t);

		/* Loop trough the all wandered records. */
		for (i = 0; i < capacity; i++) {
			
			if (get_le_wandered(entry) == 0)
				break;

			if (sec_func) {
				if ((res = sec_func(entity, tx_block, 
						    get_le_wandered(entry), 
						    JB_WAN, data)))
				{
					goto error_free_log_block;
				}
		    
				if ((res = sec_func(entity, tx_block,
						    get_le_original(entry), 
						    JB_ORG, data)))
				{
					goto error_free_log_block;
				}
			}
	
			if (han_func) {
				if (!(wan_block = aal_block_load(device, journal->blksize,
								 get_le_wandered(entry))))
				{
					aal_error("Can't read block %llu while "
						  "traversing the journal. %s.",
						  (unsigned long long)get_le_wandered(entry),
						  device->error);
					res = -EIO;
					goto error_free_log_block;
				}

				if ((res = han_func(entity, wan_block, 
						    get_le_original(entry), data)))
				{
					goto error_free_wandered;
				}

				aal_block_free(wan_block);
			}

			entry++;
		}

		aal_block_free(log_block);
	}

	return 0;
	
 error_free_wandered:
	aal_block_free(wan_block);
 error_free_log_block:
	aal_block_free(log_block);
	return res;	
}

/* Journal traverse method. Finds the oldest transaction first, then goes
   through each transaction from the oldest to the earliest.
  
   Return codes:
   0   everything okay
   < 0 some error (-ESTRUCT, -EIO, etc). */
errno_t journal40_traverse(
	reiser4_journal_ent_t *entity,          /* journal object to be traversed */
	journal40_txh_func_t txh_func,          /* TxH block callback */
	journal40_han_func_t han_func,          /* wandered/original pair callback */
	journal40_sec_func_t sec_func,          /* secondary blocks callback */
	void *data)                             /* opaque data for traverse callbacks */ 
{
	errno_t res;
	uint64_t txh_blk;
	aal_device_t *device;
	uint64_t last_flushed_tx;
	uint64_t last_commited_tx;
	journal40_t *journal;

	aal_block_t *tx_block;
	aal_list_t *tx_list = NULL;

	journal40_header_t *jheader;
	journal40_footer_t *jfooter;
	journal40_tx_header_t *tx_header;
	
	journal = (journal40_t *)entity;
	aal_assert("vpf-448", journal != NULL);
	aal_assert("vpf-487", journal->header != NULL);
	aal_assert("vpf-488", journal->header->data != NULL);

	jheader = (journal40_header_t *)journal->header->data;
	jfooter = (journal40_footer_t *)journal->footer->data;
	
	last_commited_tx = get_jh_last_commited(jheader);
    	last_flushed_tx =  get_jf_last_flushed(jfooter);

	device = journal->device;
	txh_blk = last_commited_tx;

	while (txh_blk != last_flushed_tx) {
		
		/* FIXME-VITALY->UMKA: There should be a check that the txh_blk
		   is not one of the TxH's we have met already. return 1. */
		if (txh_func && (res = txh_func(entity, txh_blk, data)))
			goto error_free_tx_list;
	    
		if (!(tx_block = aal_block_load(device, journal->blksize,
						txh_blk)))
		{
			aal_error("Can't read block %llu while traversing "
				  "the journal. %s.",
				  (unsigned long long)txh_blk,
				  device->error);
			res = -EIO;
			goto error_free_tx_list;
		}
	
		tx_header = (journal40_tx_header_t *)tx_block->data;

		if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
			aal_error("Invalid transaction header has been detected.");
			res = -ESTRUCT;
			goto error_free_tx_list;
		}

		txh_blk = get_th_prev_tx(tx_header);
		tx_list = aal_list_prepend(tx_list, tx_block);
	}

	while (tx_list != NULL) {
		
		/* The oldest valid unreplayed transaction */
		tx_block = (aal_block_t *)tx_list->data;
		
		if ((res = journal40_traverse_trans(entity, tx_block,
						    han_func, sec_func,
						    data)))
		{
			goto error_free_tx_list;
		}
	
		tx_list = aal_list_remove(tx_list, tx_block);
		aal_block_free(tx_block);
	}
    
	return 0;

 error_free_tx_list:
	
	/* Close all from the list */
	while(tx_list != NULL) {
		tx_block = (aal_block_t *)aal_list_first(tx_list)->data;
	
		tx_list = aal_list_remove(tx_list, tx_block);
		aal_block_free(tx_block);
	}
    
	return res;
}

static errno_t cb_replay(reiser4_journal_ent_t *entity, 
			 aal_block_t *block,
			 d64_t orig, void *data)
{
	errno_t res;

	aal_block_move(block, PLUG_ENT(entity)->device, orig);
	    
	if ((res = aal_block_write(block))) {
		aal_error("Can't write block %llu.",
			  (unsigned long long)block->nr);
		aal_block_free(block);
	}

	return res;
}

typedef struct replay_count {
	uint64_t tx_count;
	uint64_t blk_count;
} replay_count_t;

static errno_t cb_print_replay(reiser4_journal_ent_t *entity,
			       aal_block_t *block, blk_t orig,
			       journal40_block_t type, void *data)
{
	journal40_tx_header_t *header;
	replay_count_t *count;
	
	header = (journal40_tx_header_t *)block->data;
	count = (replay_count_t *)data;
	
	if (type == JB_WAN)
		count->blk_count++;
		
	/* A print at every transaction. */
	if (type != JB_LGR)
		return 0;

	aal_mess("Replaying transaction: id %llu, block count %lu.",
		 (unsigned long long)header->th_id,
		 (long unsigned)header->th_total);

	count->tx_count++;
	
	return 0;
}

/* Makes journal replay */
static errno_t journal40_replay(reiser4_journal_ent_t *entity) {
	replay_count_t count;
	errno_t res;

	aal_assert("umka-412", entity != NULL);

	
	aal_memset(&count, 0, sizeof(count));
	
	/* Traverse the journal and replay all transactions. */
	if ((res = journal40_traverse(entity, NULL, cb_replay,
				      cb_print_replay, &count)))
	{
		return res;
	}

	/* Update the format according to the footer's values. */
	if ((res = journal40_update_format(PLUG_ENT(entity))))
		return res;
	
	/* Update the journal. */
	if ((res = journal40_update(PLUG_ENT(entity))))
		return res;

	if (count.tx_count) {
		aal_mess("Reiser4 journal (%s) on %s: %llu transactions "
			 "replayed of the total %llu blocks.", 
			 journal40_plug.p.label, 
			 PLUG_ENT(entity)->device->name, 
			 (unsigned long long)count.tx_count,
			 (unsigned long long)count.blk_count);
	}

	/* Invalidate the journal. */
	journal40_invalidate(entity);
	return 0;
}

/* Releases the journal */
static void journal40_close(reiser4_journal_ent_t *entity) {
	aal_assert("umka-411", entity != NULL);

	aal_block_free(PLUG_ENT(entity)->header);
	aal_block_free(PLUG_ENT(entity)->footer);
	aal_free(entity);
}

reiser4_journal_plug_t journal40_plug = {
	.p = {
		.id    = {JOURNAL_REISER40_ID, 0, JOURNAL_PLUG_TYPE},
		.label = "journal40",
		.desc  = "Journal plugin.",
	},
	
	.open	  	= journal40_open,
	.create	  	= journal40_create,
	.sync	  	= journal40_sync,
	.replay   	= journal40_replay,
	.print    	= journal40_print,
	.layout   	= journal40_layout,
	.valid	  	= journal40_valid,
	.close	  	= journal40_close,
	.device   	= journal40_device,
	.set_state  	= journal40_set_state,
	.get_state  	= journal40_get_state,
	.check_struct	= journal40_check_struct,
	.invalidate	= journal40_invalidate,
	.pack		= journal40_pack,
	.unpack		= journal40_unpack,
};

#endif
