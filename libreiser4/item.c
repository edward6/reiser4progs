/*
  item.c -- common reiser4 item functions.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/* Returns count of units in item. If count method is not implemented,
 * it returns 1 */
uint32_t reiser4_item_count(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-1030", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1448", item->plugin != NULL, return 0);
	
	if (item->plugin->item_ops.count)
		return item->plugin->item_ops.count(item);

	return 1;
}

#ifndef ENABLE_COMPACT

/*
  We can estimate size for insertion and for pasting of hint->data (to be
  memcpy) or of item_info->info (data to be created on the base of).
    
  1. Insertion of data: 
  a) pos->unit == ~0ul 
  b) hint->data != NULL
  c) get hint->plugin on the base of pos.
    
  2. Insertion of info: 
  a) pos->unit == ~0ul 
  b) hint->hint != NULL
  c) hint->plugin != NULL
    
  3. Pasting of data: 
  a) pos->unit != ~0ul 
  b) hint->data != NULL
  c) get hint->plugin on the base of pos.
    
  4. Pasting of info: 
  a) pos->unit_pos != ~0ul 
  b) hint->hint != NULL
  c) get hint->plugin on the base of pos.
*/
errno_t reiser4_item_estimate(
	reiser4_coord_t *coord,	   /* item we will work with */
	reiser4_item_hint_t *hint) /* item hint to be estimated */
{
	aal_assert("vpf-106", coord != NULL, return -1);
	aal_assert("umka-541", hint != NULL, return -1);

	/* We must have hint->plugin initialized for the 2nd case */
	aal_assert("vpf-118", coord->pos.unit != ~0ul || 
		   hint->plugin != NULL, return -1);
   
	/* Here hint has been already set for the 3rd case */
	if (hint->data != NULL)
		return 0;
    
	/* Estimate for the 2nd and for the 4th cases */
	return plugin_call(return -1, hint->plugin->item_ops, 
			   estimate, NULL, coord->pos.unit, hint);
}

/* Prints passed @coord into passed @buff */
errno_t reiser4_item_print(
	reiser4_coord_t *coord,    /* item to be printed */
	char *buff,                /* buffer item to be printed in */
	uint32_t n)                /* buffer size */
{
	item_entity_t *item;
	
	aal_assert("umka-1297", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1449", item->plugin != NULL, return 0);

	if (!item->plugin->item_ops.print) {
		aal_exception_warn("Method \"print\" is not implemented in %s.",
				   item->plugin->h.label);
		return -1;
	}
	
	return item->plugin->item_ops.print(item, buff, n, 0);
}

#endif

int reiser4_item_permissn(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-1100", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1450", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == PERMISSN_ITEM;
}

int reiser4_item_tail(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-1098", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1451", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == TAIL_ITEM;
}

int reiser4_item_extent(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("vpf-238", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1452", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == EXTENT_ITEM;
}

int reiser4_item_direntry(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-1096", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1453", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == DIRENTRY_ITEM;
}

int reiser4_item_statdata(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-1094", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1454", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == STATDATA_ITEM;
}

int reiser4_item_nodeptr(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("vpf-042", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1455", item->plugin != NULL, return 0);
	
	return item->plugin->h.sign.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.sign.group == NODEPTR_ITEM;
}

uint32_t reiser4_item_len(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-760", coord != NULL, return 0);

	item = &coord->entity;
	aal_assert("umka-1460", item->plugin != NULL, return 0);
	
	return item->len;
}

reiser4_body_t *reiser4_item_body(reiser4_coord_t *coord) {
	item_entity_t *item;
	
	aal_assert("umka-554", coord != NULL, return NULL);
    
	item = &coord->entity;
	aal_assert("umka-1461", item->plugin != NULL, return 0);
	
	return item->body;
}

reiser4_plugin_t *reiser4_item_plugin(reiser4_coord_t *coord) {
	aal_assert("umka-755", coord != NULL, return NULL);
	return coord->entity.plugin;
}

errno_t reiser4_item_key(reiser4_coord_t *coord, reiser4_key_t *key) {
	item_entity_t *item;
	
	aal_assert("umka-1215", coord != NULL, return -1);
	aal_assert("umka-1271", key != NULL, return -1);

	item = &coord->entity;
	aal_assert("umka-1462", item->plugin != NULL, return 0);

	aal_memcpy(key, &item->key, sizeof(*key));
	
	return 0;
}

#ifndef ENABLE_COMPACT

errno_t reiser4_item_update(reiser4_coord_t *coord, reiser4_key_t *key) {
	object_entity_t *entity;
	
	aal_assert("umka-1403", coord != NULL, return -1);
	aal_assert("umka-1404", key != NULL, return -1);

	if (!(entity = reiser4_coord_entity(coord)))
		return -1;
	
	return plugin_call(return -1, entity->plugin->node_ops, 
			   set_key, entity, &coord->pos, key);
}

#endif

errno_t reiser4_item_max_poss_key(reiser4_coord_t *coord, reiser4_key_t *key) {
	item_entity_t *entity;
	
	aal_assert("umka-1269", coord != NULL, return -1);
	aal_assert("umka-1270", key != NULL, return -1);

	entity = &coord->entity;
	aal_assert("umka-1456", entity->plugin != NULL, return 0);
	
	return plugin_call(return -1, entity->plugin->item_ops, 
			   max_poss_key, entity, key);
}

errno_t reiser4_item_max_real_key(reiser4_coord_t *coord, reiser4_key_t *key) {
	item_entity_t *entity;
	
	aal_assert("vpf-351", coord != NULL, return -1);
	aal_assert("vpf-352", key != NULL, return -1);
    
	entity = &coord->entity;
	aal_assert("umka-1457", entity->plugin != NULL, return 0);
	
	return plugin_call(return -1, entity->plugin->item_ops,
			   max_real_key, entity, key);
}
