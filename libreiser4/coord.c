/*
  coord.c -- reiser4 tree coord functions. Coord contains full information about
  tree element position in the tree. The instance of structure reiser4_coord_t
  contains pointer to node where needed unit or item lies, item position and
  unit position in specified item.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

blk_t reiser4_coord_blk(reiser4_coord_t *coord) {
	aal_assert("umka-1445", coord != NULL, return FAKE_BLK);
	return coord->node->blk;
}

aal_device_t *reiser4_coord_device(reiser4_coord_t *coord) {
	aal_assert("umka-1553", coord != NULL, return NULL);
	return coord->node->device;
}

/* Initializes all item-related fields */
errno_t reiser4_coord_realize(reiser4_coord_t *coord) {
	rpid_t pid;
	reiser4_key_t *key;
	item_context_t *context;
	object_entity_t *entity;
	
        aal_assert("umka-1459", coord != NULL, return -1);

	entity = coord->node->entity;
	
	if ((pid = plugin_call(return -1, entity->plugin->node_ops,
			       item_pid, entity, &coord->pos)) == FAKE_PLUGIN)
	{
		aal_exception_error("Invalid item plugin id has been detected.");
		return -1;
	}
	
	if (!(coord->entity.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
		return -1;
	}

	if (!(coord->entity.body = plugin_call(return -1, entity->plugin->node_ops,
					       item_body, entity, &coord->pos)))
	{
		aal_exception_error("Can't get item body.");
		return -1;
	}

	coord->entity.pos = coord->pos.item;
	coord->entity.len = plugin_call(return -1, entity->plugin->node_ops,
					item_len, entity, &coord->pos);

	key = &coord->entity.key;
	if (plugin_call(return -1, entity->plugin->node_ops, 
			get_key, entity, &coord->pos, key))
	{
		aal_exception_error("Can't get item key.");
		return -1;
	}

	key->plugin = reiser4_key_guess(key->body);
	aal_assert("umka-1406", key->plugin != NULL, return -1);

	context = &coord->entity.context;
	
	context->node = coord->node->entity;
	context->blk = reiser4_coord_blk(coord);
	context->device = reiser4_coord_device(coord);
	
	return 0;
}

/* Initializes coord and its item related fields */
errno_t reiser4_coord_open(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	reiser4_node_t *node,	 /* the first component of coord */
	reiser4_pos_t *pos)	 /* coord pos component */
{
        aal_assert("umka-1435", coord != NULL, return -1);

	if (reiser4_coord_init(coord, node, pos))
		return -1;
	
	return reiser4_coord_realize(coord);
}

/* This function initializes passed coord by specified params */
errno_t reiser4_coord_init(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	reiser4_node_t *node,	 /* the first component of coord */
	reiser4_pos_t *pos)	 /* coord pos component */
{
	aal_assert("umka-795", coord != NULL, return -1);
    
	coord->node = node;
	coord->pos = *pos;

	return 0;
}
