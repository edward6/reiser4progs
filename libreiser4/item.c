/*
    item.c -- common reiser4 item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

errno_t reiser4_item_open(reiser4_item_t *item, 
    reiser4_node_t *node, reiser4_pos_t *pos)
{
    rpid_t pid;
    
    aal_assert("umka-1063", node != NULL, return -1);
    aal_assert("umka-1066", pos != NULL, return -1);
    aal_assert("umka-1064", item != NULL, return -1);
    
    pid = plugin_call(return 0, 
	node->entity->plugin->node_ops, item_pid, node->entity, pos);
    
    if (pid == FAKE_PLUGIN) {
        aal_exception_error("Invalid item plugin id detected. Node %llu, item %u.", 
	    aal_block_number(node->block), pos->item);
	return -1;
    }
    
    item->plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid);
    
    if (!item->plugin) {
        aal_exception_error("Can't get item plugin. Node %llu, item %u.", 
	    aal_block_number(node->block), pos->item);
	return -1;
    }
    
    item->node = node->entity;
    item->pos = pos;

    return 0;
}

errno_t reiser4_item_init(reiser4_item_t *item, 
    reiser4_node_t *node, reiser4_pos_t *pos) 
{
    aal_assert("umka-1060", node != NULL, return -1);
    aal_assert("umka-1067", pos != NULL, return -1);
    
    item->node = node->entity;
    item->pos = pos;
    
    return 0;
}

/* Returns count of units in item */
uint32_t reiser4_item_count(reiser4_item_t *item) {
    aal_assert("umka-1030", item != NULL, return 0);
    aal_assert("umka-1068", item->plugin != NULL, return 0);
    
    if (item->plugin->item_ops.count)
	return item->plugin->item_ops.count(item);

    return 1;
}

#ifndef ENABLE_COMPACT

/*
    We can estimate size for insertion and for pasting of hint->data (to be memcpy) 
    or of item_info->info (data to be created on the base of).
    
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
    reiser4_item_t *item,	/* item we will work with */
    reiser4_item_hint_t *hint	/* item hint to be estimated */
) {
    aal_assert("vpf-106", item != NULL, return -1);
    aal_assert("umka-541", hint != NULL, return -1);

    /* We must have hint->plugin initialized for the 2nd case */
    aal_assert("vpf-118", item->pos->unit != ~0ul || 
	hint->plugin != NULL, return -1);
   
    /* Here hint has been already set for the 3rd case */
    if (hint->data != NULL)
	return 0;
    
    /* Estimate for the 2nd and for the 4th cases */
    return plugin_call(return -1, hint->plugin->item_ops, 
	estimate, item, item->pos->unit, hint);
}

#endif

errno_t reiser4_item_print(reiser4_item_t *item, char *buff, uint32_t n) {
    aal_assert("umka-1297", item != NULL, return 0);
    aal_assert("umka-1298", item->plugin != NULL, return 0);

    return plugin_call(return -1, item->plugin->item_ops, print,
	item, buff, n, 0);
}

/* Returns object plugin id */
uint16_t reiser4_item_detect(reiser4_item_t *item) {
    aal_assert("umka-1294", item != NULL, return 0);
    aal_assert("umka-1295", item->plugin != NULL, return 0);
    
    if (!item->plugin->item_ops.detect)
	return FAKE_PLUGIN;

    return item->plugin->item_ops.detect(item);
}

int reiser4_item_permissn(reiser4_item_t *item) {
    aal_assert("umka-1100", item != NULL, return 0);
    aal_assert("umka-1101", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == PERMISSN_ITEM_TYPE;
}

int reiser4_item_tail(reiser4_item_t *item) {
    aal_assert("umka-1098", item != NULL, return 0);
    aal_assert("umka-1099", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == TAIL_ITEM_TYPE;
}

int reiser4_item_extent(reiser4_item_t *item) {
    aal_assert("vpf-238", item != NULL, return 0);
    aal_assert("vpf-239", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == EXTENT_ITEM_TYPE;
}

int reiser4_item_direntry(reiser4_item_t *item) {
    aal_assert("umka-1096", item != NULL, return 0);
    aal_assert("umka-1097", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == DIRENTRY_ITEM_TYPE;
}

int reiser4_item_statdata(reiser4_item_t *item) {
    aal_assert("umka-1094", item != NULL, return 0);
    aal_assert("umka-1095", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == STATDATA_ITEM_TYPE;
}

int reiser4_item_internal(reiser4_item_t *item) {
    aal_assert("vpf-042", item != NULL, return 0);
    aal_assert("umka-1072", item->plugin != NULL, return 0);

    return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
	item->plugin->item_ops.type == INTERNAL_ITEM_TYPE;
}

uint32_t reiser4_item_len(reiser4_item_t *item) {
    aal_assert("umka-760", item != NULL, return 0);
    
    return plugin_call(return 0, item->node->plugin->node_ops, 
	item_len, item->node, item->pos);
}

reiser4_body_t *reiser4_item_body(reiser4_item_t *item) {
    aal_assert("umka-554", item != NULL, return NULL);
    
    return plugin_call(return NULL, item->node->plugin->node_ops, 
	item_body, item->node, item->pos);
}

errno_t reiser4_item_key(reiser4_item_t *item, reiser4_key_t *key) {
    aal_assert("umka-1215", item != NULL, return -1);
    aal_assert("umka-1271", key != NULL, return -1);
    
    return plugin_call(return -1, item->node->plugin->node_ops, 
	get_key, item->node, item->pos, key);
}

reiser4_plugin_t *reiser4_item_plugin(reiser4_item_t *item) {
    aal_assert("umka-755", item != NULL, return NULL);
    return item->plugin;
}

errno_t reiser4_item_max_poss_key(reiser4_item_t *item, reiser4_key_t *key) {
    aal_assert("umka-1269", item != NULL, return -1);
    aal_assert("umka-1270", key != NULL, return -1);
    
    return plugin_call(return -1, item->plugin->item_ops, 
	max_poss_key, item, key);
}

errno_t reiser4_item_max_real_key(reiser4_item_t *item, reiser4_key_t *key) {
    aal_assert("vpf-351", item != NULL, return -1);
    aal_assert("vpf-352", key != NULL, return -1);
    
    return plugin_call(return -1, item->plugin->item_ops, max_real_key, 
	item, key);
}
