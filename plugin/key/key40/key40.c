/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key40.c -- reiser4 default key plugin. */

#include "key40.h"

extern reiser4_plugin_t key40_plugin;

/* Minimal possible key */
static key_entity_t minimal_key = {
	.plugin = &key40_plugin,
	.body = { 0ull, 0ull, 0ull }
};

/* Maximal possible key */
static key_entity_t maximal_key = {
	.plugin = &key40_plugin,
	.body = { ~0ull, ~0ull, ~0ull }
};

/* Translates key type from libreiser4 type to key40 one */
static key40_minor_t key40_type2minor(key_type_t type) {
	switch (type) {
	case KEY_FILENAME_TYPE:
		return KEY40_FILENAME_MINOR;
	case KEY_STATDATA_TYPE:
		return KEY40_STATDATA_MINOR;
	case KEY_ATTRNAME_TYPE:
		return KEY40_ATTRNAME_MINOR;
	case KEY_ATTRBODY_TYPE:
		return KEY40_ATTRBODY_MINOR;
	case KEY_FILEBODY_TYPE:
		return KEY40_FILEBODY_MINOR;
	default:
		aal_exception_error("Invalid key type has been "
				    "detected 0x%x.", type);
		return 0xff;
	}
}

/* Translates key type from key40 to libreiser4 one */
static key_type_t key40_minor2type(key40_minor_t minor) {
	switch (minor) {
	case KEY40_FILENAME_MINOR:
		return KEY_FILENAME_TYPE;
	case KEY40_STATDATA_MINOR:
		return KEY_STATDATA_TYPE;
	case KEY40_ATTRNAME_MINOR:
		return KEY_ATTRNAME_TYPE;
	case KEY40_ATTRBODY_MINOR:
		return KEY_ATTRBODY_TYPE;
	case KEY40_FILEBODY_MINOR:
		return KEY_FILEBODY_TYPE;
	default:
		aal_exception_error("Invalid key minor has been "
				    "detected 0x%x.", minor);
		return 0xff;
	}
}

/* Returns minimal key */
static key_entity_t *key40_minimal(void) {
	return &minimal_key;
}

/* Returns maximal key */
static key_entity_t *key40_maximal(void) {
	return &maximal_key;
}

/* Assigns src key to dst one  */
static errno_t key40_assign(key_entity_t *dst,
			    key_entity_t *src)
{
	aal_assert("umka-1110", dst != NULL);
	aal_assert("umka-1111", src != NULL);

	dst->plugin = src->plugin;

	aal_memcpy(dst->body, src->body,
		   sizeof(key40_t));
	
	return 0;
}

/* Checks if passed key is realy key40 one */
static int key40_confirm(key_entity_t *key) {
	aal_assert("vpf-137", key != NULL);
	return k40_get_minor(
		(key40_t *)key->body) < KEY40_LAST_MINOR;
}

/* Sets up key type */
static void key40_set_type(key_entity_t *key, 
			   key_type_t type)
{
	aal_assert("umka-634", key != NULL);

	k40_set_minor((key40_t *)key->body,
		      key40_type2minor(type));
}

/* Returns key type */
static key_type_t key40_get_type(key_entity_t *key) {
	aal_assert("umka-635", key != NULL);

	return key40_minor2type(
		k40_get_minor((key40_t *)key->body));
}

/* Sets up key locality */
static void key40_set_locality(key_entity_t *key, 
			       uint64_t locality) 
{
	aal_assert("umka-636", key != NULL);
	k40_set_locality((key40_t *)key->body, locality);
}

/* Returns key locality */
static uint64_t key40_get_locality(key_entity_t *key) {
	aal_assert("umka-637", key != NULL);
	return k40_get_locality((key40_t *)key->body);
}

/* Sets up key objectid */
static void key40_set_objectid(key_entity_t *key, 
			       uint64_t objectid) 
{
	aal_assert("umka-638", key != NULL);
	k40_set_objectid((key40_t *)key->body, objectid);
}

/* Returns key objectid */
static uint64_t key40_get_objectid(key_entity_t *key) {
	aal_assert("umka-639", key != NULL);
	return k40_get_objectid((key40_t *)key->body);
}

