/*
  lru.h -- list of recently used objects implementation.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_LRU_H
#define AAL_LRU_H

#include <aal/list.h>

struct lru_ops {
	int (*free) (void *);
	int (*sync) (void *);

	aal_list_t *(*get_next) (void *);
	void (*set_next) (void *, aal_list_t *);
	
	aal_list_t *(*get_prev) (void *);
	void (*set_prev) (void *, aal_list_t *);
};

typedef struct lru_ops lru_ops_t;

struct aal_lru {
	uint32_t adjust;
	aal_list_t *list;
	lru_ops_t *ops;
};

typedef struct aal_lru aal_lru_t;

extern void aal_lru_free(aal_lru_t *lru);
extern errno_t aal_lru_adjust(aal_lru_t *lru);
extern aal_lru_t *aal_lru_create(lru_ops_t *ops);

extern errno_t aal_lru_init(aal_lru_t *lru);
extern void aal_lru_fini(aal_lru_t *lru);

extern errno_t aal_lru_attach(aal_lru_t *lru, void *data);
extern errno_t aal_lru_detach(aal_lru_t *lru, void *data);
extern errno_t aal_lru_touch(aal_lru_t *lru, void *data);

#endif
