/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_short.c -- reiser4 short key plugin. */

#include "key_short.h"

extern reiser4_plugin_t key_short_plugin;

/* Returns minimal key */
static key_entity_t *key_short_minimal(void) {
	return key_common_minimal(&key_short_plugin);
}

/* Returns maximal key */
static key_entity_t *key_short_maximal(void) {
	return key_common_maximal(&key_short_plugin);
}

/* Assigns src key to dst one  */
static errno_t key_short_assign(key_entity_t *dst,
				key_entity_t *src)
{
	aal_assert("umka-1110", dst != NULL);
	aal_assert("umka-1111", src != NULL);

	dst->plugin = src->plugin;

	aal_memcpy(dst->body, src->body,
		   sizeof(key_short_t));
	
	return 0;
}

/* Checks if passed key is realy key_short one */
static int key_short_confirm(key_entity_t *key) {
	key_minor_t minor;
	
	aal_assert("vpf-137", key != NULL);
	
	minor = ks_get_minor((key_short_t *)key->body); 
	return minor < KEY_LAST_MINOR;
}

/* Sets up key type */
static void key_short_set_type(key_entity_t *key, 
			       key_type_t type)
{
	aal_assert("umka-634", key != NULL);

	ks_set_minor((key_short_t *)key->body,
		      key_common_type2minor(type));
}

/* Returns key type */
static key_type_t key_short_get_type(key_entity_t *key) {
	key_minor_t minor;
	
	aal_assert("umka-635", key != NULL);

	minor = ks_get_minor((key_short_t *)key->body);
	return key_common_minor2type(minor);
}

/* Sets up key locality */
static void key_short_set_locality(key_entity_t *key, 
				   uint64_t locality) 
{
	aal_assert("umka-636", key != NULL);
	ks_set_locality((key_short_t *)key->body, locality);
}

/* Returns key locality */
static uint64_t key_short_get_locality(key_entity_t *key) {
	aal_assert("umka-637", key != NULL);
	return ks_get_locality((key_short_t *)key->body);
}

/* Sets up key ordering (is not used in short keys ) */
static void key_short_set_ordering(key_entity_t *key, 
				   uint64_t ordering) 
{
	return;
}

/* Returns key ordering (is not used in short keys) */
static uint64_t key_short_get_ordering(key_entity_t *key) {
	return 0;
}

/* Sets up key objectid */
static void key_short_set_objectid(key_entity_t *key, 
				   uint64_t objectid) 
{
	aal_assert("umka-638", key != NULL);
	ks_set_objectid((key_short_t *)key->body, objectid);
}

/* Returns key objectid */
static uint64_t key_short_get_objectid(key_entity_t *key) {
	aal_assert("umka-639", key != NULL);
	return ks_get_objectid((key_short_t *)key->body);
}

/* Sets up key offset */
static void key_short_set_offset(key_entity_t *key, 
				 uint64_t offset)
{
	aal_assert("umka-640", key != NULL);
	ks_set_offset((key_short_t *)key->body, offset);
}

/* Returns key offset */
static uint64_t key_short_get_offset(key_entity_t *key) {
	aal_assert("umka-641", key != NULL);
	return ks_get_offset((key_short_t *)key->body);
}

#ifndef ENABLE_STAND_ALONE
/* Sets up key offset */
static void key_short_set_hash(key_entity_t *key, 
			       uint64_t hash)
{
	aal_assert("vpf-129", key != NULL);
	ks_set_hash((key_short_t *)key->body, hash);
}

/* Returns key offset */
static uint64_t key_short_get_hash(key_entity_t *key) {
	aal_assert("vpf-130", key != NULL);
	return ks_get_hash((key_short_t *)key->body);
}
#endif

/* Cleans key body */
static void key_short_clean(key_entity_t *key) {
	aal_assert("vpf-139", key != NULL);
	aal_memset(key->body, 0, sizeof(key_short_t));
}

