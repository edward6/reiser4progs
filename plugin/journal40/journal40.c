/*
  journal40.c -- reiser4 default journal plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "journal40.h"

extern reiser4_plugin_t journal40_plugin;

typedef errno_t (*journal40_layout_func_t)(journal40_t *journal,
					   aal_block_t *block, d64_t original);

typedef errno_t (*journal40_update_func_t)(journal40_t *journal,
					   aal_block_t *block);

static reiser4_core_t *core = NULL;

static errno_t journal40_hcheck(journal40_header_t *header) {
	aal_assert("umka-515", header != NULL, return -1);
	return 0;
}

static errno_t journal40_fcheck(journal40_footer_t *footer) {
	aal_assert("umka-516", footer != NULL, return -1);
	return 0;
}

static errno_t callback_fetch_journal(object_entity_t *format, 
				      blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal = (journal40_t *)data;

	device = plugin_call(return -1, format->plugin->format_ops, 
			     device, format);
    
	if (!device) {
		aal_exception_error("Invalid device has been detected.");
		return -1;
	}

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

static aal_device_t *journal40_device(object_entity_t *entity) {
	return ((journal40_t *)entity)->device;
}

static object_entity_t *journal40_open(object_entity_t *format) {
	journal40_t *journal;
	reiser4_layout_func_t layout;

	aal_assert("umka-409", format != NULL, return NULL);
    
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->format = format;
	journal->plugin = &journal40_plugin;
    
	if (!(layout = format->plugin->format_ops.journal_layout)) {
		aal_exception_error("Method \"journal_layout\" doesn't implemented "
				    "in format plugin.");
		goto error_free_journal;
	}
    
	if (layout(format, callback_fetch_journal, journal)) {
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
    
	aal_assert("umka-965", journal != NULL, return -1);
    
	if (journal40_hcheck(journal->header->data))
		return -1;
	
	if (journal40_fcheck(journal->footer->data))
		return -1;
    
	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t callback_alloc_journal(object_entity_t *format,
				      blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal = (journal40_t *)data;

	device = plugin_call(return -1, format->plugin->format_ops, 
			     device, format);
    
	if (!device) {
		aal_exception_error("Invalid device has been detected.");
		return -1;
	}
    
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
					  void *params) 
{
	journal40_t *journal;
	reiser4_layout_func_t layout;
    
	aal_assert("umka-1057", format != NULL, return NULL);
    
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;
    
	journal->format = format;
	journal->plugin = &journal40_plugin;
    
	if (!(layout = format->plugin->format_ops.journal_layout)) {
		aal_exception_error("Method \"journal_layout\" doesn't "
				    "implemented in format plugin.");
		goto error_free_journal;
	}
    
	if (layout(format, callback_alloc_journal, journal)) {
		aal_exception_error("Can't load journal metadata.");
		goto error_free_journal;
	}
    
	return (object_entity_t *)journal;

 error_free_header:
	aal_block_close(journal->header);
 error_free_journal:
	aal_free(journal);
 error:
	return NULL;
}

static errno_t callback_sync_journal(object_entity_t *format,
				      blk_t blk, void *data)
{
	aal_device_t *device;
	journal40_t *journal = (journal40_t *)data;

	device = plugin_call(return -1, format->plugin->format_ops, 
			     device, format);
    
	if (!device) {
		aal_exception_error("Invalid device has been detected.");
		return -1;
	}
    
	if (blk == aal_block_number(journal->header)) {
		if (aal_block_sync(journal->header)) {
			aal_exception_error("Can't write journal header to block %llu. %s.", 
					    blk, device->error);
			return -1;
		}
	} else {
		if (aal_block_sync(journal->footer)) {
			aal_exception_error("Can't write journal footer to block %llu. %s.", 
					    blk, device->error);
			return -1;
		}
	}
    
	return 0;
}

static errno_t journal40_sync(object_entity_t *entity) {
	reiser4_layout_func_t layout;
	journal40_t *journal = (journal40_t *)entity;

	aal_assert("umka-410", journal != NULL, return -1);
    
	if (!(layout = journal->format->plugin->format_ops.journal_layout)) {
		aal_exception_error(
			"Method \"journal_layout\" doesn't implemented in format plugin.");
		return -1;
	}
    
	if (layout(journal->format, callback_sync_journal, journal)) {
		aal_exception_error("Can't load journal metadata.");
		return -1;
	}
    
	return 0;
}

static errno_t callback_layout(journal40_t *journal, 
			       aal_block_t *block,
			       d64_t original) 
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

static errno_t callback_update(journal40_t *journal, 
			       aal_block_t *block) 
{	
	journal40_footer_t *footer = (journal40_footer_t *)journal->footer->data;	
	journal40_tx_header_t *tx_header = (journal40_tx_header_t *)block->data;
	
	/* Updating journal footer */
	set_jf_last_flushed(footer, aal_block_number(block));
	set_jf_free_blocks(footer, get_th_free_blocks(tx_header));
	set_jf_nr_files(footer, get_th_nr_files(tx_header));
	set_jf_next_oid(footer, get_th_next_oid(tx_header));
	
	return 0;
}

