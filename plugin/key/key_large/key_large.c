/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large.c -- reiser4 large key plugin. */

#ifdef ENABLE_LARGE_KEYS
#include "key_large.h"

extern reiser4_plug_t key_large_plug;

/* Returns minimal key */
static key_entity_t *key_large_minimal(void) {
	return key_common_minimal(&key_large_plug);
}

/* Returns maximal key */
static key_entity_t *key_large_maximal(void) {
	return key_common_maximal(&key_large_plug);
}

/* Assigns src key to dst one  */
static errno_t key_large_assign(key_entity_t *dst,
				key_entity_t *src)
{
	aal_assert("umka-1110", dst != NULL);
	aal_assert("umka-1111", src != NULL);

	dst->plug = src->plug;

	aal_memcpy(dst->body, src->body,
		   sizeof(key_large_t));
	
	return 0;
}

/* Checks if passed key is realy key_large one */
static int key_large_confirm(key_entity_t *key) {
	key_minor_t minor;
	
	aal_assert("vpf-137", key != NULL);
	
	minor = kl_get_minor((key_large_t *)key->body); 
	return minor < KEY_LAST_MINOR;
}

/* Sets up key type */
static void key_large_set_type(key_entity_t *key, 
			       key_type_t type)
{
	aal_assert("umka-634", key != NULL);

	kl_set_minor((key_large_t *)key->body,
		      key_common_type2minor(type));
}

/* Returns key type */
static key_type_t key_large_get_type(key_entity_t *key) {
	key_minor_t minor;
	
	aal_assert("umka-635", key != NULL);

	minor = kl_get_minor((key_large_t *)key->body);
	return key_common_minor2type(minor);
}

/* Sets up key locality */
static void key_large_set_locality(key_entity_t *key, 
				   uint64_t locality) 
{
	aal_assert("umka-636", key != NULL);
	kl_set_locality((key_large_t *)key->body, locality);
}

/* Returns key locality */
static uint64_t key_large_get_locality(key_entity_t *key) {
	aal_assert("umka-637", key != NULL);
	return kl_get_locality((key_large_t *)key->body);
}

/* Sets up key ordering (is not used in short keys ) */
static void key_large_set_ordering(key_entity_t *key, 
				   uint64_t ordering) 
{
	aal_assert("umka-2331", key != NULL);
	kl_set_ordering((key_large_t *)key->body, ordering);
}

/* Returns key ordering (is not used in short keys) */
static uint64_t key_large_get_ordering(key_entity_t *key) {
	aal_assert("umka-2332", key != NULL);
	return kl_get_ordering((key_large_t *)key->body);
}

/* Sets up key objectid */
static void key_large_set_objectid(key_entity_t *key, 
				   uint64_t objectid) 
{
	aal_assert("umka-638", key != NULL);
	kl_set_objectid((key_large_t *)key->body, objectid);
}

/* Returns key objectid */
static uint64_t key_large_get_objectid(key_entity_t *key) {
	aal_assert("umka-639", key != NULL);
	return kl_get_objectid((key_large_t *)key->body);
}

/* Sets up full key objectid */
static void key_large_set_fobjectid(key_entity_t *key, 
				    uint64_t objectid) 
{
	aal_assert("umka-2345", key != NULL);
	kl_set_fobjectid((key_large_t *)key->body, objectid);
}

/* Returns full key objectid */
static uint64_t key_large_get_fobjectid(key_entity_t *key) {
	aal_assert("umka-2346", key != NULL);
	return kl_get_fobjectid((key_large_t *)key->body);
}

/* Sets up key offset */
static void key_large_set_offset(key_entity_t *key, 
				 uint64_t offset)
{
	aal_assert("umka-640", key != NULL);
	kl_set_offset((key_large_t *)key->body, offset);
}

/* Returns key offset */
static uint64_t key_large_get_offset(key_entity_t *key) {
	aal_assert("umka-641", key != NULL);
	return kl_get_offset((key_large_t *)key->body);
}

static int key_large_tall(key_entity_t *key) {
	return (key_large_get_ordering(key) &
		0x0100000000000000ull) ? 1 : 0;
}

/* Extracts name from key */
static char *key_large_get_name(key_entity_t *key,
				char *name)
{
	char *ptr;
	uint64_t offset;
	uint64_t objectid;
	uint64_t ordering;

	aal_assert("umka-2352", key != NULL);
	aal_assert("umka-2353", name != NULL);

	/* Check if key is not tall one */
	if (key_large_tall(key))
		return NULL;

	offset = key_large_get_offset(key);
	ordering = key_large_get_ordering(key);
	objectid = key_large_get_fobjectid(key);

	/* Check if key is dot one */
	if (objectid == 0ull && offset == 0ull &&
	    ordering == 0ull)
	{
		*name = '.';
		*(name + 1) = '\0';
	} else {
		ordering &= ~0x0100000000000000ull;
		ptr = aux_unpack_string(ordering, name);
		ptr = aux_unpack_string(objectid, ptr);
		aux_unpack_string(offset, ptr);
	}

	return name;
}

