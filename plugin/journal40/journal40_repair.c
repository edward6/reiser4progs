/*
  journal40.c -- reiser4 default journal plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "journal40.h"
#include "aux/bitmap.h"

/*

Jouranl check 
 a. no one block can point before the master sb and after the end of a 
    partition.
 b. TxH, LR, wandered blocks
    - cannot be met more then once.
    - cannot point to format area.
    - cannot point to original blocks from the current transaction and from 
    the previous ones.
 c. Original blocks 
    - cannot be met more then once in the same transaction.
    - cannot point to any journal block from next transactions.

Problems.
 1. out of bounds.
 2. format area.
 3. wrong magic.
 4. met before as of the same type
 5. met before as not of the same type.

Traverse is going through the TxH in back order first, then through transactions 
themselves in right order. So TxH blocks are handled before all others.
 
I. TxH.
 1.2.3.4. kill the journal
 5. (Good magic, so we met it as wand/orig in previous transactions) - cut the 
    journal to that previous transaction; - cannot happen as TxH blocks are 
    handled first.
II. LR.
 1.2.3. Cut the journal to the current transaction. 
 4.5. (LR magic is ok) 
    If the current transaction is of a good LR circle - cut the journal to that 
    previous transaction. If not OK - to this transaction. 
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

FIXME-VITALY: For now II.4.5. and III.5. - cut to the previous transaction. 

*/

/* Flags. */
#define SAME_TXH_BREAK	1

typedef struct journal40_check {
    aux_bitmap_t *journal_layout;   /* Blocks are pointed by journal. */
    aux_bitmap_t *current_layout;   /* Blocks of current trans only. */
    blk_t format_start, format_len; /* Format bounds */
    blk_t cur_txh;		    /* Current TxH block - for current trans 
				       blocks handling. Should be set to 
				       TxH blocknumber which should be cut 
				       off. */
    blk_t find_blk;		    /* Look for this block. and put the found 
				       trans here. */
    int found_type;		    /* Put the type of the found block here. */
    int older_txh;		    /*  */
    int flags;
} journal40_check_t;

typedef errno_t (*journal40_handler_func_t)(object_entity_t *, aal_block_t *, 
    d64_t, void *);
typedef errno_t (*journal40_txh_func_t)(object_entity_t *, blk_t, void *);
typedef errno_t (*journal40_sec_func_t)(object_entity_t *, aal_block_t *, 
    blk_t, int, void *);

extern errno_t journal40_traverse(journal40_t *, journal40_handler_func_t,
    journal40_txh_func_t,journal40_sec_func_t, void *);

extern errno_t journal40_traverse_trans(journal40_t *, aal_block_t *, 
    journal40_handler_func_t, journal40_sec_func_t, void *);

extern aal_device_t *journal40_device(object_entity_t *entity);

static errno_t callback_mark_format_block(object_entity_t *format, blk_t blk,  
    void *data)
{
    aux_bitmap_t *bm = (aux_bitmap_t *)data;
    aux_bitmap_mark(bm, blk);
    
    return 0;
}

static int journal40_blk_format_check(object_entity_t *format, blk_t blk, 
    journal40_check_t *data) 
{
    aal_assert("vpf-490", format != NULL, return -1);
    aal_assert("vpf-492", data != NULL, return -1);

    /* blk is out of format bound */
    if (blk >= data->format_len || blk < data->format_start) 
	return 1;

    /* blk belongs to format area */
    return plugin_call(return -1, format->plugin->format_ops, layout, format, 
	callback_mark_format_block, data->journal_layout);
}

static errno_t callback_find_txh_blk(object_entity_t *entity, blk_t blk, 
    void *data) 
{
    journal40_check_t *check_data = (journal40_check_t *)data;
 
    if (check_data->find_blk == blk) {
	/* Found as a TxH of one of the early transaction. Cut to this 
	 * transaction. find_blk equals blk already. */
	check_data->found_type = TXH;
	return 1;
    }
 
    if (check_data->cur_txh == blk) {
	if (check_data->flags & (1 << SAME_TXH_BREAK)) {
	    check_data->found_type = 0;
	    return 1;
	}

	check_data->older_txh = 1;
    }
 
    return 0;    
}

static errno_t callback_find_sec_blk(object_entity_t *entity, 
    aal_block_t *txh_block, blk_t blk, int blk_type, void *data) 
{
    journal40_check_t *check_data = (journal40_check_t *)data;

    if (check_data->find_blk == blk) {
	/* Wanted blk found, cut to this transaction. */
	check_data->find_blk = aal_block_number(txh_block);
	check_data->found_type = blk_type;
	return 1;
    }

    return 0;
}

static errno_t callback_journal_txh_check(object_entity_t *entity, blk_t blk, 
    void *data) 
{
    journal40_t *journal = (journal40_t *)entity;
    journal40_check_t *check_data = (journal40_check_t *)data;
    errno_t ret;

    aal_assert("vpf-461", journal != NULL, return -1);
    aal_assert("vpf-491", check_data != NULL, return -1);

    if ((ret = journal40_blk_format_check(journal->format, blk, check_data)))
	return ret;

    if (aux_bitmap_test(check_data->journal_layout, blk)) 
	/* TxH block is met not for the 1 time. Kill the journal. */
	return 1;
    
    aux_bitmap_mark(check_data->journal_layout, blk);
    
    return 0;
}

