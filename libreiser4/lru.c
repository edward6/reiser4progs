/*
  lru.c -- list of recently used nodes implementation..
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <unistd.h>
#  include <signal.h>
#  include <math.h>
#  include <sys/vfs.h>
#endif

#include <reiser4/reiser4.h>

errno_t reiser4_lru_adjust(reiser4_lru_t *lru) {
	aal_list_t *curr;
	reiser4_joint_t *joint;
	
	aal_assert("umka-1519", lru != NULL, return -1);

	aal_exception_info("Shrinking tree cache on %u nodes.",
			   lru->adjust);
	
	if (!lru->adjustable)
		lru->adjust = 1;
	
	if (!(curr = aal_list_last(lru->list)))
		return 0;

	aal_mpressure_disable(lru->mpressure);
	
	while (curr && lru->adjust > 0) {
		joint = (reiser4_joint_t *)curr->data;
		curr = curr->prev;
		
		if (joint->counter == 0) {
			
			if (!joint->parent || joint->children)
				continue;

/*			if (joint->flags & JF_DIRTY)
				reiser4_joint_sync(joint);*/
			
			reiser4_joint_close(joint);
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

errno_t reiser4_lru_attach(reiser4_lru_t *lru, reiser4_joint_t *joint) {
	reiser4_joint_t *next, *prev;
	
	aal_assert("umka-1525", lru != NULL, return -1);
	aal_assert("umka-1526", joint != NULL, return -1);

	if (aal_mpressure_check())
		return -1;
	
	joint->prev = lru->list;
	joint->next = lru->list ? lru->list->next : NULL;

	lru->list = aal_list_append(lru->list, joint);
	lru->adjust++;

	if (joint->prev) {
		prev = (reiser4_joint_t *)joint->prev->data;
		prev->next = lru->list->next;
	}

	if (joint->next) {
		next = (reiser4_joint_t *)joint->next->data;
		next->prev = lru->list->next;
	}

	return 0;
}

errno_t reiser4_lru_detach(reiser4_lru_t *lru, reiser4_joint_t *joint) {
	reiser4_joint_t *next, *prev;
	
	aal_assert("umka-1528", lru != NULL, return -1);
	aal_assert("umka-1527", joint != NULL, return -1);

	if (!joint->prev && joint->next)
		return 0;
	
	if (joint->prev) {
		prev = (reiser4_joint_t *)joint->prev->data;
		prev->next = joint->next;
	}
	
	if (joint->next) {
		next = (reiser4_joint_t *)joint->next->data;
		next->prev = joint->prev;
	}
	
	if (joint->prev)
		aal_list_remove(joint->prev, joint);
	else
		lru->list = aal_list_remove(lru->list, joint);

	if (lru->adjust > 0)
		lru->adjust--;
	
	return 0;
}

errno_t reiser4_lru_touch(reiser4_lru_t *lru, reiser4_joint_t *joint) {
	reiser4_joint_t *next, *prev;
	
	aal_assert("umka-1529", lru != NULL, return -1);
	aal_assert("umka-1530", joint != NULL, return -1);

	if (lru->list && joint->prev) {
		prev = (reiser4_joint_t *)joint->prev->data;
		prev->next = joint->next;

		if (joint->next) {
			next = (reiser4_joint_t *)joint->next->data;
			next->prev = joint->prev;
		}
		
		aal_list_remove(joint->prev, joint);

		joint->prev = lru->list;
		joint->next = lru->list->next;

		aal_list_append(lru->list, joint);

		if (joint->prev) {
			prev = (reiser4_joint_t *)joint->prev->data;
			prev->next = lru->list->next;
		}

		if (joint->next) {
			next = (reiser4_joint_t *)joint->next->data;
			next->prev = lru->list->next;
		}
	}

	return 0;
}
