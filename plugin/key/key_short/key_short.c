/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_short.c -- reiser4 short key plugin. */

#ifdef ENABLE_SHORT_KEYS
#include "key_short.h"

extern reiser4_key_plug_t key_short_plug;

/* Returns minimal key */
static reiser4_key_t *key_short_minimal(void) {
	return key_common_minimal(&key_short_plug);
}

/* Returns maximal key */
static reiser4_key_t *key_short_maximal(void) {
	return key_common_maximal(&key_short_plug);
}

static uint32_t key_short_bodysize(void) {
	return KEY_SHORT_LAST_INDEX;
}

#ifndef ENABLE_MINIMAL
/* Sets up key type */
static void key_short_set_type(reiser4_key_t *key, 
			       key_type_t type) 
{
	aal_assert("umka-634", key != NULL);

	ks_set_minor((key_short_t *)key->body,
		      key_common_type2minor(type));
}

/* Returns key type */
key_type_t key_short_get_type(reiser4_key_t *key) {
	key_minor_t minor;
	
	aal_assert("umka-635", key != NULL);

	minor = ks_get_minor((key_short_t *)key->body);
	return key_common_minor2type(minor);
}

/* Sets up full key objectid */
void key_short_set_fobjectid(reiser4_key_t *key, uint64_t objectid) {
	aal_assert("umka-2345", key != NULL);
	ks_set_fobjectid((key_short_t *)key->body, objectid);
}

/* Returns full key objectid */
uint64_t key_short_get_fobjectid(reiser4_key_t *key) {
	aal_assert("umka-2346", key != NULL);
	return ks_get_fobjectid((key_short_t *)key->body);
}

/* Sets up key locality */
void key_short_set_locality(reiser4_key_t *key, uint64_t locality) {
	aal_assert("umka-636", key != NULL);
	ks_set_locality((key_short_t *)key->body, locality);
}
#endif

/* Returns key locality */
uint64_t key_short_get_locality(reiser4_key_t *key) {
	aal_assert("umka-637", key != NULL);
	return ks_get_locality((key_short_t *)key->body);
}

/* Sets up key ordering (is not used in short keys ) */
void key_short_set_ordering(reiser4_key_t *key, uint64_t ordering) {
	aal_assert("umka-2331", key != NULL);
}

/* Returns key ordering (is not used in short keys) */
uint64_t key_short_get_ordering(reiser4_key_t *key) {
	aal_assert("umka-2332", key != NULL);
	return 0;
}

/* Sets up key objectid */
void key_short_set_objectid(reiser4_key_t *key, 
			    uint64_t objectid) 
{
	aal_assert("umka-638", key != NULL);
	ks_set_objectid((key_short_t *)key->body, objectid);
}

/* Returns key objectid */
uint64_t key_short_get_objectid(reiser4_key_t *key) {
	aal_assert("umka-639", key != NULL);
	return ks_get_objectid((key_short_t *)key->body);
}

/* Sets up key offset */
void key_short_set_offset(reiser4_key_t *key, 
			  uint64_t offset)
{
	aal_assert("umka-640", key != NULL);
	ks_set_offset((key_short_t *)key->body, offset);
}

/* Returns key offset */
uint64_t key_short_get_offset(reiser4_key_t *key) {
	aal_assert("umka-641", key != NULL);
	return ks_get_offset((key_short_t *)key->body);
}

static int key_short_hashed(reiser4_key_t *key) {
	return (ks_get_fobjectid((key_short_t *)key->body) &
		HASHED_NAME_MASK) ? 1 : 0;
}

/* Extracts name from key */
static char *key_short_get_name(reiser4_key_t *key,
				char *name)
{
	char *ptr;
	uint64_t offset;
	uint64_t objectid;
                                                                                        
	/* Check if the key is a hashed one */
	if (key_short_hashed(key))
		return NULL;
	
	offset = key_short_get_offset(key);
	objectid = ks_get_fobjectid((key_short_t *)key->body);
                                                                                        
	/* Special case, handling "." entry */
	if (objectid == 0ull && offset == 0ull) {
		*name = '.';
		*(name + 1) = '\0';
	} else {
		objectid &= ~FIBRE_MASK;
		ptr = aux_unpack_string(objectid, name);
		aux_unpack_string(offset, ptr);
	}

	return name;
}


#ifndef ENABLE_MINIMAL
/* Sets up key offset */
static void key_short_set_hash(reiser4_key_t *key, 
			       uint64_t hash)
{
	aal_assert("vpf-129", key != NULL);
	ks_set_hash((key_short_t *)key->body, hash);
}

/* Returns key offset */
static uint64_t key_short_get_hash(reiser4_key_t *key) {
	aal_assert("vpf-130", key != NULL);
	return ks_get_hash((key_short_t *)key->body);
}
#endif

/* Compares two first components of the pased keys (locality and objectid) */
static int key_short_compshort(reiser4_key_t *key1, 
			       reiser4_key_t *key2) 
{
	key_minor_t minor;
	int res;

	aal_assert("umka-2217", key1 != NULL);
	aal_assert("umka-2218", key2 != NULL);

	/* Checking locality */
	if ((res = ks_comp_el((key_short_t *)key1->body,
			      (key_short_t *)key2->body, 0)))
	{
		return res;
	}
	
	minor = ks_get_minor((key_short_t *)key1->body);
	
	if (key_common_minor2type(minor) == KEY_FILENAME_TYPE)
		return 0;
	
	/* Checking object id */
	return ks_comp_el((key_short_t *)key1->body,
			  (key_short_t *)key2->body, 1);
}

