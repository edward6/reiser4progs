/*
    direntry40.c -- reiser4 default direntry plugin.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#include "direntry40.h"

static reiser4_core_t *core = NULL;

static direntry40_t *direntry40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (direntry40_t *)plugin_call(return NULL, 
	item->node->plugin->node_ops, item_body, item->node, item->pos);
}

static uint32_t direntry40_count(reiser4_item_t *item) {
    direntry40_t *direntry;
    
    aal_assert("umka-865", item != NULL, return 0);

    direntry = direntry40_body(item);
    return de40_get_count(direntry);
}

static errno_t direntry40_entry(reiser4_item_t *item, 
    uint32_t pos, reiser4_entry_hint_t *entry)
{
    entry40_t *en;
    uint32_t offset;
    
    objid40_t *objid;
    direntry40_t *direntry;
    
    aal_assert("umka-866", item != NULL, return -1);
    
    if (!(direntry = direntry40_body(item)))
	return -1;
    
    if (pos > direntry40_count(item))
	return -1;
    
    offset = sizeof(direntry40_t) + pos * sizeof(entry40_t);
    en = (entry40_t *)(((void *)direntry) + offset);
    
    entry->entryid.objectid = eid_get_objectid((entryid40_t *)(&en->entryid));
    entry->entryid.offset = eid_get_offset((entryid40_t *)(&en->entryid));
    
    offset = en40_get_offset(en); 
    objid = (objid40_t *)(((void *)direntry) + offset);
    
    entry->objid.objectid = oid_get_objectid(objid);
    entry->objid.locality = oid_get_locality(objid);

    offset += sizeof(*objid);
    entry->name = ((void *)direntry) + offset;
    
    return 0;
}

#ifndef ENABLE_COMPACT

static errno_t direntry40_estimate(reiser4_item_t *item, 
    uint32_t pos, reiser4_item_hint_t *hint) 
{
    int i;
    reiser4_direntry_hint_t *direntry_hint;
	    
    aal_assert("vpf-095", hint != NULL, return -1);
    
    direntry_hint = (reiser4_direntry_hint_t *)hint->hint;
    hint->len = direntry_hint->count * sizeof(entry40_t);
    
    for (i = 0; i < direntry_hint->count; i++) {
	hint->len += aal_strlen(direntry_hint->entry[i].name) + 
	    sizeof(objid40_t) + 1;
    }

    if (pos == ~0ul)
	hint->len += sizeof(direntry40_t);
    
    return 0;
}

static uint32_t direntry40_entrylen(direntry40_t *direntry, 
    uint32_t pos) 
{
    char *name;
    uint32_t offset;
    
    aal_assert("umka-936", 
	pos < de40_get_count(direntry), return 0);
    
    offset = en40_get_offset(&direntry->entry[pos]);
    name = (char *)(((char *)direntry) + offset + sizeof(objid40_t));
	    
    return (aal_strlen(name) + sizeof(objid40_t) + 1);
}

static errno_t direntry40_insert(reiser4_item_t *item, 
    uint32_t pos, reiser4_item_hint_t *hint)
{
    uint32_t i, offset;
    uint32_t len_before = 0;
    uint32_t len_after = 0;
    
    direntry40_t *direntry;
    reiser4_direntry_hint_t *direntry_hint;
    
    aal_assert("umka-791", item != NULL, return -1);
    aal_assert("umka-792", hint != NULL, return -1);
    aal_assert("umka-897", pos != ~0ul, return -1);

    if (!(direntry = direntry40_body(item)))
	return -1;
    
    direntry_hint = (reiser4_direntry_hint_t *)hint->hint;
    
    if (pos > de40_get_count(direntry))
	return -1;
    
    /* Getting offset area of new entry body will be created at */
    if (de40_get_count(direntry) > 0) {
	if (pos < de40_get_count(direntry)) {
	    offset = en40_get_offset(&direntry->entry[pos]) + 
		(direntry_hint->count * sizeof(entry40_t));
	} else {
	    offset = en40_get_offset(&direntry->entry[de40_get_count(direntry) - 1]);
	    offset += sizeof(entry40_t) + 
		direntry40_entrylen(direntry, de40_get_count(direntry) - 1);
	}
    } else {
	offset = sizeof(direntry40_t) + 
	    direntry_hint->count * sizeof(entry40_t);
    }

    if (direntry40_estimate(item, pos, hint))
	return -1;
    
    /* Calculating length of areas to be moved */
    len_before = (de40_get_count(direntry) - pos)*sizeof(entry40_t);
	
    for (i = 0; i < pos; i++)
	len_before += direntry40_entrylen(direntry, i);
	
    for (i = pos; i < de40_get_count(direntry); i++)
	len_after += direntry40_entrylen(direntry, i);
	
    /* Updating offsets */
    for (i = 0; i < pos; i++) {
	en40_set_offset(&direntry->entry[i], 
	    en40_get_offset(&direntry->entry[i]) + 
	    direntry_hint->count * sizeof(entry40_t));
    }
    
    for (i = pos; i < de40_get_count(direntry); i++) {
	en40_set_offset(&direntry->entry[i], 
	    en40_get_offset(&direntry->entry[i]) + hint->len);
    }
    
    /* Moving unit bodies */
    if (pos < de40_get_count(direntry)) {
	uint32_t headers = (direntry_hint->count * sizeof(entry40_t));
		
	aal_memmove(((char *)direntry) + offset + hint->len - headers, 
	    ((char *)direntry) + offset - headers, len_after + headers);
    }
    
    /* Moving unit headers headers */
    if (len_before) {
	aal_memmove(&direntry->entry[pos] + direntry_hint->count, 
	    &direntry->entry[pos], len_before);
    }
    
    /* Creating new entries */
    for (i = 0; i < direntry_hint->count; i++) {
	en40_set_offset(&direntry->entry[pos + i], offset);

	aal_memcpy(&direntry->entry[pos + i].entryid, 
	    &direntry_hint->entry[i].entryid, sizeof(entryid40_t));
	
	aal_memcpy(((char *)direntry) + offset, 
	    &direntry_hint->entry[i].objid, sizeof(objid40_t));
	
	offset += sizeof(objid40_t);
	
	aal_memcpy((char *)(direntry) + offset, 
	    direntry_hint->entry[i].name, 
	    aal_strlen(direntry_hint->entry[i].name));

	offset += aal_strlen(direntry_hint->entry[i].name);
	*((char *)(direntry) + offset) = '\0';
	offset++;
    }
    
    /* Updating direntry count field */
    de40_set_count(direntry, de40_get_count(direntry) + 
	direntry_hint->count);
    
    return 0;
}

