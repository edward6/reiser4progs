/*
  journal40.c -- reiser4 default journal plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "journal40.h"

#define JOURNAL40_HEADER (4096 * 19)
#define JOURNAL40_FOOTER (4096 * 20)

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t journal40_plugin;

static errno_t journal40_layout(object_entity_t *entity,
				block_func_t func,
				void *data)
{
	blk_t blk;
	journal40_t *journal;

	aal_assert("umka-1040", entity != NULL);
	aal_assert("umka-1041", func != NULL);
    
	journal = (journal40_t *)entity;
	
	blk = JOURNAL40_HEADER / aal_device_get_bs(journal->device);
    
	if (func(entity, blk, data))
		return -1;
    
	blk = JOURNAL40_FOOTER / aal_device_get_bs(journal->device);
    
	if (func(entity, blk, data))
		return -1;

	return 0;
}

static errno_t journal40_hcheck(journal40_header_t *header) {
	aal_assert("umka-515", header != NULL);
	return 0;
}

static errno_t journal40_fcheck(journal40_footer_t *footer) {
	aal_assert("umka-516", footer != NULL);
	return 0;
}

aal_device_t *journal40_device(object_entity_t *entity) {
	aal_assert("vpf-455", entity != NULL);
	return ((journal40_t *)entity)->device;
}

static errno_t callback_fetch_journal(object_entity_t *entity, 
				      blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal;

	journal = (journal40_t *)entity;
	device = journal->device;
		
	if (!journal->header) {
		if (!(journal->header = aal_block_open(device, blk))) {
			aal_exception_error("Can't read journal header from "
					    "block %llu. %s.", blk, device->error);
			return -1;
		}
	} else {
		if (!(journal->footer = aal_block_open(device, blk))) {
			aal_exception_error("Can't read journal footer from "
					    "block %llu. %s.", blk, device->error);
			return -1;
		}
	}
    
	return 0;
}

static object_entity_t *journal40_open(object_entity_t *format,
				       aal_device_t *device,
				       uint64_t start, uint64_t len)
{
	journal40_t *journal;

	aal_assert("umka-409", device != NULL);
	aal_assert("umka-1692", format != NULL);
    
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->device = device;
	journal->format = format;
	journal->plugin = &journal40_plugin;

	journal->area.start = start;
	journal->area.len = len;

	if (journal40_layout((object_entity_t *)journal,
			     callback_fetch_journal, journal))
	{
		aal_exception_error("Can't load journal metadata.");
		goto error_free_journal;
	}
    
	return (object_entity_t *)journal;

 error_free_journal:
	aal_free(journal);
 error:
	return NULL;
}

static errno_t journal40_valid(object_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;
    
	aal_assert("umka-965", journal != NULL);
    
	if (journal40_hcheck(journal->header->data))
		return -1;
	
	if (journal40_fcheck(journal->footer->data))
		return -1;
    
	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t callback_alloc_journal(object_entity_t *entity,
				      blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal = (journal40_t *)entity;

	device = journal->device;
	
	if (!journal->header) {
		if (!(journal->header = aal_block_create(device, blk, 0))) {
			aal_exception_error("Can't alloc journal "
					    "header on block %llu.", blk);
			return -1;
		}
	} else {
		if (!(journal->footer = aal_block_create(device, blk, 0))) {
			aal_exception_error("Can't alloc journal footer "
					    "on block %llu.", blk);
			return -1;
		}
	}
    
	return 0;
}

static object_entity_t *journal40_create(object_entity_t *format,
					 aal_device_t *device,
					 uint64_t start, uint64_t len,
					 void *hint)
{
	journal40_t *journal;
    
	aal_assert("umka-1057", device != NULL);
	aal_assert("umka-1691", format != NULL);
    
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;
    
	journal->device = device;
	journal->format = format;
	journal->plugin = &journal40_plugin;

	journal->area.start = start;
	journal->area.len = len;
    
	if (journal40_layout((object_entity_t *)journal,
			     callback_alloc_journal, journal))
	{
		aal_exception_error("Can't load journal metadata.");
		goto error_free_journal;
	}
    
	return (object_entity_t *)journal;

 error_free_journal:
	aal_free(journal);
 error:
	return NULL;
}

static errno_t callback_sync_journal(object_entity_t *entity,
				     blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal = (journal40_t *)entity;

	device = journal->device;

	if (blk == aal_block_number(journal->header)) {
		if (aal_block_sync(journal->header)) {
			aal_exception_error("Can't write journal header to block "
					    "%llu. %s.", blk, device->error);
			return -1;
		}
	} else {
		if (aal_block_sync(journal->footer)) {
			aal_exception_error("Can't write journal footer to block "
					    "%llu. %s.", blk, device->error);
			return -1;
		}
	}
    
	return 0;
}

static errno_t journal40_sync(object_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;

	aal_assert("umka-410", journal != NULL);
    
	if (journal40_layout(entity, callback_sync_journal, journal)) {
		aal_exception_error("Can't load journal metadata.");
		return -1;
	}
    
	return 0;
}

static errno_t callback_journal_handler(object_entity_t *entity,
					aal_block_t *block,
					d64_t original, void *data) 
{
	aal_block_relocate(block, original);
	    
	if (aal_block_sync(block)) {
		
		aal_exception_error("Can't write block %llu.", 
				    aal_block_number(block));
		
		aal_block_close(block);
		return -1;
	}

	return 0;
}

static errno_t journal40_update(journal40_t *journal) {
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

	footer = (journal40_footer_t *)journal->footer->data;	
	header = (journal40_header_t *)journal->header->data;
	
	last_commited_tx = get_jh_last_commited(header);
	last_flushed_tx = get_jf_last_flushed(footer);

	if (last_flushed_tx == last_commited_tx)
		return 0;

	device = journal->device;

	if (!(tx_block = aal_block_open(device, last_commited_tx))) {
		aal_exception_error("Can't read block %llu while updating "
				    "the journal. %s.", last_commited_tx, device->error);
		return -1;
	}
	
	tx_header = (journal40_tx_header_t *)tx_block->data;
	
	if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
		aal_exception_error("Invalid transaction header has "
				    "been detected.");
		return -1;
	}
	
	/* Updating journal footer */
	set_jf_last_flushed(footer, last_commited_tx);
	set_jf_free_blocks(footer, get_th_free_blocks(tx_header));
	
	set_jf_nr_files(footer, get_th_nr_files(tx_header));
	set_jf_next_oid(footer, get_th_next_oid(tx_header));

	return 0;
}

