/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40.c -- reiser4 journal plugin. */

#ifndef ENABLE_STAND_ALONE
#include "journal40.h"
#include "journal40_repair.h"

extern reiser4_plug_t journal40_plug;

/* Functions for making journal dirty, clean, etc. */
static int journal40_isdirty(generic_entity_t *entity) {
	aal_assert("umka-2081", entity != NULL);
	return ((journal40_t *)entity)->dirty;
}

void journal40_mkdirty(generic_entity_t *entity) {
	aal_assert("umka-2082", entity != NULL);
	((journal40_t *)entity)->dirty = 1;
}

static void journal40_mkclean(generic_entity_t *entity) {
	aal_assert("umka-2083", entity != NULL);
	((journal40_t *)entity)->dirty = 0;
}

static errno_t journal40_valid(generic_entity_t *entity) {
	aal_assert("umka-965", entity != NULL);
	return 0;
}

/* Journal enumerator function. */
static errno_t journal40_layout(generic_entity_t *entity,
				region_func_t region_func,
				void *data)
{
	blk_t blk;
	journal40_t *journal;

	aal_assert("umka-1040", entity != NULL);
	aal_assert("umka-1041", region_func != NULL);
    
	journal = (journal40_t *)entity;
	
	blk = JOURNAL40_BLOCKNR(journal->blksize);
	return region_func(entity, blk, 2, data);
}

aal_device_t *journal40_device(generic_entity_t *entity) {
	aal_assert("vpf-455", entity != NULL);
	return ((journal40_t *)entity)->device;
}

/* Helper function for fetching journal footer and header in initialization
   time. */
static errno_t callback_fetch_journal(void *entity, blk_t start,
				      count_t width, void *data)
{
	journal40_t *journal = (journal40_t *)entity;

	/* Load journal header. */
	if (!(journal->header = aal_block_load(journal->device,
					       journal->blksize,
					       start)))
	{
		aal_exception_error("Can't read journal header from "
				    "block %llu. %s.", start,
				    journal->device->error);
		return -EIO;
	}
	
	/* Load journal footer. */
	if (!(journal->footer = aal_block_load(journal->device,
					       journal->blksize,
					       start + 1)))
	{
		aal_exception_error("Can't read journal footer from "
				    "block %llu. %s.",
				    start + 1, journal->device->error);
		return -EIO;
	}
    
	return 0;
}

/* Open journal on passed @format entity, @start and @blocks. Uses passed @desc
   for getting device journal is working on and fs block size. */
