/*
  lru.c -- list of recently used objects implementation.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

errno_t aal_lru_adjust(aal_lru_t *lru) {
	void *data;
	aal_list_t *walk;
	
	aal_assert("umka-1519", lru != NULL);

	if (!(walk = aal_list_last(lru->list)))
		return 0;

	while (walk && lru->adjust > 0) {
		data = walk->data;
		walk = walk->prev;

#ifndef ENABLE_STAND_ALONE
		if (lru->ops->sync) {
			if (!lru->ops->sync(data))
				continue;
		}
#endif

		lru->ops->free(data);
	}

	return 0;
}

aal_lru_t *aal_lru_create(lru_ops_t *ops) {
	aal_lru_t *lru;

	if (!(lru = aal_calloc(sizeof(*lru), 0)))
		return NULL;

	lru->ops = ops;
	
	return lru;
}

void aal_lru_free(aal_lru_t *lru) {
	aal_assert("umka-1531", lru != NULL);

	if (lru->list)
		aal_list_free(lru->list);

	aal_free(lru);
}

errno_t aal_lru_attach(aal_lru_t *lru, void *data) {
	aal_list_t *prev, *next;
	
	aal_assert("umka-1525", lru != NULL);
	aal_assert("umka-1526", data != NULL);

	lru->ops->set_prev(data, lru->list);

	lru->ops->set_next(data, lru->list ?
			   lru->list->next : NULL);
	
	lru->list = aal_list_append(lru->list, data);

	if ((prev = lru->ops->get_prev(data)))
		lru->ops->set_next(prev->data, lru->list->next);

	if ((next = lru->ops->get_next(data)))
		lru->ops->set_prev(next->data, lru->list->next);

	lru->adjust++;

	return 0;
}

errno_t aal_lru_detach(aal_lru_t *lru, void *data) {
	aal_list_t *next, *prev;
	
	aal_assert("umka-1528", lru != NULL);
	aal_assert("umka-1527", data != NULL);

	next = lru->ops->get_next(data);
	prev = lru->ops->get_prev(data);
	
	if (prev)
		lru->ops->set_next(prev->data, next);
	
	if (next)
		lru->ops->set_prev(next->data, prev);
	
	if (prev)
		aal_list_remove(prev, data);
	else
		lru->list = aal_list_remove(lru->list, data);

	if (lru->adjust > 0)
		lru->adjust--;
	
	return 0;
}

errno_t aal_lru_touch(aal_lru_t *lru, void *data) {
	aal_list_t *next, *prev;
	
	aal_assert("umka-1529", lru != NULL);
	aal_assert("umka-1530", data != NULL);

	aal_lru_detach(lru, data);
	return aal_lru_attach(lru, data);
}
