/*
  coord.h -- reiser4 tree coord functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_COORD_H
#define REISER4_COORD_H

#include <reiser4/types.h>

extern blk_t reiser4_coord_block(reiser4_coord_t *coord);
extern errno_t reiser4_coord_realize(reiser4_coord_t *coord);
extern aal_device_t *reiser4_coord_device(reiser4_coord_t *coord);

extern errno_t reiser4_coord_assign(reiser4_coord_t *coord,
					     reiser4_node_t *node);

extern errno_t reiser4_coord_init(reiser4_coord_t *coord,
					   reiser4_node_t *node,
					   rpos_t *pos);

extern errno_t reiser4_coord_open(reiser4_coord_t *coord,
					   reiser4_node_t *node,
					   rpos_t *pos);

#endif
