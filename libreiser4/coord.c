/*
  coord.c -- reiser4 tree coord functions. Coord contains full information about
  tree element position in the tree. The instance of structure reiser4_coord_t
  contains pointer to node where needed unit or item lies, item position and
  unit position in specified item.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

/* Returns entity from passed @coord */
object_entity_t *reiser4_coord_entity(reiser4_coord_t *coord) {
	aal_assert("umka-1434", coord != NULL, return NULL);
	
	switch (coord->context) {
	case CT_ENTITY:
		return coord->u.entity;
	case CT_NODE:
		return coord->u.node->entity;
	case CT_JOINT:
		return coord->u.joint->node->entity;
	case CT_RAW:
		return coord->u.data;
	default:
		return NULL;
	}
}

/* Returns block coord points to */
aal_block_t *reiser4_coord_block(reiser4_coord_t *coord) {
	aal_assert("umka-1445", coord != NULL, return NULL);
	
	switch (coord->context) {
	case CT_NODE:
		return coord->u.node->block;
	case CT_JOINT:
		return coord->u.joint->node->block;
	default:
		return NULL;
	}
}

/* Returns block coord points to */
reiser4_node_t *reiser4_coord_node(reiser4_coord_t *coord) {
	aal_assert("umka-1463", coord != NULL, return NULL);
	
	switch (coord->context) {
	case CT_NODE:
		return coord->u.node;
	case CT_JOINT:
		return coord->u.joint->node;
	default:
		return NULL;
	}
}

/* Initializes all item-related fields */
errno_t reiser4_coord_realize(reiser4_coord_t *coord) {
	rpid_t pid;
	reiser4_key_t *key;
	item_context_t *context;
	object_entity_t *entity;
	
        aal_assert("umka-1459", coord != NULL, return -1);

	if (!(entity = reiser4_coord_entity(coord))) {
		aal_exception_error("Invalid coord context. Can't get coord entity.");
		return -1;
	}
	
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
	context->block = reiser4_coord_block(coord);
	context->node = reiser4_coord_entity(coord);
	
	return 0;
}

/* Initializes coord and its item related fields */
errno_t reiser4_coord_open(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	void *data,	         /* the first component of coord */
	coord_context_t context, /* coord context */
	reiser4_pos_t *pos)	 /* coord pos component */
{
        aal_assert("umka-1435", coord != NULL, return -1);

	if (reiser4_coord_init(coord, data, context, pos))
		return -1;
	
	return reiser4_coord_realize(coord);
}

/* This function initializes passed coord by specified params */
errno_t reiser4_coord_init(
	reiser4_coord_t *coord,	 /* coord to be initialized */
	void *data,	         /* the first component of coord */
	coord_context_t context, /* coord context */
	reiser4_pos_t *pos)	 /* coord pos component */
{
	aal_assert("umka-795", coord != NULL, return -1);
    
	coord->u.data = data;
	coord->context = context;
	coord->pos = *pos;

	return 0;
}
