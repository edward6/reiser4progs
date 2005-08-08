/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40.c -- reiser4 default journal plugin.

   Journal check description:
   a. no one block can point before the master sb and after the end of a 
      partition.
   b. TxH, LGR, wandered blocks
      - cannot be met more then once in the whole journal.
      - cannot point to format area.
      - cannot be met as original blocks from the current transaction and from 
        the previous ones, but could be of next ones.
   c. Original blocks 
      - cannot point to any journal block from any next and the current transactions.
 
   Problems.
   1. out of bounds.
   2. format area.
   3. wrong magic.
   4. met before as of the same type
   5. met before as not of the same type.
   
   Traverse is going through the TxH in back order first, then through 
   transactions themselves in right order. So TxH blocks are handled before 
   all others.
   
   I. TxH.
   1.2.3.4. kill the journal
   5. (Good magic, so we met it as wand/orig in previous transactions) - cut the
      journal to that previous transaction; - cannot happen as TxH blocks are 
      handled first.
   II. LGR.
   1.2.3. Cut the journal to the current transaction. 
   4.5. (LGR magic is ok) 
        If the current transaction is of a good LGR circle - cut the journal to 
	that previous transaction. If not OK - to this transaction. 
   III. wandered
   1.2. Cut the journal to the current transaction. 
   3. -. 
   4. (+ orig) Cut the journal to the previous transaction. 
   5. (TxH, LG) Cut the journal to this transaction.
   IV. original
   1.2. Cut the journal to the current transaction.
   3. -.
   4.5. In older transactions - no problem. Otherwise, cut the journal to the 
        current transaction (do not forget to check the current). 

   FIXME-VITALY: For now II.4.5. and III.5. - cut to the previous transaction. */

#ifndef ENABLE_MINIMAL

#include "journal40.h"
#include "journal40_repair.h"

#include <reiser4/bitmap.h>
#include <repair/plugin.h>

/* Traverse flags. */
#define TF_SAME_TXH_BREAK   1   /* break when current trans was reached. */
#define TF_DATA_AREA_ONLY   2   /* check if block lies in data area only or in 
				   data and format area. */

typedef struct journal40_check {
	reiser4_bitmap_t *journal_layout;   /* All blocks are pointed by journal. */
	reiser4_bitmap_t *current_layout;   /* Blocks of current trans only. */
	blk_t cur_txh;			    /* TxH block of the current trans at
					       traverse time. And the oldest problem 
					       trans at traverse return time if return 1. */
	blk_t wanted_blk;		    /* Nested traverses look for this block and 
					       put the TxH block of the found trans here. */
	journal40_block_t found_type;	    /* Put the type of the found block here. */
	int flags;
	
	layout_func_t layout_func;
	void *layout_data;
} journal40_check_t;

static char *blk_types[] = {
	"Unknown",
	"Transaction Header",
	"Log Record",
	"Wandered",
	"Original"
};

static char *__blk_type_name(journal40_block_t blk_type) {
	if (blk_type == JB_INV || blk_type >= JB_LST)
		return blk_types[0];
	
	return blk_types[blk_type];
}

/* Callback for format.layout. Returns 1 for all fotmat blocks. */
static errno_t cb_check_format_block(blk_t start, count_t width, void *data) {
	blk_t blk = *(blk_t *)data;
	return (blk >= start && blk < start + width);
}

/* Check if blk belongs to format area. */
static errno_t journal40_blk_format_check(journal40_t *journal, blk_t blk, 
					  journal40_check_t *data) 
{
	aal_assert("vpf-490", journal != NULL);
	aal_assert("vpf-492", data != NULL);
	
	/* blk is out of format bound */
	if (blk >= journal->area.len || blk < journal->area.start) 
		return -ESTRUCT;
	
	/* If blk can be from format area, nothing to check anymore. */
	if (!(data->flags & (1 << TF_DATA_AREA_ONLY)))
		return 0;
	
	/* blk belongs to format area */
	return data->layout_func(data->layout_data, 
				 cb_check_format_block, &blk) ? -ESTRUCT : 0;
}