static generic_entity_t *journal40_open(fs_desc_t *desc, generic_entity_t *format,
					uint64_t start, uint64_t blocks)
{
	journal40_t *journal;

	aal_assert("umka-409", desc != NULL);
	aal_assert("umka-1692", format != NULL);

	/* Initializign journal entity. */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->dirty = 0;
	journal->format = format;
	journal->device = desc->device;
	journal->plug = &journal40_plug;
	journal->blksize = desc->blksize;

	journal->area.len = blocks;
	journal->area.start = start;

	/* Calling journal enumerator in order to fetch journal header and
	   footer. */
	if (journal40_layout((generic_entity_t *)journal,
			     callback_fetch_journal, journal))
	{
		aal_exception_error("Can't open journal "
				    "header/footer.");
		goto error_free_journal;
	}

	return (generic_entity_t *)journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Helper function for creating empty journal header and footer. Used in journal
   create time. */
static errno_t callback_alloc_journal(void *entity, blk_t start,
				      count_t width, void *data)
{
	journal40_t *journal = (journal40_t *)entity;
	
	if (!(journal->header = aal_block_alloc(journal->device,
						journal->blksize,
						start)))
	{
		aal_exception_error("Can't alloc journal header "
				    "on block %llu.", start);
		return -ENOMEM;
	}

	if (!(journal->footer = aal_block_alloc(journal->device,
						journal->blksize,
						start + 1)))
	{
		aal_exception_error("Can't alloc journal footer "
				    "on block %llu.", start + 1);
		return -ENOMEM;
	}

	return 0;
}

/* Create journal entity on passed params. Return create instance to caller. */
static generic_entity_t *journal40_create(fs_desc_t *desc, generic_entity_t *format,
					  uint64_t start, uint64_t blocks)
{
	journal40_t *journal;
    
	aal_assert("umka-1057", desc != NULL);
	aal_assert("umka-1691", format != NULL);

	/* Initializing journal entity. Making it dirty. Setting up all
	   fields. */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->dirty = 1;
	journal->format = format;
	journal->device = desc->device;
	journal->plug = &journal40_plug;
	journal->blksize = desc->blksize;

	journal->area.len = blocks;
	journal->area.start = start;

	/* Create journal header and footer. */
	if (journal40_layout((generic_entity_t *)journal,
			     callback_alloc_journal, journal))
	{
		aal_exception_error("Can't create journal "
				    "header/footer.");
		goto error_free_journal;
	}
    
	return (generic_entity_t *)journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Helper function for save jopurnal header and footer to device journal is
   working on. */
static errno_t callback_sync_journal(void *entity, blk_t start,
				     count_t width, void *data)
{
	journal40_t *journal = (journal40_t *)entity;
	
	if (aal_block_write(journal->header)) {
		aal_exception_error("Can't write journal header. "
				    "%s.", journal->device->error);
		return -EIO;
	}
	
	if (aal_block_write(journal->footer)) {
		aal_exception_error("Can't write journal footer. "
				    "%s.", journal->device->error);
		return -EIO;
	}

	return 0;
}

/* Save journal metadata to device. */
static errno_t journal40_sync(generic_entity_t *entity) {
	errno_t res;

	aal_assert("umka-410", entity != NULL);

	if ((res = journal40_layout(entity, callback_sync_journal, NULL)))
		return res;
	
	((journal40_t *)entity)->dirty = 0;
	
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
		aal_exception_error("Can't read block %llu while updating "
				    "the journal. %s.", last_commited_tx,
				    device->error);
		return -EIO;
	}

	tx_header = (journal40_tx_header_t *)tx_block->data;
	
	if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
		aal_exception_error("Invalid transaction header has "
				    "been detected.");
		res = -EINVAL;
		goto error_free_tx_block;
	}
	
	/* Updating journal footer */
	set_jf_last_flushed(footer, last_commited_tx);
	set_jf_free_blocks(footer, get_th_free_blocks(tx_header));
	set_jf_used_oids(footer, get_th_used_oids(tx_header));
	set_jf_next_oid(footer, get_th_next_oid(tx_header));

	journal->dirty = 1;

 error_free_tx_block:
	aal_block_free(tx_block);
	return res;
}

/* Traverses one journal transaction. This is used for transactions replaying,
   checking, etc. */