/* Sets up key offset */
static void key40_set_offset(key_entity_t *key, 
			     uint64_t offset)
{
	aal_assert("umka-640", key != NULL);
	k40_set_offset((key40_t *)key->body, offset);
}

/* Returns key offset */
static uint64_t key40_get_offset(key_entity_t *key) {
	aal_assert("umka-641", key != NULL);
	return k40_get_offset((key40_t *)key->body);
}

#ifndef ENABLE_STAND_ALONE
/* Sets up key offset */
static void key40_set_hash(key_entity_t *key, 
			   uint64_t hash)
{
	aal_assert("vpf-129", key != NULL);
	k40_set_hash((key40_t *)key->body, hash);
}

/* Returns key offset */
static uint64_t key40_get_hash(key_entity_t *key) {
	aal_assert("vpf-130", key != NULL);
	return k40_get_hash((key40_t *)key->body);
}
#endif

/* Cleans key body */
static void key40_clean(key_entity_t *key) {
	aal_assert("vpf-139", key != NULL);
	aal_memset(key->body, 0, sizeof(key40_t));
}

static int key40_tall(key_entity_t *key) {
	uint64_t objectid = key40_get_objectid(key);
	return (objectid & 0x0100000000000000ull) ? 1 : 0;
}

/* Compares two first components of the pased keys (locality and objectid) */
static int key40_compare_short(key_entity_t *key1, 
			       key_entity_t *key2) 
{
	int res;

	aal_assert("umka-2217", key1 != NULL);
	aal_assert("umka-2218", key2 != NULL);
	
	if ((res = k40_comp_el((key40_t *)key1->body,
			       (key40_t *)key2->body, 0)) != 0)
	{
		return res;
	}
	
	if (key40_get_type(key1) == KEY_FILENAME_TYPE)
		return 0;
	
	return k40_comp_el((key40_t *)key1->body,
			   (key40_t *)key2->body, 1);
}

/* Compares two passed keys. Returns -1 if key1 lesser than key2, 0 if keys are
   equal and 1 if key1 is bigger then key2. */
static int key40_compare(key_entity_t *key1, 
			 key_entity_t *key2) 
{
	int res;

	aal_assert("vpf-135", key1 != NULL);
	aal_assert("vpf-136", key2 != NULL);
    	
	if ((res = k40_comp_el((key40_t *)key1->body,
			       (key40_t *)key2->body, 0)))
		return res;
	
	if ((res = k40_comp_el((key40_t *)key1->body,
			       (key40_t *)key2->body, 1)))
		return res;
	
	return k40_comp_el((key40_t *)key1->body,
			   (key40_t *)key2->body, 2);
}

/* Builds hash of the passed @name by means of using a hash plugin */
static errno_t key40_build_hash(key_entity_t *key,
				reiser4_plugin_t *hash,
				const char *name) 
{
	uint16_t len;
	uint64_t objectid, offset;
    
	aal_assert("vpf-101", key != NULL);
	aal_assert("vpf-102", name != NULL);
	aal_assert("vpf-128", hash != NULL); 
    
	if ((len = aal_strlen(name)) == 1 && name[0] == '.')
		return 0;
    
	/* Not dot, pack the first part of the name into objectid */
	objectid = aux_pack_string((char *)name, 1);
    
	if (len <= OID_CHARS + sizeof(uint64_t)) {
		offset = 0ull;

		if (len > OID_CHARS) {
			/* Does not fit into objectid, pack the second part of
			   the name into offset. */
			offset = aux_pack_string((char *)name + OID_CHARS, 0);
		}
	} else {

		/* Build hash by means of using hash plugin */
		objectid |= 0x0100000000000000ull;
		
		offset = plugin_call(hash->o.hash_ops, build,
				     (const char *)(name + OID_CHARS),
				     aal_strlen(name) - OID_CHARS);
	}

	/* Objectid must occupie 60 bits. If it takes more, then we have broken
	   key, or objectid allocator reached this value, that impossible in
	   near future and apprentry denotes bug in object allocator. */
	aal_assert("umka-1499", !(objectid & ~KEY40_OBJECTID_MASK));

	/* Setting up objectid and offset */
	key40_set_objectid(key, objectid);
	key40_set_offset(key, offset);

	return 0;
}