/* TxH callback for nested traverses. Should find the transaction which TxH 
   block number equals to wanted blk. Set found_type then to TxH. Returns 1 
   when traverse should be stopped, zeroes found_type if no satisfied 
   transaction was found. */
static errno_t cb_find_txh_blk(generic_entity_t *entity, blk_t blk, void *data)
{
	journal40_check_t *check_data = (journal40_check_t *)data;
	
	/* If wanted blk == TxH block number. */
	if (check_data->wanted_blk == blk) {
		/* wanted_blk equals blk already. */
		check_data->found_type = JB_TXH;
		return -ESTRUCT;
	}
	
	/* If the current transaction was reached and traverse should stop here. */
	if ((check_data->cur_txh == blk) && 
	    (check_data->flags & (1 << TF_SAME_TXH_BREAK))) 
	{
		check_data->found_type = JB_INV;
		return -ESTRUCT;
	}
	
	return 0;    
}

/* Secondary (not TxH) blocks callback for nested traverses. Should find the 
   transaction which contains block number equal to wanted blk. Set wanted_blk 
   to TxH block number and found_type to the type of found blk. */
static errno_t cb_find_sec_blk(generic_entity_t *entity, aal_block_t *txh_block,
			       blk_t blk, journal40_block_t blk_type,void *data)
{
	journal40_check_t *check_data = (journal40_check_t *)data;
	
	if (check_data->wanted_blk == blk) {
		check_data->wanted_blk = txh_block->nr;
		check_data->found_type = blk_type;
		return -ESTRUCT;
	}
	
	return 0;
}

/* TxH callback for traverse. Returns 1 if blk is a block out of format bound 
   or of format area or is met more then once. data->cur_txh = 0 all the way
   here to explain the traverse caller that the whole journal is invalid. */
static errno_t cb_journal_txh_check(generic_entity_t *entity, 
				    blk_t blk, void *data) 
{
	journal40_t *journal = (journal40_t *)entity;
	journal40_check_t *check_data = (journal40_check_t *)data;
	
	aal_assert("vpf-461", journal != NULL);
	aal_assert("vpf-491", check_data != NULL);
	
	check_data->flags = 1 << TF_DATA_AREA_ONLY;
	
	if (journal40_blk_format_check(journal, blk, check_data)) {
		fsck_mess("Transaction header lies in the illegal block "
			  "(%llu) for the used format (%s).", blk, 
			  journal->format->plug->label);
		return -ESTRUCT;
	}
	
	if (reiser4_bitmap_test(check_data->journal_layout, blk)) {
		/* TxH block is met not for the 1 time. Kill the journal. */
		fsck_mess("Transaction header in the block (%llu) was "
			  "met already.", blk);
		return -ESTRUCT;
	}
	
	reiser4_bitmap_mark(check_data->journal_layout, blk);
	
	return 0;
}

/* Secondary blocks callback for traverse. Does all the work described above for 
   all block types except TxH. */
