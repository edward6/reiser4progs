/*
    coord.h -- reiser4 tree coord functions.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef COORD_H
#define COORD_H

#include <reiser4/filesystem.h>

extern inline void reiser4_pos_init(reiser4_pos_t *pos,
    uint32_t item, uint32_t unit);

extern reiser4_coord_t *reiser4_coord_create(reiser4_joint_t *joint, 
    uint32_t item, uint32_t unit);

extern errno_t reiser4_coord_init(reiser4_coord_t *coord, 
    reiser4_joint_t *joint, uint32_t item, uint32_t unit);

extern errno_t reiser4_coord_dup(reiser4_coord_t *coord,
    reiser4_coord_t *dup);

extern void reiser4_coord_close(reiser4_coord_t *coord);

#endif

