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
 b. TxH, LGR, wandered blocks
    - cannot be met more then once in the whole journal.
    - cannot point to format area.
    - cannot be met as original blocks from the current transaction and from 
    the previous ones, but could be of next ones.
 c. Original blocks 
    - cannot point to any journal block from any next and the current 
    transactions.

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
II. LGR.
 1.2.3. Cut the journal to the current transaction. 
 4.5. (LGR magic is ok) 
    If the current transaction is of a good LGR circle - cut the journal to that 
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

/* Traverse flags. */
#define TF_SAME_TXH_BREAK   1   /* break when current trans was reached. */

typedef struct journal40_check {
    aux_bitmap_t *journal_layout;   /* All blocks are pointed by journal. */
    aux_bitmap_t *current_layout;   /* Blocks of current trans only. */
    blk_t format_start, format_len; /* Format bounds. */
    blk_t cur_txh;		    /* TxH block of the current trans at
				       traverse time. And the oldest problem 
				       trans at traverse return time if return 
				       1. */
    blk_t wanted_blk;		    /* Nested traverses look for this block
				       and put the TxH block of the found trans 
				       here. */
    int found_type;		    /* Put the type of the found block here. */
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


/* Callback for format.layout. Returns 1 for all fotmat blocks. */
static errno_t callback_check_format_block(object_entity_t *format, blk_t blk,  
    void *data)
{
    blk_t *wanted_blk = (blk_t *)data;

    return *wanted_blk == blk;
}

/* Check if blk belongs to format area. */
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
	callback_check_format_block, &blk);
}

/* TxH callback for nested traverses. Should find the transaction which TxH 
 * block number equals to wanted blk. Set found_type then to TxH. 
 * Returns 1 when traverse should be stopped, zeroes found_type if no satisfied 
 * transaction was found. */
static errno_t callback_find_txh_blk(object_entity_t *entity, blk_t blk, 
    void *data) 
{
    journal40_check_t *check_data = (journal40_check_t *)data;

    /* If wanted blk == TxH block number. */
    if (check_data->wanted_blk == blk) {
	/* wanted_blk equals blk already. */
	check_data->found_type = TXH;
	return 1;
    }
 
    /* If the current transaction was reached and traverse should stop here. */
    if ((check_data->cur_txh == blk) && 
	(check_data->flags & (1 << TF_SAME_TXH_BREAK))) 
    {
	check_data->found_type = 0;
	return 1;
    }
 
    return 0;    
}

/* Secondary (not TxH) blocks callback for nested traverses. Should find the 
 * transaction which contains block number equal to wanted blk. Set wanted_blk 
 * to TxH block number and found_type to the type of found blk. */
static errno_t callback_find_sec_blk(object_entity_t *entity, 
    aal_block_t *txh_block, blk_t blk, int blk_type, void *data) 
{
    journal40_check_t *check_data = (journal40_check_t *)data;

    if (check_data->wanted_blk == blk) {
	check_data->wanted_blk = aal_block_number(txh_block);
	check_data->found_type = blk_type;
	return 1;
    }

    return 0;
}

/* TxH callback for traverse. Returns 1 if blk is a block out of format bound 
 * or of format area or is met more then once. data->cur_txh = 0 all the way
 * here to explain the traverse caller that the whole journal is invalid. */
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

/* Secondary blocks callback for traverse. Does all the work described above 
 * for all block types except TxH. */
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
	aal_memset(check_data->current_layout->map, 0,
	    check_data->current_layout->size);
	check_data->cur_txh = aal_block_number(txh_block);
    }

    /* Check that blk is not out of bound and (not for original block) that it 
     * is not from format area. */
    if (blk_type == ORG) {
	if (blk >= check_data->format_len || blk < check_data->format_start) 
	    return 1;
    } else {
	ret = journal40_blk_format_check(journal->format, blk, check_data);
	if (ret)	
	    return ret;
    }    

    /* Read the block and check the magic for LGR. */
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
	    /* Check LRG circle for this trans. If it is valid - cut the 
	     * journal to the trans where blk was met for the first time. 
	     * If it is not valid - cut the journal to this trans. */
	    /* Run traverse for this trans only with no callbacks. 
	     * If returns not 0 - cur_txh = blk_txh. 0 - no problem - run 
	     * traverse for searching the first transaction where blk is met. 
	     * If returns not 0 - cur_txh = blk_txh */
	    
	    /* blk was met in the current trans more then once. */
	    if (aux_bitmap_test(check_data->current_layout, blk)) 
		return 1;

	    /* Traverse of 1 trans with no callbacks shows if LRG circle is 
	     * valid. */
	    ret = journal40_traverse_trans(journal, txh_block, NULL, NULL, NULL);
	    if (ret == 0) {
		/* Find the place we met blk previous time. */
		check_data->wanted_blk = blk;
		if ((ret = journal40_traverse(journal, NULL, NULL, 
		    callback_find_sec_blk, check_data)) != 1) 
		{
		    aal_exception_bug("Traverse which should find a transaction"
			" the block (%llu) was met for the first time returned "
			"the unexpected value (%d).", blk, ret);
		    return -1;
		}
		/* Found trans is the oldest problem, return it to caller. */
		check_data->cur_txh = check_data->wanted_blk;
	    } else if (ret < 0) 
		return ret;

	    return 1;
	} else if (blk_type == WAN) {
	    /* Run the whole traverse to find the transaction we met blk for the first 
	     * time and get its type. */	    
	    check_data->wanted_blk = blk;
	    check_data->flags = 0;
	    if ((ret = journal40_traverse(journal, NULL, callback_find_txh_blk, 
		callback_find_sec_blk, check_data)) != 1)
	    {
		aal_exception_bug("Traverse which should find a transaction"
		    " the block (%llu) was met for the first time returned "
		    "the unexpected value (%d).", blk, ret);
		return -1;
	    }
	
	    /* The oldest problem transaction for TxH or LGR is the current one, 
	     * and for WAN, ORG is that found trans. */    
	    if (check_data->found_type == WAN || check_data->found_type == ORG)
		check_data->cur_txh = check_data->wanted_blk;

	    return 1;
	} else if (blk_type == ORG) {
	    /* Did we met this block in the same trans already? */
	    if (aux_bitmap_test(check_data->current_layout, blk)) 
		return 1;

	    /* If we met it before as TxH block, it could be one of next trans',
	     * run traverse with one txh callback to find it out. */
	    check_data->wanted_blk = blk;

	    /* Stop looking through TxH's when reach the current trans. */
	    check_data->flags = (1 << TF_SAME_TXH_BREAK);
	    if ((ret = journal40_traverse(journal, NULL, callback_find_txh_blk, 
		NULL, check_data)) != 1)
	    {
		aal_exception_bug("Traverse which should find a transaction"
		    " the block (%llu) was met for the first time returned "
		    "the unexpected value (%d).", blk, ret);
		return -1;
	    }

	    /* If TxH was found, the current transaction is the oldest problem 
	     * trans. */
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
    errno_t ret;
    
    aal_assert("vpf-447", journal != NULL, return -1);

    aal_memset(&data, 0, sizeof(data));
    
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
    
    if ((ret = journal40_traverse((journal40_t *)entity, NULL, 
	callback_journal_txh_check, callback_journal_sec_check, &data)) < 0)
	return ret;


    if (ret) {
	/* Journal should be updated */
	if (!data.cur_txh)
	    data.cur_txh = get_jf_last_flushed(
		(journal40_footer_t *)journal->footer->data);
	
	set_jh_last_commited((journal40_header_t *)journal->header->data, 
	    data.cur_txh);
    }
    
    return 0;
    
}

