/*
  coord.h -- reiser4 tree coord functions.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef COORD_H
#define COORD_H

#include <reiser4/filesystem.h>

extern errno_t reiser4_coord_realize(reiser4_coord_t *coord);

extern aal_block_t *reiser4_coord_block(reiser4_coord_t *coord);

extern reiser4_node_t *reiser4_coord_node(reiser4_coord_t *coord);

extern object_entity_t *reiser4_coord_entity(reiser4_coord_t *coord);

extern errno_t reiser4_coord_init(reiser4_coord_t *coord, void *data,
				  coord_context_t context, reiser4_pos_t *pos);

extern errno_t reiser4_coord_open(reiser4_coord_t *coord, void *data,
				  coord_context_t context, reiser4_pos_t *pos);

#endif