errno_t journal40_traverse_trans(
	journal40_t *journal,			/* journal object to be traversed */
	aal_block_t *tx_block,			/* trans header of a transaction */
	journal40_handler_func_t handler_func,	/* wandered/original pair callback */
	journal40_sec_func_t sec_func,		/* secondary blocks callback */
	void *data) 
{
	errno_t ret;
	uint64_t log_blk;
	uint32_t i, capacity;
	aal_device_t *device;

	journal40_lr_entry_t *entry;
	aal_block_t *log_block = NULL;
	aal_block_t *wan_block = NULL;	
	journal40_lr_header_t *lr_header;

	device = journal->device;
	log_blk = get_th_next_block((journal40_tx_header_t *)tx_block->data);
	
	while (log_blk != aal_block_number(tx_block)) {
		
		/*
		  FIXME-VITALY->UMKA: There should be a check that the log_blk
		  is not one of the LGR's of the same transaction. return 1.
		*/
	    
		if (sec_func && (ret = sec_func((object_entity_t *)journal, 
						tx_block, log_blk, LGR, data)))
			goto error;
	    
		if (!(log_block = aal_block_open(device, log_blk))) {
			aal_exception_error("Can't read block %llu while "
					    "traversing the journal. %s.", 
					    log_blk, device->error);
			ret = -1;
			goto error;
		}

		lr_header = (journal40_lr_header_t *)log_block->data;
		log_blk = get_lh_next_block(lr_header);

		if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
			aal_exception_error("Invalid log record header has been"
					    " detected.");
			ret = 1;
			goto error;
		}

		entry = (journal40_lr_entry_t *)(lr_header + 1);
		
		capacity = (device->blocksize - sizeof(journal40_lr_header_t)) /
			sizeof(journal40_lr_entry_t);

		for (i = 0; i < capacity; i++) {
			if (get_le_wandered(entry) == 0)
				break;

			if (sec_func) {
				if ((ret = sec_func((object_entity_t *)journal, tx_block, 
						    get_le_wandered(entry), WAN, data)))
					goto error_free_log_block;
		    
				if ((ret = sec_func((object_entity_t *)journal, tx_block,
						    get_le_original(entry), ORG, data)))
					goto error_free_log_block;
			}
	
			if (handler_func) {
				wan_block = aal_block_open(device, get_le_wandered(entry));
		
				if (!wan_block) {
					aal_exception_error("Can't read block %llu while "
							    "traversing the journal. %s.",
							    get_le_wandered(entry), 
							    device->error);
					ret = -1;
					goto error_free_log_block;
				}

				if ((ret = handler_func((object_entity_t *)journal, wan_block, 
							get_le_original(entry), data)))
					goto error_free_wandered;

				aal_block_close(wan_block);
			}

			entry++;
		}

		aal_block_close(log_block);
	}

	return 0;
	
 error_free_wandered:
	aal_block_close(wan_block);
 error_free_log_block:
	aal_block_close(log_block);
 error:
	return ret;	
}