errno_t journal40_traverse_trans(
	journal40_t *journal,                   /* journal object to be traversed */
	aal_block_t *tx_block,                  /* trans header of a transaction */
	journal40_han_func_t han_func,          /* wandered/original pair callback */
	journal40_sec_func_t sec_func,          /* secondary blocks callback */
	void *data) 
{
	errno_t res;
	uint64_t log_blk;
	uint32_t i, capacity;
	aal_device_t *device;

	journal40_lr_entry_t *entry;
	aal_block_t *log_block = NULL;
	aal_block_t *wan_block = NULL;	
	journal40_lr_header_t *lr_header;

	device = journal->device;
	log_blk = get_th_next_block((journal40_tx_header_t *)tx_block->data);
	
	while (log_blk != tx_block->nr) {
		
		/* FIXME-VITALY->UMKA: There should be a check that the log_blk
		   is not one of the LGR's of the same transaction. return 1. */
		if (sec_func && (res = sec_func((generic_entity_t *)journal, 
						tx_block, log_blk, JB_LGR,
						data)))
		{
			return res;
		}

		/* Loading log record block. */
		if (!(log_block = aal_block_load(device, journal->blksize,
						 log_blk)))
		{
			aal_exception_error("Can't read block %llu while "
					    "traversing the journal. %s.", 
					    log_blk, device->error);
			return -EIO;
		}

		/* Checking it for validness, that is check magic, etc. */
		lr_header = (journal40_lr_header_t *)log_block->data;
		log_blk = get_lh_next_block(lr_header);

		if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
			aal_exception_error("Invalid log record header has been"
					    " detected.");
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
				if ((res = sec_func((generic_entity_t *)journal, tx_block, 
						    get_le_wandered(entry), JB_WAN, data)))
				{
					goto error_free_log_block;
				}
		    
				if ((res = sec_func((generic_entity_t *)journal, tx_block,
						    get_le_original(entry), JB_ORG, data)))
				{
					goto error_free_log_block;
				}
			}
	
			if (han_func) {
				if (!(wan_block = aal_block_load(device, journal->blksize,
								 get_le_wandered(entry))))
				{
					aal_exception_error("Can't read block %llu while "
							    "traversing the journal. %s.",
							    get_le_wandered(entry), 
							    device->error);
					res = -EIO;
					goto error_free_log_block;
				}

				if ((res = han_func((generic_entity_t *)journal, wan_block, 
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
	journal40_t *journal,                   /* journal object to be traversed */
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

	aal_block_t *tx_block;
	aal_list_t *tx_list = NULL;

	journal40_header_t *jheader;
	journal40_footer_t *jfooter;
	journal40_tx_header_t *tx_header;

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
		if (txh_func && (res = txh_func((generic_entity_t *)journal, 
						txh_blk, data)))
			goto error_free_tx_list;
	    
		if (!(tx_block = aal_block_load(device, journal->blksize,
						txh_blk)))
		{
			aal_exception_error("Can't read block %llu while "
					    "traversing the journal. %s.",
					    txh_blk, device->error);
			res = -EIO;
			goto error_free_tx_list;
		}
	
		tx_header = (journal40_tx_header_t *)tx_block->data;

		if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
			aal_exception_error("Invalid transaction header "
					    "has been detected.");
			res = -ESTRUCT;
			goto error_free_tx_list;
		}

		txh_blk = get_th_prev_tx(tx_header);
		tx_list = aal_list_prepend(tx_list, tx_block);
	}

	while (tx_list != NULL) {
		
		/* The oldest valid unreplayed transction */
		tx_block = (aal_block_t *)tx_list->data;
		
		if ((res = journal40_traverse_trans(journal, tx_block,
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

static errno_t callback_replay_handler(generic_entity_t *entity,
				       aal_block_t *block,
				       d64_t orig, void *data) 
{
	errno_t res;
	journal40_t *journal;

	journal = (journal40_t *)entity;
	
	aal_block_move(block, journal->device, orig);
	    
	if ((res = aal_block_write(block))) {
		aal_exception_error("Can't write block %llu.", 
				    block->nr);
		
		aal_block_free(block);
	}

	return res;
}

/* Makes journal replay */
static errno_t journal40_replay(generic_entity_t *entity) {
	errno_t res;

	aal_assert("umka-412", entity != NULL);

	if ((res = journal40_traverse((journal40_t *)entity, NULL,
				      callback_replay_handler,
				      NULL, NULL)))
	{
		return res;
	}

	return journal40_update((journal40_t *)entity);
}

/* Safely extracts string field from passed location */
static void extract_string(char *stor, char *orig, uint32_t max) {
	uint32_t i;
	
	for (i = 0; i < max; i++) {
		if (orig[i] == '\0')
			break;
	}

	aal_memcpy(stor, orig, i);
}

/* Helper function for printing transaction header. */
static errno_t callback_print_txh(generic_entity_t *entity,
				  blk_t blk, void *data)
{
	aal_block_t *block;
	journal40_t *journal;
	aal_stream_t *stream;

	char magic[TXH_MAGIC_SIZE];
	journal40_tx_header_t *txh;

	if (blk == INVAL_BLK)
		return -EINVAL;
	
	stream = (aal_stream_t *)data;
	journal = (journal40_t *)entity;

	if (!(block = aal_block_load(journal->device,
				     journal->blksize,
				     blk)))
	{
		return -EIO;
	}
	
	txh = (journal40_tx_header_t *)block->data;

	aal_stream_format(stream, "Transaction header:\n");

	aal_memset(magic, 0, sizeof(magic));
	extract_string(magic, txh->magic, sizeof(magic));
	
	aal_stream_format(stream, "magic:\t%s\n", magic);

	aal_stream_format(stream, "id: \t0x%llx\n",
			  get_th_id(txh));
	
	aal_stream_format(stream, "total:\t%lu\n",
			  get_th_total(txh));
	
	aal_stream_format(stream, "prev:\t%llu\n",
			  get_th_prev_tx(txh));
	
	aal_stream_format(stream, "next block:\t%llu\n",
			  get_th_next_block(txh));
	
	aal_stream_format(stream, "free blocks:\t%llu\n",
			  get_th_free_blocks(txh));
	
	aal_stream_format(stream, "used oids:\t%llu\n",
			  get_th_used_oids(txh));
	
	aal_stream_format(stream, "next oid:\t0x%llx\n\n",
			  get_th_next_oid(txh));
	
	aal_block_free(block);

	return 0;
}

/* Printing pair (wandered and original) blocks */
static errno_t callback_print_par(generic_entity_t *entity,
				  aal_block_t *block,
				  blk_t orig, void *data)
{
	aal_stream_format((aal_stream_t *)data,
			  "%llu -> %llu\n",
			  orig, block->nr);
	return 0;
}

/* hellper function for printing log record. */
static errno_t callback_print_lgr(generic_entity_t *entity,
				  aal_block_t *block, blk_t blk,
				  journal40_block_t bel, void *data)
{
	aal_stream_t *stream;
	char magic[LGR_MAGIC_SIZE];
	journal40_lr_header_t *lgr;
	
	if (bel != JB_LGR)
		return 0;

	stream = (aal_stream_t *)data;
	lgr = (journal40_lr_header_t *)block->data;
	
	aal_stream_format(stream, "Log record:\n");

	aal_memset(magic, 0, sizeof(magic));
	extract_string(magic, lgr->magic, sizeof(magic));
	
	aal_stream_format(stream, "magic:\t%s\n",
			  magic);
	
	aal_stream_format(stream, "id: \t0x%llx\n",
			  get_lh_id(lgr));
	
	aal_stream_format(stream, "total:\t%lu\n",
			  get_lh_total(lgr));
	
	aal_stream_format(stream, "serial:\t0x%lx\n",
			  get_lh_serial(lgr));
	
	aal_stream_format(stream, "next block:\t%llu\n\n",
			  get_lh_next_block(lgr));
	
	return 0;
}

/* Prints journal structures into passed @stream */
static errno_t journal40_print(generic_entity_t *entity,
			       aal_stream_t *stream, 
			       uint16_t options)
{
	journal40_t *journal;
	journal40_footer_t *footer;
	journal40_header_t *header;
	
	aal_assert("umka-1465", entity != NULL);
	aal_assert("umka-1466", stream != NULL);

	journal = (journal40_t *)entity;
		
	/* Printing journal header and journal footer first */
	header = JHEADER(journal->header);
	footer = JFOOTER(journal->footer);

	aal_stream_format(stream, "Journal:\n");
	
	aal_stream_format(stream, "plugin: \t%s\n",
			  entity->plug->label);

	aal_stream_format(stream, "description:\t%s\n\n",
			  entity->plug->desc);
	
	aal_stream_format(stream, "Journal header:\n");

	aal_stream_format(stream, "last commited:\t%llu\n\n",
			  get_jh_last_commited(header));
		
	aal_stream_format(stream, "Journal footer:\n");

	aal_stream_format(stream, "last flushed:\t%llu\n",
			  get_jf_last_flushed(footer));
	
	aal_stream_format(stream, "free blocks:\t%llu\n",
			  get_jf_free_blocks(footer));
	
	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  get_jf_next_oid(footer));
	
	aal_stream_format(stream, "used oids:\t%llu\n",
			  get_jf_used_oids(footer));

	/* Print all transactions */
	return journal40_traverse((journal40_t *)entity,
				  callback_print_txh,
				  callback_print_par,
				  callback_print_lgr,
				  (void *)stream);
}

/* Releases the journal */
static void journal40_close(generic_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;
    
	aal_assert("umka-411", entity != NULL);

	aal_block_free(journal->header);
	aal_block_free(journal->footer);
	aal_free(journal);
}

static reiser4_journal_ops_t journal40_ops = {
	.open	  	= journal40_open,
	.create	  	= journal40_create,
	.sync	  	= journal40_sync,
	.isdirty  	= journal40_isdirty,
	.mkdirty  	= journal40_mkdirty,
	.mkclean  	= journal40_mkclean,
	.replay   	= journal40_replay,
	.print    	= journal40_print,
	.check_struct	= journal40_check_struct,
	.layout   	= journal40_layout,
	.valid	  	= journal40_valid,
	.close	  	= journal40_close,
	.device   	= journal40_device
};

static reiser4_plug_t journal40_plug = {
	.cl    = CLASS_INIT,
	.id    = {JOURNAL_REISER40_ID, 0, JOURNAL_PLUG_TYPE},
	.label = "journal40",
	.desc  = "Journal for reiser4, ver. " VERSION,
	.o = {
		.journal_ops = &journal40_ops
	}
};

static reiser4_plug_t *journal40_start(reiser4_core_t *c) {
	return &journal40_plug;
}

plug_register(journal40, journal40_start, NULL);
#endif