static errno_t callback_journal_sec_check(object_entity_t *entity, 
    aal_block_t *txh_block, blk_t blk, int blk_type, void *data) 
{
    journal40_t *journal = (journal40_t *)entity;
    journal40_check_t *check_data = (journal40_check_t *)data;
    errno_t ret;

    aal_assert("vpf-461", journal != NULL, return -1);
    aal_assert("vpf-491", check_data != NULL, return -1);
    aal_assert("vpf-506", txh_block != NULL, return -1);
    aal_assert("vpf-507", txh_block->device != NULL, return -1);    

    /* If we start working with a new trans, zero the current trans bitmap. */
    if (check_data->cur_txh != aal_block_number(txh_block)) {
	aal_memset(check_data->current_layout->map, 
	    check_data->current_layout->size, 0);
	check_data->cur_txh = aal_block_number(txh_block);
    }
    if ((ret = journal40_blk_format_check(journal->format, blk, check_data))) 
	return ret;

    /* Read the block and check the magic for LR. */
    if (blk_type == LGR) {
	aal_block_t *log_block;
	journal40_lr_header_t *lr_header;

	if (!(log_block = aal_block_open(txh_block->device, blk))) {
	    aal_exception_error("Can't read block %llu while traversing "
		"the journal. %s.", blk, txh_block->device->error);
	    return -1;
	}

	lr_header = (journal40_lr_header_t *)log_block->data;

	if (aal_memcmp(lr_header->magic, LGR_MAGIC, LGR_MAGIC_SIZE)) {
	    aal_block_close(log_block);
	    return 1;
	}

	aal_block_close(log_block);
    }

    if (aux_bitmap_test(check_data->journal_layout, blk)) {
	/* Block was met before. */
	if (blk_type == LGR) {
	    /* Run traverse for this trans only with no callbacks. 
	     * If returns not 0 - cur_txh = blk_txh. 0 - no problem - run 
	     * traverse for searching the first transaction where blk is met. 
	     * If returns not 0 - cur_txh = blk_txh */
	    
	    /* blk was met in the current trans more then once. */
	    if (aux_bitmap_test(check_data->current_layout, blk)) 
		return 1;

	    ret = journal40_traverse_trans(journal, txh_block, NULL, NULL, NULL);
	    if (ret == 0) {
		/* Find the place we met blk previous time. */
		check_data->find_blk = blk;
		if ((ret = journal40_traverse(journal, NULL, NULL, 
		    callback_find_sec_blk, check_data)) != 1) 
		{
		    aal_exception_bug("Traverse which should find a transaction"
			" the block (%llu) was met for the first time returned "
			"the unexpected value (%d).", blk, ret);
		    return -1;
		}
		check_data->cur_txh = check_data->find_blk;
	    } else if (ret < 0) 
		return ret;

	    return 1;
	} else if (blk_type == WAN) {
	    /* Run the whole traverse to get the type of blk we met before.
	     * If TxH or LR - cur_txh = blk_txh, 
	     * Otherwise cur_txh = blk_THAT_txh. */	    
	    check_data->find_blk = blk;
	    check_data->flags = 0;
	    if ((ret = journal40_traverse(journal, NULL, callback_find_txh_blk, 
		callback_find_sec_blk, check_data)) != 1)
	    {
		aal_exception_bug("Traverse which should find a transaction"
		    " the block (%llu) was met for the first time returned "
		    "the unexpected value (%d).", blk, ret);
		return -1;
	    }
	    
	    if (check_data->found_type == WAN || check_data->found_type == ORG)
		check_data->cur_txh = check_data->find_blk;

	    return 1;
	} else if (blk_type == ORG) {
	    /* Did we met this block in the same trans already? */
	    if (aux_bitmap_test(check_data->current_layout, blk)) 
		return 1;

	    /* What if the TxH of any next trans? */	    
	    check_data->find_blk = blk;
	    check_data->flags = (1 << SAME_TXH_BREAK);
	    if ((ret = journal40_traverse(journal, NULL, callback_find_txh_blk, 
		NULL, check_data)) != 1)
	    {
		aal_exception_bug("Traverse which should find a transaction"
		    " the block (%llu) was met for the first time returned "
		    "the unexpected value (%d).", blk, ret);
		return -1;
	    }

	    if (check_data->found_type != 0) 		
		return 1;
	}
    }

    aux_bitmap_mark(check_data->journal_layout, blk);
    aux_bitmap_mark(check_data->current_layout, blk);
 
    return 0;
}

errno_t journal40_check(object_entity_t *entity) {
    journal40_t *journal = (journal40_t *)entity;
    journal40_check_t data;
    
    aal_assert("vpf-447", journal != NULL, return -1);

    aal_memset(&data, sizeof(data), 0);
    
    data.format_start = plugin_call(return -1, journal->format->plugin->format_ops,
	start, journal->format);

    data.format_len = plugin_call(return -1, journal->format->plugin->format_ops, 
	get_len, journal->format);
    
    if (!(data.journal_layout = aux_bitmap_create(data.format_len))) {
	aal_exception_error("Failed to allocate a control bitmap for journal "
	    "layout.");
	return -1;
    }    
     
    if (!(data.current_layout = aux_bitmap_create(data.format_len))) {
	aal_exception_error("Failed to allocate a control bitmap for current "
	    "transaction blocks.");
	return -1;
    }    
    
    return journal40_traverse((journal40_t *)entity, NULL, 
	callback_journal_txh_check, callback_journal_sec_check, &data);
}

