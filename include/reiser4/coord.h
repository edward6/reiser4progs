/*
  coord.h -- reiser4 tree coord functions.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef COORD_H
#define COORD_H

#include <reiser4/filesystem.h>

extern blk_t reiser4_coord_blk(reiser4_coord_t *coord);
extern errno_t reiser4_coord_realize(reiser4_coord_t *coord);
extern aal_device_t *reiser4_coord_device(reiser4_coord_t *coord);

extern errno_t reiser4_coord_init(reiser4_coord_t *coord,
				  reiser4_node_t *node,
				  reiser4_pos_t *pos);

extern errno_t reiser4_coord_open(reiser4_coord_t *coord,
				  reiser4_node_t *node,
				  reiser4_pos_t *pos);

#endif