/* Builds key by passed locality, objectid, and name. It is suitable for
   creating entry keys. */
static errno_t key40_build_entry(key_entity_t *key,
				 reiser4_plugin_t *hash,
				 uint64_t locality,
				 uint64_t objectid,
				 const char *name) 
{
	aal_assert("vpf-140", key != NULL);
	aal_assert("umka-667", name != NULL);
	aal_assert("umka-1006", hash != NULL);

	key40_clean(key);

	key->plugin = &key40_plugin;
	key40_set_locality(key, objectid);
	key40_set_type(key, key40_minor2type(KEY40_FILENAME_MINOR));
    
	return key40_build_hash(key, hash, name);
}

/* Builds generic key by all its components */
static errno_t key40_build_generic(key_entity_t *key,
				   key_type_t type,
				   uint64_t locality,
				   uint64_t objectid,
				   uint64_t offset)
{
	aal_assert("vpf-141", key != NULL);

	key40_clean(key);
	key->plugin = &key40_plugin;
	
	k40_set_locality((key40_t *)key->body, locality);	
	k40_set_objectid((key40_t *)key->body, objectid);
	k40_set_minor((key40_t *)key->body, key40_type2minor(type));	
	k40_set_offset((key40_t *)key->body, offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Simple validness check */
static errno_t key40_valid(key_entity_t *key) {
	uint8_t band;
	key40_minor_t minor;
	
	aal_assert("vpf-243", key != NULL);

	if (!key40_confirm(key))
		return -EINVAL;
	
	minor = k40_get_minor((key40_t *)key->body);
	band = k40_get_band((key40_t *)key->body);
	
	if ((minor == KEY40_FILENAME_MINOR && band == 1) || band == 0)
		return 0;

	return -EINVAL;
}

/* Key minor names. They are used key40_print function */
static const char *const minor_names[] = {
	"NAME", "SD", "AN", "AB", "FB", "?"
};

/* Translates passed minor into corresponding name */
const char *key40_minor2name(key40_minor_t type) {
	if (type > KEY40_LAST_MINOR)
		type = KEY40_LAST_MINOR;
    
	return minor_names[type];
}

/* Prints key into passed stream */
errno_t key40_print(key_entity_t *key,
		    aal_stream_t *stream,
		    uint16_t options) 
{
	aal_assert("vpf-191", key != NULL);
	aal_assert("umka-1548", stream != NULL);

	aal_stream_format(stream, "[ key40 %llx:%x:%llx:%llx %s ]",
			  key40_get_locality(key), key40_get_type(key),
			  key40_get_objectid(key), key40_get_offset(key),
			  key40_minor2name(key40_get_type(key)));

	return 0;
}
#endif

static reiser4_key_ops_t key40_ops = {
	.tall              = key40_tall,
	.confirm	   = key40_confirm,
	.assign		   = key40_assign,
	.minimal	   = key40_minimal,
	.maximal	   = key40_maximal,
	.clean		   = key40_clean,

	.compare	   = key40_compare,
	.compare_short	   = key40_compare_short,
		
#ifndef ENABLE_STAND_ALONE
	.valid		   = key40_valid,
	.print		   = key40_print,

	.set_hash	   = key40_set_hash,
	.get_hash	   = key40_get_hash,
#endif
		
	.set_type	   = key40_set_type,
	.get_type	   = key40_get_type,

	.set_locality	   = key40_set_locality,
	.get_locality	   = key40_get_locality,

	.set_objectid	   = key40_set_objectid,
	.get_objectid	   = key40_get_objectid,

	.set_offset	   = key40_set_offset,
	.get_offset	   = key40_get_offset,
	
	.build_entry       = key40_build_entry,
	.build_generic     = key40_build_generic
};

static reiser4_plugin_t key40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = KEY_REISER40_ID,
		.group = 0,
		.type = KEY_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "key40",
		.desc = "Key for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.key_ops = &key40_ops
	}
};

static reiser4_plugin_t *key40_start(reiser4_core_t *c) {
	return &key40_plugin;
}

plugin_register(key40, key40_start, NULL);

