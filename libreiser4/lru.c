/*
  lru.c -- list of recently used nodes implementation..
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

errno_t reiser4_lru_adjust(reiser4_lru_t *lru) {
	aal_list_t *curr;
	reiser4_node_t *node;
	
	aal_assert("umka-1519", lru != NULL, return -1);

/*	aal_exception_info("Shrinking tree cache on %u nodes.",
			   lru->adjust);*/
	
	if (!lru->adjustable)
		lru->adjust = 1;
	
	if (!(curr = aal_list_last(lru->list)))
		return 0;

	aal_mpressure_disable(lru->mpressure);
	
	while (curr && lru->adjust > 0) {
		node = (reiser4_node_t *)curr->data;
		curr = curr->prev;
		
		if (node->counter == 0) {
			
			if (!node->parent || node->children)
				continue;

/*			if (node->flags & NF_DIRTY)
				reiser4_node_sync(node);*/
			
			reiser4_node_close(node);
		}
	}
	
	aal_mpressure_enable(lru->mpressure);

	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t reiser4_lru_mpressure(void *data, int result) {
	reiser4_lru_t *lru = (reiser4_lru_t *)data;

	if (!lru->adjust || !lru->list || !result)
		return 0;
	
	return reiser4_lru_adjust(lru);
}

#endif

errno_t reiser4_lru_init(reiser4_lru_t *lru) {

#ifndef ENABLE_COMPACT
	lru->mpressure = aal_mpressure_handler_create(reiser4_lru_mpressure,
						      "tree cache", lru);
	lru->adjustable = aal_mpressure_active();
#else
	lru->adjustable = 0;
	lru->mpressure = NULL;
#endif
	
	return 0;
}

void reiser4_lru_fini(reiser4_lru_t *lru) {
	aal_assert("umka-1531", lru != NULL, return);

	if (lru->mpressure)
		aal_mpressure_handler_free(lru->mpressure);
	
	if (lru->list)
		aal_list_free(lru->list);
}

errno_t reiser4_lru_attach(reiser4_lru_t *lru, reiser4_node_t *node) {
	reiser4_node_t *next, *prev;
	
	aal_assert("umka-1525", lru != NULL, return -1);
	aal_assert("umka-1526", node != NULL, return -1);

	if (aal_mpressure_check())
		return -1;
	
	node->lru.prev = lru->list;
	node->lru.next = lru->list ? lru->list->next : NULL;

	lru->list = aal_list_append(lru->list, node);
	lru->adjust++;

	if (node->lru.prev) {
		prev = (reiser4_node_t *)node->lru.prev->data;
		prev->lru.next = lru->list->next;
	}

	if (node->lru.next) {
		next = (reiser4_node_t *)node->lru.next->data;
		next->lru.prev = lru->list->next;
	}

	return 0;
}

errno_t reiser4_lru_detach(reiser4_lru_t *lru, reiser4_node_t *node) {
	reiser4_node_t *next, *prev;
	
	aal_assert("umka-1528", lru != NULL, return -1);
	aal_assert("umka-1527", node != NULL, return -1);

	if (!node->lru.prev && node->lru.next)
		return 0;
	
	if (node->lru.prev) {
		prev = (reiser4_node_t *)node->lru.prev->data;
		prev->lru.next = node->lru.next;
	}
	
	if (node->lru.next) {
		next = (reiser4_node_t *)node->lru.next->data;
		next->lru.prev = node->lru.prev;
	}
	
	if (node->lru.prev)
		aal_list_remove(node->lru.prev, node);
	else
		lru->list = aal_list_remove(lru->list, node);

	if (lru->adjust > 0)
		lru->adjust--;
	
	return 0;
}

errno_t reiser4_lru_touch(reiser4_lru_t *lru, reiser4_node_t *node) {
	reiser4_node_t *next, *prev;
	
	aal_assert("umka-1529", lru != NULL, return -1);
	aal_assert("umka-1530", node != NULL, return -1);

	if (lru->list && node->lru.prev) {
		prev = (reiser4_node_t *)node->lru.prev->data;
		prev->lru.next = node->lru.next;

		if (node->lru.next) {
			next = (reiser4_node_t *)node->lru.next->data;
			next->lru.prev = node->lru.prev;
		}
		
		aal_list_remove(node->lru.prev, node);

		node->lru.prev = lru->list;
		node->lru.next = lru->list->next;

		aal_list_append(lru->list, node);

		if (node->lru.prev) {
			prev = (reiser4_node_t *)node->lru.prev->data;
			prev->lru.next = lru->list->next;
		}

		if (node->lru.next) {
			next = (reiser4_node_t *)node->lru.next->data;
			next->lru.prev = lru->list->next;
		}
	}

	return 0;
}