static errno_t direntry40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    direntry40_t *direntry;
    
    aal_assert("umka-1010", item != NULL, return -1);

    if (!(direntry = direntry40_body(item)))
	return -1;
    
    de40_set_count(direntry, 0);
    return direntry40_insert(item, 0, hint);
}

static uint16_t direntry40_remove(reiser4_item_t *item, 
    uint32_t pos)
{
    uint16_t rem_len;
    uint32_t offset;
    uint32_t i, head_len;

    direntry40_t *direntry;
    
    aal_assert("umka-934", item != NULL, return 0);

    if (!(direntry = direntry40_body(item)))
	return -1;
    
    if (pos >= de40_get_count(direntry))
	return 0;

    offset = en40_get_offset(&direntry->entry[pos]);
    
    head_len = offset - sizeof(entry40_t) -
	(((char *)&direntry->entry[pos]) - ((char *)direntry));

    rem_len = direntry40_entrylen(direntry, pos);

    aal_memmove(&direntry->entry[pos], 
	&direntry->entry[pos + 1], head_len);

    for (i = 0; i < pos; i++) {
	en40_set_offset(&direntry->entry[i], 
	    en40_get_offset(&direntry->entry[i]) - sizeof(entry40_t));
    }
    
    if (pos < (uint32_t)de40_get_count(direntry) - 1) {
	uint32_t foot_len = 0;
	
	offset = en40_get_offset(&direntry->entry[pos]);
	
	for (i = pos; i < (uint32_t)de40_get_count(direntry) - 1; i++)
	    foot_len += direntry40_entrylen(direntry, i);
	
	aal_memmove((((char *)direntry) + offset) - (sizeof(entry40_t) +
	    rem_len), ((char *)direntry) + offset, foot_len);

	for (i = pos; i < (uint32_t)de40_get_count(direntry) - 1; i++) {
	    en40_set_offset(&direntry->entry[i], 
		en40_get_offset(&direntry->entry[i]) - 
		(sizeof(entry40_t) + rem_len));
	}
    }
    
    de40_set_count(direntry, de40_get_count(direntry) - 1);
    
    return rem_len + sizeof(entry40_t);
}

extern errno_t direntry40_check(reiser4_item_t *item, 
    uint16_t options);

#endif

static errno_t direntry40_print(reiser4_item_t *item, 
    char *buff, uint32_t n, uint16_t options) 
{
    aal_assert("umka-548", item != NULL, return -1);
    aal_assert("umka-549", buff != NULL, return -1);

    return -1;
}

/* 
    Helper function that is used by lookup method for getting n-th element of 
    direntry.
*/
static inline void *callback_get_entry(void *array, 
    uint32_t pos, void *data) 
{
    direntry40_t *direntry = (direntry40_t *)array;
    return &direntry->entry[pos].entryid;
}