/*
  Journal traverse method. Finds the oldest transaction first, then goes through
  each transaction from the oldest to the earliest.
  
  Returns:
  0:  everything okay
  1:  for bad journal structure; 
  -1: fatal error
*/
errno_t journal40_traverse(
	journal40_t *journal,			/* journal object to be traversed */
	journal40_handler_func_t handler_func,	/* wandered/original pair callback */
	journal40_txh_func_t txh_func,		/* TxH block callback */
	journal40_sec_func_t sec_func,		/* secondary blocks callback */
	void *data)				/* opaque data for traverse callbacks. */ 
{
	errno_t ret;
	uint64_t txh_blk;
	aal_device_t *device;
	uint64_t last_flushed_tx;
	uint64_t last_commited_tx;

	aal_block_t *tx_block;
	aal_list_t *tx_list = NULL;
	journal40_tx_header_t *tx_header;

	journal40_header_t *jheader;
	journal40_footer_t *jfooter;
    
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
		
		/*
		  FIXME-VITALY->UMKA: There should be a check that the txh_blk
		  is not one of the TxH's we have met already. return 1.
		*/
	    
		if (txh_func && (ret = txh_func((object_entity_t *)journal, 
						txh_blk, data)))
			goto error_free_tx_list;
	    
		if (!(tx_block = aal_block_open(device, txh_blk))) {
			aal_exception_error("Can't read block %llu while traversing "
					    "the journal. %s.", txh_blk, device->error);
			ret = -1;
			goto error_free_tx_list;
		}
	
		tx_header = (journal40_tx_header_t *)tx_block->data;

		if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
			aal_exception_error("Invalid transaction header has been detected.");
			ret = 1;
			goto error_free_tx_list;
		}

		txh_blk = get_th_prev_tx(tx_header);
		tx_list = aal_list_append(tx_list, tx_block);
	}

	while (tx_list != NULL) {
		
		/* The last valid unreplayed transction */
		tx_block = (aal_block_t *)aal_list_last(tx_list)->data;
		
		if ((ret = journal40_traverse_trans(journal, tx_block, handler_func, 
						    sec_func, data)))
			goto error_free_tx_list;
	
		tx_list = aal_list_remove(tx_list, tx_block);
		aal_block_close(tx_block);
	}
    
	return 0;

 error_free_tx_list:
	
	/* Close all from the list */;
	while(tx_list != NULL) {
		tx_block = (aal_block_t *)aal_list_first(tx_list)->data;
	
		tx_list = aal_list_remove(tx_list, tx_block);
		aal_block_close(tx_block);
	}
    
	return ret;
}

static errno_t journal40_replay(object_entity_t *entity) {
	int trans_nr = 0;
    
	aal_assert("umka-412", entity != NULL);

	if (journal40_traverse((journal40_t *)entity,
			       callback_journal_handler, 
			       NULL, NULL, NULL))
		return -1;

	/* FIXME-UMKA: Super block has been left not updated. */
	journal40_update((journal40_t *)entity);

	return 0;
}

static errno_t journal40_print(object_entity_t *entity,
			       aal_stream_t *stream, 
			       uint16_t options)
{
	aal_assert("umka-1465", entity != NULL);
	aal_assert("umka-1466", stream != NULL);

	return 0;
}

#endif

static void journal40_close(object_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;
    
	aal_assert("umka-411", entity != NULL);

	aal_block_close(journal->header);
	aal_block_close(journal->footer);
	aal_free(journal);
}

extern errno_t journal40_check(object_entity_t *, layout_func_t, void *);

static reiser4_plugin_t journal40_plugin = {
	.journal_ops = {
		.h = {
			.handle = empty_handle,
			.id = JOURNAL_REISER40_ID,
			.group = 0,
			.type = JOURNAL_PLUGIN_TYPE,
			.label = "journal40",
			.desc = "Default journal for reiserfs 4.0, ver. " VERSION,
		},
		.open	= journal40_open,
		
#ifndef ENABLE_COMPACT
		.create	= journal40_create,
		.sync	= journal40_sync,
		.replay = journal40_replay,
		.print  = journal40_print,
		.check  = journal40_check,
#else
		.create = NULL,
		.sync	= NULL,
		.replay = NULL,
		.print  = NULL,
		.check  = NULL,
#endif
		.layout = journal40_layout,
		.valid	= journal40_valid,
		.close	= journal40_close,
		.device = journal40_device
	}
};

static reiser4_plugin_t *journal40_start(reiser4_core_t *c) {
	core = c;
	return &journal40_plugin;
}

plugin_register(journal40_start, NULL);

