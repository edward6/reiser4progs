/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large.c -- reiser4 large key plugin. */

#ifdef ENABLE_LARGE_KEYS
#include "key_large.h"

extern reiser4_plug_t key_large_plug;

/* Returns minimal key */
static reiser4_key_t *key_large_minimal(void) {
	return key_common_minimal(&key_large_plug);
}

/* Returns maximal key */
static reiser4_key_t *key_large_maximal(void) {
	return key_common_maximal(&key_large_plug);
}

static uint32_t key_lage_bodysize(void) {
	return KEY_LARGE_LAST_INDEX;
}

/* Assigns src key to dst one  */
static errno_t key_large_assign(reiser4_key_t *dst,
				reiser4_key_t *src)
{
	aal_assert("umka-1110", dst != NULL);
	aal_assert("umka-1111", src != NULL);

	dst->plug = src->plug;
	dst->adjust = src->adjust;

	aal_memcpy(dst->body, src->body,
		   sizeof(key_large_t));
	
	return 0;
}

/* Sets up key type */
static void key_large_set_type(reiser4_key_t *key, 
			       key_type_t type) 
{
	aal_assert("umka-634", key != NULL);

	kl_set_minor((key_large_t *)key->body,
		      key_common_type2minor(type));
}

/* Returns key type */
key_type_t key_large_get_type(reiser4_key_t *key) {
	key_minor_t minor;
	
	aal_assert("umka-635", key != NULL);

	minor = kl_get_minor((key_large_t *)key->body);
	return key_common_minor2type(minor);
}

/* Sets up key locality */
void key_large_set_locality(reiser4_key_t *key, uint64_t locality) {
	aal_assert("umka-636", key != NULL);
	kl_set_locality((key_large_t *)key->body, locality);
}

/* Returns key locality */
uint64_t key_large_get_locality(reiser4_key_t *key) {
	aal_assert("umka-637", key != NULL);
	return kl_get_locality((key_large_t *)key->body);
}

/* Sets up key ordering (is not used in short keys ) */
void key_large_set_ordering(reiser4_key_t *key, uint64_t ordering) {
	aal_assert("umka-2331", key != NULL);
	kl_set_ordering((key_large_t *)key->body, ordering);
}

/* Returns key ordering (is not used in short keys) */
uint64_t key_large_get_ordering(reiser4_key_t *key) {
	aal_assert("umka-2332", key != NULL);
	return kl_get_ordering((key_large_t *)key->body);
}

/* Sets up key objectid */
void key_large_set_objectid(reiser4_key_t *key, 
			    uint64_t objectid) 
{
	aal_assert("umka-638", key != NULL);
	kl_set_objectid((key_large_t *)key->body, objectid);
}

/* Returns key objectid */
uint64_t key_large_get_objectid(reiser4_key_t *key) {
	aal_assert("umka-639", key != NULL);
	return kl_get_objectid((key_large_t *)key->body);
}

/* Sets up full key objectid */
void key_large_set_fobjectid(reiser4_key_t *key, uint64_t objectid) {
	aal_assert("umka-2345", key != NULL);
	kl_set_fobjectid((key_large_t *)key->body, objectid);
}

/* Returns full key objectid */
uint64_t key_large_get_fobjectid(reiser4_key_t *key) {
	aal_assert("umka-2346", key != NULL);
	return kl_get_fobjectid((key_large_t *)key->body);
}

/* Sets up key offset */
void key_large_set_offset(reiser4_key_t *key, 
			  uint64_t offset)
{
	aal_assert("umka-640", key != NULL);
	kl_set_offset((key_large_t *)key->body, offset);
}

/* Returns key offset */
uint64_t key_large_get_offset(reiser4_key_t *key) {
	aal_assert("umka-641", key != NULL);
	return kl_get_offset((key_large_t *)key->body);
}

static int key_large_hashed(reiser4_key_t *key) {
	return (key_large_get_ordering(key) & HASHED_NAME_MASK) ? 1 : 0;
}

/* Extracts name from key */
static char *key_large_get_name(reiser4_key_t *key,
				char *name)
{
	char *ptr;
	uint64_t offset;
	uint64_t objectid;
	uint64_t ordering;

	aal_assert("umka-2352", key != NULL);
	aal_assert("umka-2353", name != NULL);

	/* Check if the key is a hashed one */
	if (key_large_hashed(key))
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
		ordering &= ~FIBRE_MASK;
		ptr = aux_unpack_string(ordering, name);
		ptr = aux_unpack_string(objectid, ptr);
		aux_unpack_string(offset, ptr);
	}

	return name;
}