#ifndef ENABLE_STAND_ALONE
/* Sets up key offset */
static void key_large_set_hash(key_entity_t *key, 
			       uint64_t hash)
{
	aal_assert("vpf-129", key != NULL);
	kl_set_hash((key_large_t *)key->body, hash);
}

/* Returns key offset */
static uint64_t key_large_get_hash(key_entity_t *key) {
	aal_assert("vpf-130", key != NULL);
	return kl_get_hash((key_large_t *)key->body);
}
#endif

/* Cleans key body */
static void key_large_clean(key_entity_t *key) {
	aal_assert("vpf-139", key != NULL);
	aal_memset(key->body, 0, sizeof(key_large_t));
}

#ifndef ENABLE_STAND_ALONE
/* Figures out if items are of one file or not. */
static int key_large_compshort(key_entity_t *key1, 
			       key_entity_t *key2) 
{
	uint64_t ord1, ord2;
	int res;

	aal_assert("umka-2217", key1 != NULL);
	aal_assert("umka-2218", key2 != NULL);

	/* Cheking locality first */
	if ((res = kl_comp_el((key_large_t *)key1->body,
			      (key_large_t *)key2->body, 0)))
	{
		return res;
	}
	
	/* There is nothing to check for entry keys anymore. */
	if (key_large_get_type(key1) == KEY_FILENAME_TYPE)
		return 0;

	ord1 = key_large_get_ordering(key1);
	ord2 = key_large_get_ordering(key2);
	
	/* Checking ordering. */
	if ((res = aal_memcmp(&ord1, &ord2, sizeof(ord1))))
		return res;
	
	/* Cheking objectid  */
	return kl_comp_el((key_large_t *)key1->body,
			  (key_large_t *)key2->body, 2);
}
#endif

static int key_large_compraw(body_t *key1, body_t *key2) {
	int res;

	if ((res = kl_comp_el((key_large_t *)key1,
			      (key_large_t *)key2, 0)))
	{
		return res;
	}
	
	if ((res = kl_comp_el((key_large_t *)key1,
			      (key_large_t *)key2, 1)))
	{
		return res;
	}
	
	if ((res = kl_comp_el((key_large_t *)key1,
			      (key_large_t *)key2, 2)))
	{
		return res;
	}
	
	return kl_comp_el((key_large_t *)key1,
			  (key_large_t *)key2, 3);
}

/* Compares two passed keys. Returns -1 if key1 lesser than key2, 0 if keys are
   equal and 1 if key1 is bigger then key2. */
static int key_large_compfull(key_entity_t *key1, 
			      key_entity_t *key2) 
{
	aal_assert("vpf-135", key1 != NULL);
	aal_assert("vpf-136", key2 != NULL);

	return key_large_compraw(key1->body, key2->body);
}

/* Builds hash of the passed @name by means of using a hash plugin */
static errno_t key_large_build_hash(key_entity_t *key,
				    reiser4_plug_t *hash,
				    char *name) 
{
	uint16_t len;
	uint64_t offset;
	uint64_t objectid;
	uint64_t ordering;
    
	aal_assert("vpf-101", key != NULL);
	aal_assert("vpf-102", name != NULL);
    
	if ((len = aal_strlen(name)) == 1 && name[0] == '.')
		return 0;

	aal_assert("vpf-128", hash != NULL); 

	ordering = aux_pack_string(name, 1);

	if (len > ORDERING_CHARS) 
		objectid = aux_pack_string(name + ORDERING_CHARS, 0);
	else
		objectid = 0ull;

	if (len <= ORDERING_CHARS + OBJECTID_CHARS + OFFSET_CHARS) {
		if (len > INLINE_CHARS)
			offset = aux_pack_string(name + INLINE_CHARS, 0);
		else
			offset = 0ull;
	} else {
		ordering |= 0x0100000000000000ull;

		offset = plug_call(hash->o.hash_ops, build,
				   name + INLINE_CHARS,
				   len - INLINE_CHARS);
	}

	/* Setting up objectid and offset */
	key_large_set_ordering(key, ordering);
	key_large_set_objectid(key, objectid);
	key_large_set_offset(key, offset);

	return 0;
}

/* Builds key by passed locality, objectid, and name. It is suitable for
   creating entry keys. */