static errno_t cb_journal_sec_check(generic_entity_t *entity, 
				    aal_block_t *txh_block, blk_t blk, 
				    journal40_block_t blk_type, void *data)
{
	journal40_t *journal = (journal40_t *)entity;
	journal40_check_t *check_data = (journal40_check_t *)data;
	errno_t res;
	
	aal_assert("vpf-461", journal != NULL);
	aal_assert("vpf-491", check_data != NULL);
	aal_assert("vpf-506", txh_block != NULL);
	aal_assert("vpf-507", txh_block->device != NULL);
	
	/* If we start working with a new trans, zero the current trans bitmap. */
	if (check_data->cur_txh != txh_block->nr) {
		aal_memset(check_data->current_layout->map, 0,
			   check_data->current_layout->size);
		check_data->cur_txh = txh_block->nr;
	}
	
	/* Check that blk is not out of bound and (not for original block) that 
	   it is not from format area. */
	check_data->flags = blk_type == JB_ORG ? 0 : 1 << TF_DATA_AREA_ONLY;
	
	if (journal40_blk_format_check(journal, blk, check_data)) {
		fsck_mess("%s lies in the illegal block (%llu) for the "
			  "used format (%s).", __blk_type_name(blk_type), 
			  blk, journal->format->plug->label);
		return -ESTRUCT;
	}
	
	/* Read the block and check the magic for LGR. */
	if (blk_type == JB_LGR) {
		aal_block_t *log_block;
		journal40_lr_header_t *lr_header;
		
		if (!(log_block = aal_block_load(txh_block->device,
						 journal->blksize, blk)))
		{
			aal_error("Can't read block %llu while "
				  "traversing the journal. %s.", blk, 
				  txh_block->device->error);
			return -EIO;
		}
		
		lr_header = (journal40_lr_header_t *)log_block->data;
		
		if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
			fsck_mess("Transaction Header (%llu), Log record "
				  "(%llu): Log Record Magic was not found.", 
				  check_data->cur_txh, blk);
			aal_block_free(log_block);
			return -ESTRUCT;
		}
		
		aal_block_free(log_block);
	}
	
	if (reiser4_bitmap_test(check_data->journal_layout, blk)) {
		/* blk was met in the current trans more then once. */
		if (reiser4_bitmap_test(check_data->current_layout, blk)) {
			fsck_mess("Transaction Header (%llu): %s block "
				  "(%llu) was met in the transaction "
				  "more then once.", check_data->cur_txh, 
				  __blk_type_name(blk_type), blk);
			return -ESTRUCT;
		}
		
		/* Block was met before. */
		if (blk_type == JB_LGR) {
			/* Check LRG circle for this trans. If it is valid - cut 
			   the journal to the trans where blk was met for the 
			   first time. If it is not valid - cut the journal to 
			   this trans.
			   
			   Run traverse for this trans only with no callbacks. 
			   If returns not 0 - cur_txh = blk_txh. 0 - no problem - 
			   run traverse for searching the first transaction where 
			   blk is met. If returns not 0 - cur_txh = blk_txh */
	    
			/* Traverse of 1 trans with no callbacks shows if LRG 
			   circle is valid. */
			res = journal40_traverse_trans(journal, txh_block, NULL, 
						       NULL, NULL);
			if (res == 0) {
				/* Find the place we met blk previous time. */
				check_data->wanted_blk = blk;
				
				res = journal40_traverse(journal, NULL, NULL, 
							 cb_find_sec_blk, 
							 check_data);
				
				if (res != -ESTRUCT) {
					aal_error("Traverse failed to find "
						  "a transaction the block "
						  "(%llu) was met for the "
						  "first time.", blk);
					return res;
				}
				
				/* Found trans is the oldest problem, return it 
				   to caller. */				
				fsck_mess("Transaction Header (%llu): "
					  "transaction looks correct but "
					  "uses the block (%llu) already "
					  "used in the transaction (%llu).", 
					  check_data->cur_txh, blk, 
					  check_data->wanted_blk);
				
				check_data->cur_txh = check_data->wanted_blk;
			} else if (res != -ESTRUCT) {
				aal_error("Transaction Header (%llu): "
					  "corrupted log record circle "
					  "found.", txh_block->nr);
				
				return res;
			}
			
			return -ESTRUCT;
		} else if (blk_type == JB_WAN) {
			/* Run the whole traverse to find the transaction we met blk 
			   for the first time and get its type. */
			check_data->wanted_blk = blk;
			check_data->flags = 0;
			
			res = journal40_traverse(journal, cb_find_txh_blk, 
						 NULL, cb_find_sec_blk, 
						 check_data);
			
			if (res != -ESTRUCT) {
				aal_error("Traverse failed to find a "
					  "transaction the block (%llu) was "
					  "met for the first time.", blk);
				return res;
			}
			
			fsck_mess("Transaction Header (%llu): transaction "
				  "looks correct but uses the block (%llu) "
				  "already used in the transaction (%llu) "
				  "as a %s block.", check_data->cur_txh,
				  blk, check_data->wanted_blk, 
				  __blk_type_name(check_data->found_type));
			
			/* The oldest problem transaction for TxH or LGR is the 
			   current one, and for WAN, ORG is that found trans. */
			if (check_data->found_type == JB_WAN || 
			    check_data->found_type == JB_ORG)
				check_data->cur_txh = check_data->wanted_blk;
			
			return -ESTRUCT;
		} else if (blk_type == JB_ORG) {
			/* It could be met before as TxH block of a next trans or 
			   as any other block of previous trans. It is legal to 
			   meet it in a previous trans. Run traverse with one txh 
			   callback to check for next trans' TxH blocks. */
			check_data->wanted_blk = blk;
			
			/* Stop looking through TxH's when reach the current trans. */
			check_data->flags = (1 << TF_SAME_TXH_BREAK);
			
			res = journal40_traverse(journal, cb_find_txh_blk, 
						 NULL, NULL, check_data);
			
			if (res != -ESTRUCT) {
				aal_error("Traverse failed to find a "
					  "transaction the block (%llu) was "
					  "met for the first time.", blk);
				return res;
			}
			
			/* If TxH was found, the current transaction is the oldest 
			   problem trans. */
			if (check_data->found_type != JB_INV) {
				fsck_mess("Transaction Header (%llu): "
					  "original location (%llu) was "
					  "met before as a Transaction "
					  "Header of one of the next "
					  "transactions.", 
					  check_data->cur_txh, blk);
				return -ESTRUCT;
			}
		}
	}
	
	reiser4_bitmap_mark(check_data->journal_layout, blk);
	reiser4_bitmap_mark(check_data->current_layout, blk);
	
	return 0;
}