/* Compares two passed key bodies. */
static int key_short_compraw(void *key1, void *key2) {
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
static int key_short_compfull(reiser4_key_t *key1, 
			      reiser4_key_t *key2) 
{
	aal_assert("vpf-135", key1 != NULL);
	aal_assert("vpf-136", key2 != NULL);

	return key_short_compraw(key1->body, key2->body);
}

/* Builds hash of the passed @name by means of using a hash plugin */
static void key_short_build_hash(reiser4_key_t *key,
				 reiser4_hash_plug_t *hash,
				 reiser4_fibre_plug_t *fibre,
				 char *name) 
{
	uint16_t len;
	uint64_t objectid, offset;
    
	aal_assert("vpf-101", key != NULL);
	aal_assert("vpf-102", name != NULL);
    
	if ((len = aal_strlen(name)) == 1 && name[0] == '.')
		return;
    
	aal_assert("vpf-128", hash != NULL); 
	aal_assert("vpf-1567", fibre != NULL);
	
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
		objectid |= HASHED_NAME_MASK;
		
		offset = plugcall(hash, build,
				  (unsigned char *)name + OBJECTID_CHARS,
				  len - OBJECTID_CHARS);
	}
	
	objectid |= ((uint64_t)plugcall(fibre, build, 
					name, len) << FIBRE_SHIFT);
	
	/* Objectid must occupie 60 bits. If it takes more, then we have broken
	   key, or objectid allocator reached this value, that impossible in
	   near future and apprentry denotes bug in object allocator. */
	aal_assert("umka-1499", !(objectid & ~KEY_SHORT_OBJECTID_MASK));

	/* Setting up objectid and offset */
	ks_set_fobjectid((key_short_t *)key->body, objectid);
	key_short_set_offset(key, offset);
}

/* Builds key by passed locality, objectid, and name. It is suitable for
   creating entry keys. */
static void key_short_build_hashed(reiser4_key_t *key,
				   reiser4_hash_plug_t *hash,
				   reiser4_fibre_plug_t *fibre,
				   uint64_t locality,
				   uint64_t objectid,
				   char *name) 
{
	key_type_t type;
	
	aal_assert("vpf-140", key != NULL);
	aal_assert("umka-667", name != NULL);

	aal_memset(key, 0, sizeof(*key));
	type = key_common_minor2type(KEY_FILENAME_MINOR);
	
	key->plug = &key_short_plug;
	ks_set_locality((key_short_t *)key->body, objectid);
	ks_set_minor((key_short_t *)key->body,
		      key_common_type2minor(type));
	
	key_short_build_hash(key, hash, fibre, name);
}

/* Builds generic key by all its components */
static errno_t key_short_build_generic(reiser4_key_t *key,
				       key_type_t type,
				       uint64_t locality,
				       uint64_t ordering,
				       uint64_t objectid,
				       uint64_t offset)
{
	aal_assert("vpf-141", key != NULL);

	aal_memset(key, 0, sizeof(*key));
	key->plug = &key_short_plug;
	
	ks_set_locality((key_short_t *)key->body, locality);

	if (type == KEY_FILENAME_TYPE) {
		ks_set_fobjectid((key_short_t *)key->body, objectid);
	} else {
		ks_set_objectid((key_short_t *)key->body, objectid);
	}

	ks_set_minor((key_short_t *)key->body,
		     key_common_type2minor(type));
	
	ks_set_offset((key_short_t *)key->body, offset);

	return 0;
}

#ifndef ENABLE_MINIMAL
extern void key_short_print(reiser4_key_t *key,
			    aal_stream_t *stream, 
			    uint16_t options);

extern errno_t key_short_check_struct(reiser4_key_t *key);
#endif

reiser4_key_plug_t key_short_plug = {
	.p = {
		.id    = {KEY_SHORT_ID, 0, KEY_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "key_short",
		.desc  = "Short key plugin.",
#endif
	},
	
	.hashed		= key_short_hashed,
	.minimal	= key_short_minimal,
	.maximal	= key_short_maximal,
	.bodysize	= key_short_bodysize,
	.compfull	= key_short_compfull,
	.compraw	= key_short_compraw,
	.compshort	= key_short_compshort,
		
	.build_hashed   = key_short_build_hashed,
	.build_generic  = key_short_build_generic,
	
#ifndef ENABLE_MINIMAL
	.check_struct	= key_short_check_struct,
	.print		= key_short_print,

	.set_hash	= key_short_set_hash,
	.get_hash	= key_short_get_hash,
		
	.set_type	= key_short_set_type,
	.get_type	= key_short_get_type,

	.set_fobjectid	= key_short_set_fobjectid,
	.get_fobjectid	= key_short_get_fobjectid,
	
	.set_locality	= key_short_set_locality,
#endif	
	.get_locality	= key_short_get_locality,
	
	.set_objectid	= key_short_set_objectid,
	.get_objectid	= key_short_get_objectid,

	.set_ordering	= key_short_set_ordering,
	.get_ordering	= key_short_get_ordering,

	.set_offset	= key_short_set_offset,
	.get_offset	= key_short_get_offset,
	.get_name       = key_short_get_name
};

#endif
