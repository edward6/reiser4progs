/*
  lru.c -- list of recently used nodes implementation..
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef LRU_H
#define LRU_H

extern errno_t reiser4_lru_adjust(reiser4_lru_t *lru);
extern errno_t reiser4_lru_init(reiser4_lru_t *lru);
extern void reiser4_lru_fini(reiser4_lru_t *lru);

extern errno_t reiser4_lru_attach(reiser4_lru_t *lru,
				  reiser4_node_t *node);

extern errno_t reiser4_lru_detach(reiser4_lru_t *lru,
				  reiser4_node_t *node);

extern errno_t reiser4_lru_touch(reiser4_lru_t *lru,
				 reiser4_node_t *node);

#endif
