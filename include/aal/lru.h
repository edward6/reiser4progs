/*
  lru.h -- list of recently used objects implementation.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_LRU_H
#define AAL_LRU_H

#ifndef ENABLE_ALONE

#include <aal/types.h>

extern void aal_lru_free(aal_lru_t *lru);
extern errno_t aal_lru_adjust(aal_lru_t *lru);
extern aal_lru_t *aal_lru_create(lru_ops_t *ops);

extern errno_t aal_lru_init(aal_lru_t *lru);
extern void aal_lru_fini(aal_lru_t *lru);

extern errno_t aal_lru_attach(aal_lru_t *lru, void *data);
extern errno_t aal_lru_detach(aal_lru_t *lru, void *data);
extern errno_t aal_lru_touch(aal_lru_t *lru, void *data);

#endif

#endif