static errno_t key_large_build_entry(key_entity_t *key,
				     reiser4_plug_t *hash,
				     uint64_t locality,
				     uint64_t objectid,
				     char *name) 
{
	key_type_t type;
	
	aal_assert("vpf-140", key != NULL);
	aal_assert("umka-667", name != NULL);

	key_large_clean(key);
	type = key_common_minor2type(KEY_FILENAME_MINOR);
	
	key->plug = &key_large_plug;
	key_large_set_locality(key, objectid);
	key_large_set_type(key, type);
    
	return key_large_build_hash(key, hash, name);
}

/* Builds generic key by all its components */
static errno_t key_large_build_gener(key_entity_t *key,
				     key_type_t type,
				     uint64_t locality,
				     uint64_t ordering,
				     uint64_t objectid,
				     uint64_t offset)
{
	aal_assert("vpf-141", key != NULL);

	key_large_clean(key);
	key->plug = &key_large_plug;
	
	kl_set_locality((key_large_t *)key->body,
			locality);
	
	kl_set_ordering((key_large_t *)key->body,
			ordering);
	
	kl_set_objectid((key_large_t *)key->body,
			objectid);

	kl_set_minor((key_large_t *)key->body,
		     key_common_type2minor(type));
	
	kl_set_offset((key_large_t *)key->body,
		      offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Simple validness check */
static errno_t key_large_valid(key_entity_t *key) {
	uint8_t band;
	key_minor_t minor;
	
	aal_assert("vpf-243", key != NULL);

	if (!key_large_confirm(key))
		return -EINVAL;
	
	minor = kl_get_minor((key_large_t *)key->body);
	band = kl_get_band((key_large_t *)key->body);

	if (band == 0)
		return 0;
	
	if (minor == KEY_FILENAME_MINOR && band == 1)
		return 0;

	return -EINVAL;
}

/* Prints key into passed stream */
errno_t key_large_print(key_entity_t *key,
			aal_stream_t *stream,
			uint16_t options) 
{
	const char *name;
	
	aal_assert("vpf-191", key != NULL);
	aal_assert("umka-1548", stream != NULL);

	if (options == PO_INO) {
		aal_stream_format(stream, "%llx:%llx:%llx",
				  key_large_get_locality(key),
				  key_large_get_ordering(key),
				  key_large_get_objectid(key));
	} else {
		name = key_common_minor2name(key_large_get_type(key));

		aal_stream_format(stream, "%llx:%x:%llx:%llx:%llx:%s",
				  key_large_get_locality(key),
				  key_large_get_type(key),
				  key_large_get_ordering(key),
				  key_large_get_fobjectid(key),
				  key_large_get_offset(key), name);
	}
	
	return 0;
}
#endif

static reiser4_key_ops_t key_large_ops = {
	.tall              = key_large_tall,
	.assign            = key_large_assign,
	.clean             = key_large_clean,
	.minimal           = key_large_minimal,
	.maximal           = key_large_maximal,
	.compraw	   = key_large_compraw,
	.compfull	   = key_large_compfull,

#ifndef ENABLE_STAND_ALONE
	.compshort	   = key_large_compshort,
#endif
		
	.build_entry       = key_large_build_entry,
	.build_gener       = key_large_build_gener,
	
#ifndef ENABLE_STAND_ALONE
	.valid		   = key_large_valid,
	.print		   = key_large_print,

	.set_hash	   = key_large_set_hash,
	.get_hash	   = key_large_get_hash,
#endif
		
	.set_type	   = key_large_set_type,
	.get_type	   = key_large_get_type,

	.set_locality	   = key_large_set_locality,
	.get_locality	   = key_large_get_locality,

	.set_ordering	   = key_large_set_ordering,
	.get_ordering	   = key_large_get_ordering,
	
	.set_objectid	   = key_large_set_objectid,
	.get_objectid	   = key_large_get_objectid,

	.set_fobjectid	   = key_large_set_fobjectid,
	.get_fobjectid	   = key_large_get_fobjectid,

	.set_offset	   = key_large_set_offset,
	.get_offset	   = key_large_get_offset,
	.get_name          = key_large_get_name
};

static reiser4_plug_t key_large_plug = {
	.cl    = CLASS_INIT,
	.id    = {KEY_LARGE_ID, 0, KEY_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "key_large",
	.desc  = "Large key for reiser4, ver. " VERSION,
#endif
	.o = {
		.key_ops = &key_large_ops
	}
};

static reiser4_plug_t *key_large_start(reiser4_core_t *c) {
	return &key_large_plug;
}

plug_register(key_large, key_large_start, NULL);
#endif