static int key_short_tall(key_entity_t *key) {
	uint64_t objectid = key_short_get_objectid(key);
	return (objectid & 0x0100000000000000ull) ? 1 : 0;
}

#ifndef ENABLE_STAND_ALONE
/* Compares two first components of the pased keys (locality and objectid) */
static int key_short_compshort(key_entity_t *key1, 
			       key_entity_t *key2) 
{
	int res;

	aal_assert("umka-2217", key1 != NULL);
	aal_assert("umka-2218", key2 != NULL);

	/* Checking locality */
	if ((res = ks_comp_el((key_short_t *)key1->body,
			      (key_short_t *)key2->body, 0)))
	{
		return res;
	}
	
	if (key_short_get_type(key1) == KEY_FILENAME_TYPE)
		return 0;

	/* Checking object id */
	return ks_comp_el((key_short_t *)key1->body,
			  (key_short_t *)key2->body, 1);
}
#endif

static int key_short_compraw(body_t *key1, body_t *key2) {
	int res;

	if ((res = ks_comp_el((key_short_t *)key1,
			      (key_short_t *)key2, 0)))
	{
		return res;
	}
	
	if ((res = ks_comp_el((key_short_t *)key1,
			      (key_short_t *)key2, 1)))
	{
		return res;
	}
	
	return ks_comp_el((key_short_t *)key1,
			  (key_short_t *)key2, 2);
}

/* Compares two passed keys. Returns -1 if key1 lesser than key2, 0 if keys are
   equal and 1 if key1 is bigger then key2. */
static int key_short_compfull(key_entity_t *key1, 
			      key_entity_t *key2) 
{
	aal_assert("vpf-135", key1 != NULL);
	aal_assert("vpf-136", key2 != NULL);

	return key_short_compraw(key1->body, key2->body);
}

/* Builds hash of the passed @name by means of using a hash plugin */
static errno_t key_short_build_hash(key_entity_t *key,
				    reiser4_plugin_t *hash,
				    char *name) 
{
	uint16_t len;
	uint64_t objectid, offset;
    
	aal_assert("vpf-101", key != NULL);
	aal_assert("vpf-102", name != NULL);
	aal_assert("vpf-128", hash != NULL); 
    
	if ((len = aal_strlen(name)) == 1 && name[0] == '.')
		return 0;
    
	/* Not dot, pack the first part of the name into objectid */
	objectid = aux_pack_string(name, 1);
    
	if (len <= OBJECTID_CHARS + OFFSET_CHARS) {
		offset = 0ull;

		if (len > OBJECTID_CHARS) {
			/* Does not fit into objectid, pack the second part of
			   the name into offset. */
			offset = aux_pack_string(name + OBJECTID_CHARS, 0);
		}
	} else {

		/* Build hash by means of using hash plugin */
		objectid |= 0x0100000000000000ull;
		
		offset = plugin_call(hash->o.hash_ops, build,
				     name + OBJECTID_CHARS,
				     len - OBJECTID_CHARS);
	}

	/* Objectid must occupie 60 bits. If it takes more, then we have broken
	   key, or objectid allocator reached this value, that impossible in
	   near future and apprentry denotes bug in object allocator. */
	aal_assert("umka-1499", !(objectid & ~KEY_SHORT_OBJECTID_MASK));

	/* Setting up objectid and offset */
	key_short_set_objectid(key, objectid);
	key_short_set_offset(key, offset);

	return 0;
}

/* Builds key by passed locality, objectid, and name. It is suitable for
   creating entry keys. */
static errno_t key_short_build_entry(key_entity_t *key,
				     reiser4_plugin_t *hash,
				     uint64_t ordering,
				     uint64_t objectid,
				     char *name) 
{
	key_type_t type;
	
	aal_assert("vpf-140", key != NULL);
	aal_assert("umka-667", name != NULL);
	aal_assert("umka-1006", hash != NULL);

	key_short_clean(key);
	type = key_common_minor2type(KEY_FILENAME_MINOR);
	
	key->plugin = &key_short_plugin;
	key_short_set_locality(key, objectid);
	key_short_set_type(key, type);
    
	return key_short_build_hash(key, hash, name);
}