#ifndef ENABLE_STAND_ALONE
/* Sets up key offset */
static void key_large_set_hash(reiser4_key_t *key, 
			       uint64_t hash)
{
	aal_assert("vpf-129", key != NULL);
	kl_set_hash((key_large_t *)key->body, hash);
}

/* Returns key offset */
static uint64_t key_large_get_hash(reiser4_key_t *key) {
	aal_assert("vpf-130", key != NULL);
	return kl_get_hash((key_large_t *)key->body);
}
#endif

/* Cleans key body */
static void key_large_clean(reiser4_key_t *key) {
	aal_assert("vpf-139", key != NULL);

	key->adjust = 0;
	aal_memset(key->body, 0, sizeof(key->body));
}

/* Figures out if items are of one file or not. */
static int key_large_compshort(reiser4_key_t *key1, 
			       reiser4_key_t *key2) 
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
	
	/* Checking objectid  */
	return kl_comp_el((key_large_t *)key1->body,
			  (key_large_t *)key2->body, 2);
}

static int key_large_compraw(void *key1, void *key2) {
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
static int key_large_compfull(reiser4_key_t *key1, 
			      reiser4_key_t *key2) 
{
	aal_assert("vpf-135", key1 != NULL);
	aal_assert("vpf-136", key2 != NULL);

	return key_large_compraw(key1->body, key2->body);
}

/* Builds hash of the passed @name by means of using a hash plugin */
static errno_t key_large_build_hash(reiser4_key_t *key,
				    reiser4_plug_t *hash,
				    reiser4_plug_t *fibre,
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
	aal_assert("vpf-1568", fibre != NULL);

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
		ordering |= HASHED_NAME_MASK;

		offset = plug_call(hash->o.hash_ops, build,
				   name + INLINE_CHARS,
				   len - INLINE_CHARS);
	}

	ordering |= ((uint64_t)plug_call(fibre->o.fibre_ops, build, 
					 name, len) << FIBRE_SHIFT);
	
	/* Setting up objectid and offset */
	key_large_set_ordering(key, ordering);
	key_large_set_objectid(key, objectid);
	key_large_set_offset(key, offset);

	return 0;
}

/* Builds key by passed locality, objectid, and name. It is suitable for
   creating entry keys. */
static errno_t key_large_build_hashed(reiser4_key_t *key,
				      reiser4_plug_t *hash,
				      reiser4_plug_t *fibre,
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
    
	return key_large_build_hash(key, hash, fibre, name);
}

/* Builds generic key by all its components */
static errno_t key_large_build_generic(reiser4_key_t *key,
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
	
	if (type == KEY_FILENAME_TYPE) {
		kl_set_fobjectid((key_large_t *)key->body,
				objectid);
	} else {
		kl_set_objectid((key_large_t *)key->body,
				objectid);
	}
	
	kl_set_minor((key_large_t *)key->body,
		     key_common_type2minor(type));
	
	kl_set_offset((key_large_t *)key->body,
		      offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
extern void key_large_print(reiser4_key_t *key,
			    aal_stream_t *stream,
			    uint16_t options);

extern errno_t key_large_check_struct(reiser4_key_t *key);
#endif

static reiser4_key_ops_t key_large_ops = {
	.hashed		= key_large_hashed,
	.assign		= key_large_assign,
	.clean		= key_large_clean,
	.minimal	= key_large_minimal,
	.maximal	= key_large_maximal,
	.bodysize	= key_lage_bodysize,
	.compraw	= key_large_compraw,
	.compfull	= key_large_compfull,
	.compshort	= key_large_compshort,

	.build_hashed   = key_large_build_hashed,
	.build_generic  = key_large_build_generic,
	
#ifndef ENABLE_STAND_ALONE
	.check_struct	= key_large_check_struct,
	.print		= key_large_print,

	.set_hash	= key_large_set_hash,
	.get_hash	= key_large_get_hash,
#endif
		
	.set_type	= key_large_set_type,
	.get_type	= key_large_get_type,

	.set_locality	= key_large_set_locality,
	.get_locality	= key_large_get_locality,

	.set_ordering	= key_large_set_ordering,
	.get_ordering	= key_large_get_ordering,
	
	.set_objectid	= key_large_set_objectid,
	.get_objectid	= key_large_get_objectid,

	.set_fobjectid	= key_large_set_fobjectid,
	.get_fobjectid	= key_large_get_fobjectid,

	.set_offset	= key_large_set_offset,
	.get_offset	= key_large_get_offset,
	.get_name	= key_large_get_name
};

static reiser4_plug_t key_large_plug = {
	.cl    = class_init,
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