errno_t journal40_check_struct(generic_entity_t *entity, 
			       layout_func_t func, void *data)
{
	journal40_t *journal = (journal40_t *)entity;
	journal40_header_t *header;
	journal40_tx_header_t *txh;
	journal40_check_t jdata;
	errno_t ret;

	aal_assert("vpf-447", journal != NULL);
	aal_assert("vpf-733", func != NULL);

	aal_memset(&jdata, 0, sizeof(jdata));

	jdata.layout_func = func;
	jdata.layout_data = data;

	if (!(jdata.journal_layout = 
	      reiser4_bitmap_create(journal->area.len))) 
	{
		aal_error("Failed to allocate a control bitmap for "
			  "journal layout.");
		return -ENOMEM;
	}

	if (!(jdata.current_layout = 
	      reiser4_bitmap_create(journal->area.len))) 
	{
		aal_error("Failed to allocate a control bitmap of the "
			  "current transaction blocks.");
		
		ret = -ENOMEM;
		goto error_free_layout;
	}

	ret = journal40_traverse((journal40_t *)entity, cb_journal_txh_check,
				 NULL, cb_journal_sec_check, &jdata);

	if (ret && ret != -ESTRUCT)
		goto error_free_current;

	if (ret) {
		/* Journal should be updated */
		if (!jdata.cur_txh) {
			fsck_mess("Journal has broken list of transaction "
				  "headers. Reinitialize the journal.");

			jdata.cur_txh = 
				get_jf_last_flushed((journal40_footer_t *)
						    journal->footer->data);
		} else {
			aal_block_t *tx_block = NULL;
			aal_device_t *device = NULL;

			/* jdata.cur_txh is the oldest problem transaction. 
			   Set the last_committed to the previous one. */
			device = journal40_device((generic_entity_t *)journal);

			if (device == NULL) {
				aal_error("Invalid device has been detected.");
				ret = -EINVAL;
				goto error_free_current;
			}

			tx_block = aal_block_load(device, 
						  journal->blksize,
						  jdata.cur_txh);
			
			if (!tx_block) {
				aal_error("Can't read the block %llu while "
					  "checking the journal. %s.", 
					  jdata.cur_txh, device->error);
				ret = -EIO;
				goto error_free_current;
			}

			txh = (journal40_tx_header_t *)tx_block->data;
			fsck_mess("Corrupted transaction (%llu) was found. "
				  "The last valid transaction is (%llu).", 
				  jdata.cur_txh, get_th_prev_tx(txh));

			jdata.cur_txh = get_th_prev_tx(txh);

			aal_block_free(tx_block);
		}

		header = (journal40_header_t *)journal->header->data;
		set_jh_last_commited(header, jdata.cur_txh);

		journal40_mkdirty(journal);
	}
	
	reiser4_bitmap_close(jdata.current_layout);
	reiser4_bitmap_close(jdata.journal_layout);

	return 0;
	
 error_free_current:
	reiser4_bitmap_close(jdata.current_layout);
 error_free_layout:
	reiser4_bitmap_close(jdata.journal_layout);
	return ret;
}

