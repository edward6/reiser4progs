/*
  coord.c -- reiser4 tree coord functions. Coord contains full information
  about smaller tree element position in the tree. The instance of structure 
  reiser4_coord_t contains pointer to node where needed unit or item lies,
  item position and unit position in specified item. 
  Copyright (C) 1996-2002 Hans Reiser.
*/

#include <reiser4/reiser4.h>

/* Initializes reiser4_pos_t struct */
inline void reiser4_pos_init(
	reiser4_pos_t *pos,	/* pos to be initialized */
	uint32_t item,		/* item number */
	uint32_t unit)		/* unit number */
{
	aal_assert("umka-955", pos != NULL, return);
	pos->item = item;
	pos->unit = unit;
}

/* Creates coord instance based on passed joint, item pos and unit pos params */
reiser4_coord_t *reiser4_coord_create(
	reiser4_joint_t *joint,	/* the first component of coord */
	uint32_t item,		/* the second one */
	uint32_t unit)		/* the third one */
{
	reiser4_coord_t *coord;

	/* Allocating memory for instance of coord */
	if (!(coord = aal_calloc(sizeof(*coord), 0)))
		return NULL;

	/* Initializing needed fields */
	reiser4_coord_init(coord, joint, item, unit);
    
	return coord;
}

/* This function initializes passed coord by specified params */
errno_t reiser4_coord_init(
	reiser4_coord_t *coord,	/* coord to be initialized */
	reiser4_joint_t *joint,	/* the first component of coord */
	uint32_t item,		/* the second one */
	uint32_t unit)		/* the third one */
{
	aal_assert("umka-795", coord != NULL, return -1);
    
	coord->joint = joint;
	coord->pos.item = item;
	coord->pos.unit = unit;

	return 0;
}

/* Makes duplicate of the passed @coord */
errno_t reiser4_coord_dup(
	reiser4_coord_t *coord,	/* coord to be duplicated */
	reiser4_coord_t *dup)	/* the clone will be saved */
{
	aal_assert("umka-1264", coord != NULL, return -1);
	aal_assert("umka-1265", dup != NULL, return -1);

	*dup = *coord;
	return 0;
}

/* Freeing passed coord */
void reiser4_coord_close(
	reiser4_coord_t *coord)	/* coord to be freed */
{
	aal_assert("umka-793", coord != NULL, return);
	aal_free(coord);
}