static errno_t journal40_lr_layout(journal40_t *journal, 
				   aal_block_t *tx_block,
				   journal40_layout_func_t layout_func,
				   journal40_update_func_t update_func)
{
	uint32_t total;
	uint64_t log_blk;
	uint32_t end_blk;
    
	aal_device_t *device;
	aal_block_t *log_block;

	journal40_lr_header_t *lr_header;
	journal40_tx_header_t *tx_header;
    
	device = journal->device;
	tx_header = (journal40_tx_header_t *)tx_block->data;
    
	end_blk = aal_block_number(tx_block);
	log_blk = get_th_next_block(tx_header);
	total = get_th_total(tx_header);
    
	while (log_blk != end_blk) {
		uint32_t i, capacity;
		journal40_lr_entry_t *entry;

		if (!layout_func)
			break;
		    
		if (!(log_block = aal_block_open(device, log_blk))) {
			aal_exception_error("Can't read block %llu while replaying "
					    "the journal. %s.", log_blk, device->error);
			return -1;
		}

		lr_header = (journal40_lr_header_t *)log_block->data;
		log_blk = get_lh_next_block(lr_header);

		if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
			aal_exception_error("Invalid log record header has been detected.");
			return -1;
		}
	
		entry = (journal40_lr_entry_t *)(lr_header + 1);
	
		capacity = (device->blocksize - sizeof(journal40_lr_header_t)) / 
			sizeof(journal40_lr_entry_t);
	
		for (i = 0; i < capacity; i++) {
			aal_block_t *block = aal_block_open(device, get_le_wandered(entry));
	    
			if (!block) {
				aal_exception_error("Can't read block %llu while replaying "
						    "the journal. %s.", get_le_wandered(entry),
						    device->error);
				return -1;
			}

			if (layout_func(journal, block, get_le_original(entry)))
				return -1;
			
			aal_block_close(block);
			entry++;
		}

		aal_block_close(log_block);

	}

	if (update_func)
		update_func(journal, tx_block);
	
	return 0;
}

/* 
    Returns 0 if replayed, 1 if there is no any transaction to be flushed and 
    -1 in the case of error.
*/
static errno_t format40_tx_layout(journal40_t *journal, 
				  journal40_layout_func_t layout_func,
				  journal40_update_func_t update_func) 
{
	int ret;
	uint64_t prev_tx;
	uint64_t last_flushed_tx;
	uint64_t last_commited_tx;

	aal_block_t *tx_block;
	journal40_tx_header_t *tx_header;
    
	last_commited_tx = 
		get_jh_last_commited((journal40_header_t *)journal->header->data);
    
	last_flushed_tx = 
		get_jf_last_flushed((journal40_footer_t *)journal->footer->data);
    
	/* Check if all transactions are replayed */
	if (last_commited_tx == last_flushed_tx)
		return 1;

	prev_tx = last_commited_tx;
    
	/* Searching for oldest not flushed transaction */
	while (1) {

		if (!(tx_block = aal_block_open(journal->device, prev_tx))) {
			aal_exception_error("Can't read block %llu while replaying "
					    "the journal. %s.", prev_tx,
					    journal->device->error);
			return -1;
		}
	
		tx_header = (journal40_tx_header_t *)tx_block->data;

		if (aal_memcmp(tx_header->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
			aal_exception_error("Invalid transaction header has "
					    "been detected.");
			return -1;
		}
	
		prev_tx = get_th_prev_tx(tx_header);

		if (prev_tx == last_flushed_tx)
			break;

		aal_block_close(tx_block);
	}
    
	ret = journal40_lr_layout(journal, tx_block, layout_func, update_func);
	
	aal_block_close(tx_block);

	return ret;
}

static int journal40_replay(object_entity_t *entity) {
	int trans_nr = 0;
    
	aal_assert("umka-412", entity != NULL, return -1);

	while (format40_tx_layout((journal40_t *)entity, callback_layout, 
				  callback_update) == 0)
		trans_nr++;
    
	if (trans_nr)
		journal40_sync(entity);
    
	return trans_nr;
}

static errno_t journal40_print(object_entity_t *entity, char *buff, 
			      uint32_t n, uint16_t options)
{
	aal_assert("umka-1465", entity != NULL, return -1);
	aal_assert("umka-1466", buff != NULL, return -1);

	return 0;
}

#endif

static void journal40_close(object_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;
    
	aal_assert("umka-411", entity != NULL, return);

	aal_block_close(journal->header);
	aal_block_close(journal->footer);
	aal_free(journal);
}

static reiser4_plugin_t journal40_plugin = {
	.journal_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = JOURNAL_REISER40_ID,
				.group = 0,
				.type = JOURNAL_PLUGIN_TYPE
			},
			.label = "journal40",
			.desc = "Default journal for reiserfs 4.0, ver. " VERSION,
		},
		.open	= journal40_open,
		
#ifndef ENABLE_COMPACT
		.create	= journal40_create,
		.sync	= journal40_sync,
		.replay = journal40_replay,
		.print  = journal40_print,
#else
		.create = NULL,
		.sync	= NULL,
		.replay = NULL,
		.print  = NULL,
#endif
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

