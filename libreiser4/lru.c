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
#  include <sys/vfs.h>
#endif

#include <reiser4/reiser4.h>

#ifndef ENABLE_COMPACT

static int check_lru = 0;

static void callback_check_lru(int dummy) {
	check_lru = 1;
	alarm(1);
}

/* Returns true in the case we should adjust lru due to memory pressure */
static int reiser4_lru_nomem(reiser4_lru_t *lru) {
	FILE *f;
	
	int dint, res;
	long rss, vmsize;

	long dlong;
	char cmd[256], dchar;

	uint32_t diff;

	aal_assert("umka-1524", lru != NULL, return -1);
	
	if (!(f = fopen("/proc/self/stat", "r"))) {
		aal_exception_error("Can't open /proc/self/stat.");
		return 0;
	}

	fscanf(f, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu "
	       "%ld %ld %ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu "
	       "%lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu\n",
	       &dint, cmd, &dchar, &dint, &dint, &dint, &dint, &dint, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &vmsize, &rss, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dint, &dint, &dlong, &dlong);
	
	diff = labs((vmsize - (rss << 12)) - lru->swaped);

	if (!(res = diff > 4096))
		lru->adjust = 0;

	lru->swaped = vmsize - (rss << 12);

	fclose(f);
	
	return res;
}

#endif

errno_t reiser4_lru_adjust(reiser4_lru_t *lru) {
	aal_list_t *curr;
	reiser4_joint_t *joint;
	
	aal_assert("umka-1519", lru != NULL, return -1);

	if (!lru->adjustable)
		lru->adjust = 1;
	
	aal_exception_info("Shrinking cache on %u nodes.", lru->adjust);
	
	if (!(curr = aal_list_last(lru->list)))
		return 0;
	
	while (curr && lru->adjust-- > 0) {
		joint = (reiser4_joint_t *)curr->data;
		curr = curr->prev;
		
		if (joint->counter == 0) {
			
			if (!joint->parent || joint->children)
				continue;

			reiser4_joint_detach(joint->parent, joint);
				
/*			if (joint->flags & JF_DIRTY)
				reiser4_joint_sync(joint);*/
			
			reiser4_joint_close(joint);
		}
	}

	return 0;
}

errno_t reiser4_lru_init(reiser4_lru_t *lru) {

#ifndef ENABLE_COMPACT
	struct statfs fs_st;
	struct sigaction new, old;
#endif

	aal_memset(lru, 0, sizeof(*lru));

#ifndef ENABLE_COMPACT
	lru->adjustable = statfs("/proc", &fs_st) != -1 &&
		fs_st.f_type == 0x9fa0;

	reiser4_lru_nomem(lru);

	if (lru->adjustable) {
		new.sa_flags = 0;
		new.sa_handler = callback_check_lru;

		sigaction(SIGALRM, &new, &old);
		alarm(1);
	}
	
#else
	lru->adjustable = 0;
#endif
	
	return 0;
}

void reiser4_lru_fini(reiser4_lru_t *lru) {
	aal_assert("umka-1531", lru != NULL, return);

	if (lru->list)
		aal_list_free(lru->list);
}

errno_t reiser4_lru_attach(reiser4_lru_t *lru, reiser4_joint_t *joint) {
	reiser4_joint_t *next, *prev;
	
	aal_assert("umka-1525", lru != NULL, return -1);
	aal_assert("umka-1526", joint != NULL, return -1);
	
	if (check_lru) {
		int do_adjust = lru->list != NULL;
		
#ifndef ENABLE_COMPACT
		do_adjust = do_adjust && reiser4_lru_nomem(lru);
#endif
		if (do_adjust && lru->adjust) {
			if (reiser4_lru_adjust(lru)) {
				aal_exception_error("Can't adjust tree lru.");
				return -1;
			}
		}
		
		check_lru = !check_lru;
	}
		
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