/* Builds generic key by all its components */
static errno_t key_short_build_gener(key_entity_t *key,
				     key_type_t type,
				     uint64_t locality,
				     uint64_t ordering,
				     uint64_t objectid,
				     uint64_t offset)
{
	aal_assert("vpf-141", key != NULL);

	key_short_clean(key);
	key->plugin = &key_short_plugin;
	
	ks_set_locality((key_short_t *)key->body,
			locality);
	
	ks_set_objectid((key_short_t *)key->body,
			objectid);

	ks_set_minor((key_short_t *)key->body,
		     key_common_type2minor(type));
	
	ks_set_offset((key_short_t *)key->body,
		      offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Simple validness check */
static errno_t key_short_valid(key_entity_t *key) {
	uint8_t band;
	key_minor_t minor;
	
	aal_assert("vpf-243", key != NULL);

	if (!key_short_confirm(key))
		return -EINVAL;
	
	minor = ks_get_minor((key_short_t *)key->body);
	band = ks_get_band((key_short_t *)key->body);

	if (band == 0)
		return 0;
	
	if (minor == KEY_FILENAME_MINOR && band == 1)
		return 0;

	return -EINVAL;
}

/* Prints key into passed stream */
errno_t key_short_print(key_entity_t *key,
			aal_stream_t *stream,
			uint16_t options) 
{
	aal_assert("vpf-191", key != NULL);
	aal_assert("umka-1548", stream != NULL);

	aal_stream_format(stream, "[ key_short %llx:%x:%llx:%llx %s ]",
			  key_short_get_locality(key), key_short_get_type(key),
			  key_short_get_objectid(key), key_short_get_offset(key),
			  key_common_minor2name(key_short_get_type(key)));

	return 0;
}
#endif

static reiser4_key_ops_t key_short_ops = {
	.tall              = key_short_tall,
	.confirm           = key_short_confirm,
	.assign            = key_short_assign,
	.clean             = key_short_clean,
	.minimal           = key_short_minimal,
	.maximal           = key_short_maximal,
	.compfull	   = key_short_compfull,
	.compshort	   = key_short_compshort,
	.compraw           = key_short_compraw,
		
	.build_entry       = key_short_build_entry,
	.build_gener       = key_short_build_gener,
	
#ifndef ENABLE_STAND_ALONE
	.valid		   = key_short_valid,
	.print		   = key_short_print,

	.set_hash	   = key_short_set_hash,
	.get_hash	   = key_short_get_hash,
#endif
		
	.set_type	   = key_short_set_type,
	.get_type	   = key_short_get_type,

	.set_offset	   = key_short_set_offset,
	.get_offset	   = key_short_get_offset,
	
	.set_locality	   = key_short_set_locality,
	.get_locality	   = key_short_get_locality,

	.set_ordering	   = key_short_set_ordering,
	.get_ordering	   = key_short_get_ordering,
	
	.set_objectid	   = key_short_set_objectid,
	.get_objectid	   = key_short_get_objectid,

	.set_fobjectid	   = NULL,
	.get_fobjectid	   = NULL
};

static reiser4_plugin_t key_short_plugin = {
	.cl    = CLASS_INIT,
	.id    = {KEY_SHORT_ID, 0, KEY_PLUGIN_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "key_short",
	.desc  = "Short key for reiser4, ver. " VERSION,
#endif
	.o = {
		.key_ops = &key_short_ops
	}
};

static reiser4_plugin_t *key_short_start(reiser4_core_t *c) {
	return &key_short_plugin;
}

plugin_register(key_short, key_short_start, NULL);
