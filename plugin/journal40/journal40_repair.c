/*
  journal40.c -- reiser4 default journal plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "journal40.h"

typedef errno_t (*journal40_handler_func_t)(object_entity_t *entity,  
    aal_block_t *block, d64_t original);
typedef errno_t (*journal40_blk_func_t)(object_entity_t *entity, blk_t);

extern errno_t journal40_traverse(journal40_t *journal, 
	journal40_handler_func_t handler_func, journal40_blk_func_t layout_func,
	journal40_blk_func_t target_func);

static errno_t callback_journal_layout_check(object_entity_t *entity, blk_t blk) {
    journal40_t *journal = (journal40_t *)entity;

    aal_assert("vpf-461", journal != NULL, return -1);

    return 0;
}

static errno_t callback_journal_original_check(object_entity_t *entity, blk_t blk) {
    journal40_t *journal = (journal40_t *)entity;

    aal_assert("vpf-466", journal != NULL, return -1);

    return 0;
}

errno_t journal40_check(object_entity_t *entity) {
    aal_assert("vpf-447", entity != NULL, return -1);

    return journal40_traverse((journal40_t *)entity, NULL, 
	callback_journal_layout_check, callback_journal_original_check);
}