/* 
    Helper function that is used by lookup method for comparing given key with 
    passed dirid.
*/
static inline int callback_comp_entry(
    reiser4_body_t *entryid,	/* entryid passed by binay search */
    reiser4_body_t *lookkey,	/* looked key */
    void *data			/* user-specified data */
) {
    reiser4_key_t entrykey;
    reiser4_plugin_t *plugin;
    reiser4_key_type_t type;

    roid_t locality;
    roid_t objectid;
    uint64_t offset;

    aal_assert("umka-657", entryid != NULL, return -1);
    aal_assert("umka-658", lookkey != NULL, return -1);
    aal_assert("umka-659", data != NULL, return -1);
    
    plugin = (reiser4_plugin_t *)data;
    
    locality = plugin_call(return -1, plugin->key_ops, 
	get_locality, lookkey);

    type = plugin_call(return -1, plugin->key_ops,
        get_type, lookkey);
    
    objectid = *((uint64_t *)entryid);
    offset = *((uint64_t *)entryid + 1);

    plugin_call(return -1, plugin->key_ops,
	build_generic, entrykey.body, type, locality, 
	objectid, offset);
    
    return plugin_call(return -1, plugin->key_ops, 
	compare, entrykey.body, lookkey);
}

static errno_t direntry40_maxkey(reiser4_item_t *item, 
    reiser4_key_t *key) 
{
    uint64_t offset;
    roid_t objectid;
    reiser4_body_t *maxkey;
    
    aal_assert("umka-716", key->plugin != NULL, return -1);

    if (plugin_call(return 0, item->node->plugin->node_ops,
	    get_key, item->node, item->pos, key))
	return -1;
    
    maxkey = plugin_call(return -1, key->plugin->key_ops,
	maximal,);
    
    objectid = plugin_call(return -1, key->plugin->key_ops,
	get_objectid, maxkey);
    
    offset = plugin_call(return -1, key->plugin->key_ops, 
	get_offset, maxkey);
    
    plugin_call(return -1, key->plugin->key_ops, set_objectid, 
	key->body, objectid);
    
    plugin_call(return -1, key->plugin->key_ops, set_offset, 
	key->body, offset);
    
    return 0;
}

static int direntry40_lookup(reiser4_item_t *item, 
    reiser4_key_t *key, uint32_t *pos)
{
    int lookup;
    uint64_t unit;
    
    direntry40_t *direntry;
    reiser4_key_t maxkey, minkey;

    aal_assert("umka-610", key != NULL, return -1);
    aal_assert("umka-717", key->plugin != NULL, return -1);
    
    aal_assert("umka-609", item != NULL, return -1);
    aal_assert("umka-629", pos != NULL, return -1);
    
    if (!(direntry = direntry40_body(item)))
	return -1;
    
    /* FIXME-UMKA: Here should not be hardcoded key40 plugin id */
    maxkey.plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE, KEY_REISER40_ID);
	
    if (direntry40_maxkey(item, &maxkey))
	return -1;
    
    if (plugin_call(return -1, key->plugin->key_ops,
	compare, key->body, maxkey.body) > 0)
    {
	*pos = direntry40_count(item) - 1;
	return 0;
    }
    
    minkey.plugin = maxkey.plugin;
    if (plugin_call(return 0, item->node->plugin->node_ops,
	    get_key, item->node, item->pos, &minkey))
	return -1;
    
    if (plugin_call(return -1, key->plugin->key_ops,
	compare, minkey.body, key->body) > 0)
    {
	*pos = 0;
	return 0;
    }
    
    lookup = reiser4_aux_binsearch((void *)direntry, direntry40_count(item), 
	key->body, callback_get_entry, callback_comp_entry, key->plugin, &unit);

    if (lookup != -1) {
	*pos = (uint32_t)unit;
	if (lookup == 0) (*pos)++;
    }
    
    return lookup;
}

static reiser4_plugin_t direntry40_plugin = {
    .item_ops = {
	.h = {
	    .handle = NULL,
	    .id = ITEM_CDE40_ID,
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "direntry40",
	    .desc = "Compound direntry for reiserfs 4.0, ver. " VERSION,
	},
	.type = DIRENTRY_ITEM_TYPE,
	
#ifndef ENABLE_COMPACT	    
        .init	    = direntry40_init,
        .insert	    = direntry40_insert,
        .remove	    = direntry40_remove,
        .estimate   = direntry40_estimate,
        .check	    = direntry40_check,
#else
        .init	    = NULL,
        .estimate   = NULL,
        .insert	    = NULL,
        .remove	    = NULL,
        .check	    = NULL,
#endif
        .valid	    = NULL,
	    
        .print	    = direntry40_print,
        .lookup	    = direntry40_lookup,
        .maxkey	    = direntry40_maxkey,
        .count	    = direntry40_count,
	
	.specific = {
	    .direntry = { 
		.entry = direntry40_entry
	    }
	}
    }
};

static reiser4_plugin_t *direntry40_start(reiser4_core_t *c) {
    core = c;
    return &direntry40_plugin;
}

plugin_register(direntry40_start);