void journal40_invalidate(generic_entity_t *entity) {
	journal40_t *journal = (journal40_t *)entity;
	journal40_footer_t *footer;
	journal40_header_t *header;

	aal_assert("vpf-1554", entity != NULL);

	footer = JFOOTER(journal->footer);
	header = JHEADER(journal->header);

	set_jh_last_commited(header, 0);
	set_jf_last_flushed(footer, 0);
	set_jf_free_blocks(footer, 0);
	set_jf_used_oids(footer, 0);
	set_jf_next_oid(footer, 0);

	journal40_mkdirty(journal);
}

/* Safely extracts string field from passed location. */
static void extract_string(char *stor, char *orig, uint32_t max) {
	uint32_t i;
	
	for (i = 0; i < max; i++) {
		if (orig[i] == '\0')
			break;
	}

	aal_memcpy(stor, orig, i);
}

/* Helper function for printing transaction header. */
static errno_t cb_print_txh(generic_entity_t *entity,
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
static errno_t cb_print_par(generic_entity_t *entity,
			    aal_block_t *block,
			    blk_t orig, void *data)
{
	aal_stream_format((aal_stream_t *)data,
			  "%llu -> %llu\n", orig, block->nr);
	return 0;
}

/* hellper function for printing log record. */
static errno_t cb_print_lgr(generic_entity_t *entity,
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
void journal40_print(generic_entity_t *entity,
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
	
	aal_stream_format(stream, "Journal header block (%llu):\n", 
			  journal->header->nr);

	aal_stream_format(stream, "last commited:\t%llu\n\n",
			  get_jh_last_commited(header));
		
	aal_stream_format(stream, "Journal footer block (%llu):\n",
			  journal->footer->nr);

	aal_stream_format(stream, "last flushed:\t%llu\n",
			  get_jf_last_flushed(footer));
	
	aal_stream_format(stream, "free blocks:\t%llu\n",
			  get_jf_free_blocks(footer));
	
	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  get_jf_next_oid(footer));
	
	aal_stream_format(stream, "used oids:\t%llu\n",
			  get_jf_used_oids(footer));

	/* Print all transactions. */
	journal40_traverse((journal40_t *)entity, cb_print_txh,
			   cb_print_par, cb_print_lgr, (void *)stream);
}

static errno_t journal40_block_pack(journal40_t *journal, aal_stream_t *stream,
				    reiser4_bitmap_t *layout, uint64_t blk) 
{
	journal40_tx_header_t *txh;
	journal40_lr_header_t *lrh;
	journal40_lr_entry_t *lre;
	aal_block_t *block;
	uint32_t i, num;
	errno_t res;
	
	if (blk < journal->area.start || blk >= journal->area.len)
		return 0;
	
	if (reiser4_bitmap_test(layout, blk))
		return 0;

	reiser4_bitmap_mark(layout, blk);

	if (!(block = aal_block_load(journal->device, journal->blksize, blk))) {
		aal_error("Can't read block %llu while traversing the journal."
			  "%s.", blk, journal->device->error);
		return -EIO;
	}
	
	aal_stream_write(stream, BLOCK_PACK_SIGN, 4);
	aal_stream_write(stream, &block->nr, sizeof(block->nr));
	aal_stream_write(stream, block->data, block->size);

	txh = (journal40_tx_header_t *)block->data;
	if (!aal_memcmp(txh->magic, TXH_MAGIC, TXH_MAGIC_SIZE)) {
		if ((res = journal40_block_pack(journal, stream, layout, 
						get_th_next_block(txh))))
		{
			goto done_block;
		}

		if ((res = journal40_block_pack(journal, stream, layout, 
						get_th_prev_tx(txh))))
		{
			goto done_block;
		}
	}

	lrh = (journal40_lr_header_t *)block->data;
	if (!aal_memcmp(lrh->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
		lre = (journal40_lr_entry_t *)(lrh + 1);
		num = (journal->blksize - sizeof(*lrh)) / sizeof(*lre);
		
		for (i = 0; i < num; i++, lre++) {
			blk = get_le_wandered(lre);
			if (!blk) break;

			if ((res = journal40_block_pack(journal, stream, 
							layout, blk)))
			{
				goto done_block;
			}
		}
	}
	
	aal_block_free(block);
	return 0;
	
 done_block:
	aal_block_free(block);
	return res;
}

errno_t journal40_pack(generic_entity_t *entity, aal_stream_t *stream) {
	journal40_header_t *jheader;
	journal40_t *journal;
	reiser4_bitmap_t *layout;
	uint64_t blk;
	errno_t res;
	
	aal_assert("vpf-1745", entity != NULL);
	aal_assert("vpf-1746", stream != NULL);

	journal = (journal40_t *)entity;
	
	if (!(layout = reiser4_bitmap_create(journal->area.len))) {
		aal_error("Failed to allocate a control bitmap for "
			  "journal layout.");
		return -ENOMEM;
	}
	
	jheader = (journal40_header_t *)journal->header->data;
	blk = get_jh_last_commited(jheader);
	
	aal_stream_write(stream, journal->header->data, journal->header->size);
	aal_stream_write(stream, journal->footer->data, journal->footer->size);
	
	/* Getting all blocks that are pointed by journal, do not control 
	   the journal structure. */
	res = journal40_block_pack(journal, stream, layout, blk);

	reiser4_bitmap_close(layout);
	return res;
}

generic_entity_t *journal40_unpack(aal_device_t *device, 
				   uint32_t blksize, 
				   generic_entity_t *format, 
				   generic_entity_t *oid,
				   uint64_t start, 
				   uint64_t blocks, 
				   aal_stream_t *stream) 
{
	journal40_t *journal;
	uint64_t read;
	blk_t jblk;
	
	aal_assert("vpf-1755", device != NULL);
	aal_assert("vpf-1756", format != NULL);
	aal_assert("vpf-1757", oid != NULL);
	
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
	jblk =  JOURNAL40_BLOCKNR(blksize);

	if (!(journal->header = aal_block_alloc(device, blksize, jblk))) {
		aal_error("Can't alloc journal header on block %llu.", jblk);
		goto error_free_journal;
	}

	if (!(journal->footer = aal_block_alloc(device, blksize, jblk + 1))) {
		aal_error("Can't alloc journal footer on block %llu.", 
			  jblk + 1);
		
		goto error_free_header;
	}

	read = aal_stream_read(stream, journal->header->data, blksize);
	journal->header->dirty = 1;
	
	if (read != blksize) {
		aal_error("Can't unpack journal header. Stream is over?");
		goto error_free_footer;
	}

	read = aal_stream_read(stream, journal->footer->data, blksize);
	journal->footer->dirty = 1;
	
	if (read != blksize) {
		aal_error("Can't unpack journal footer. Stream is over?");
		goto error_free_footer;
	}
	
	/* Other blocks are unpacked in reiser4_fs_unpack as usual blocks. */
	
	journal40_mkdirty(journal);
	return (generic_entity_t *)journal;

 error_free_footer:
	aal_block_free(journal->footer);
 error_free_header:
	aal_block_free(journal->header);
 error_free_journal:
	aal_free(journal);
	return NULL;
}

#endif
