/*
  coord.c -- reiser4 tree coord functions. Coord contains full information about
  tree element position in the tree. The instance of structure reiser4_coord_t
  contains pointer to node where needed unit or item lies, item position and
  unit position in specified item.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

blk_t reiser4_coord_block(reiser4_coord_t *coord) {
	aal_assert("umka-1445", coord != NULL);
	return coord->node->blk;
}

aal_device_t *reiser4_coord_device(reiser4_coord_t *coord) {
	aal_assert("umka-1553", coord != NULL);
	return coord->node->device;
}

/* Initializes all item-related fields */
errno_t reiser4_coord_realize(reiser4_coord_t *coord) {
	rpid_t pid;
	reiser4_key_t *key;
	item_entity_t *item;
	object_entity_t *entity;
	
        aal_assert("umka-1459", coord != NULL);

	entity = coord->node->entity;
	
	if ((pid = plugin_call(entity->plugin->node_ops, item_pid,
			       entity, &coord->pos)) == INVAL_PID)
	{
		aal_exception_error("Invalid item plugin id has been "
				    "detected.");
		return -1;
	}

	item = &coord->item;
	
	if (!(item->plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id "
				    "0x%x.", pid);
		return -1;
	}

	if (!(item->body = plugin_call(entity->plugin->node_ops, item_body,
				       entity, &coord->pos)))
	{
		aal_exception_error("Can't get item body.");
		return -1;
	}

	/* Initializing item entity fields */
	item->pos = coord->pos;
	
	item->len = plugin_call(entity->plugin->node_ops, item_len,
				entity, &coord->pos);

	/* Initializing item context fields */
	item->con.blk = reiser4_coord_block(coord);
	item->con.device = reiser4_coord_device(coord);
	
	/* Initializing item enviromnent fields */
	if (coord->node->tree) {
		item->env.oid = coord->node->tree->fs->oid->entity;
		item->env.alloc = coord->node->tree->fs->alloc->entity;
	}
		
	return 0;
}

/* This function initializes passed coord by specified params */
errno_t reiser4_coord_init(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	reiser4_node_t *node,	 /* the first component of coord */
	rpos_t *pos)	         /* coord pos component */
{
	aal_assert("umka-795", coord != NULL);
	aal_assert("umka-1728", node != NULL);
    
	coord->node = node;

	if (pos != NULL)
		coord->pos = *pos;

	return 0;
}

/* Initializes coord and its item related fields */
errno_t reiser4_coord_open(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	reiser4_node_t *node,	 /* the first component of coord */
	rpos_t *pos)	         /* coord pos component */
{
        aal_assert("umka-1435", coord != NULL);

	if (reiser4_coord_init(coord, node, pos))
		return -1;

	return reiser4_coord_realize(coord);
}

errno_t reiser4_coord_assign(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	reiser4_node_t *node)	 /* the node to be assigned */
{
        aal_assert("umka-1730", coord != NULL);
	
	aal_memset(coord, 0, sizeof(*coord));
	return reiser4_coord_init(coord, node, NULL);
}
