/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key.c -- reiser4 common key code. */  

#include <reiser4/libreiser4.h>

#ifndef ENABLE_MINIMAL
void reiser4_key_free(reiser4_key_t *key) {
	aal_free(key);
}
#endif

int reiser4_key_compshort(
	reiser4_key_t *key1,	    /* the first key for comparing */
	reiser4_key_t *key2)	    /* the second one */
{
	aal_assert("umka-764", key1 != NULL);
	aal_assert("umka-765", key2 != NULL);

	aal_assert("umka-906", key1->plug != NULL);

	return objcall(key1, compshort, key2);
}

/* Compares two keys in plugin independent manner by means of using one of
   passed keys plugin. */
int reiser4_key_compfull(
	reiser4_key_t *key1,	    /* the first key for comparing */
	reiser4_key_t *key2)	    /* the second one */
{
	aal_assert("umka-764", key1 != NULL);
	aal_assert("umka-765", key2 != NULL);

	aal_assert("umka-906", key1->plug != NULL);

	return objcall(key1, compfull, key2);
}

#ifndef ENABLE_MINIMAL
/* Builds full non-directory key */
errno_t reiser4_key_build_generic(
	reiser4_key_t *key,	    /* key to be built */
	uint32_t type,		    /* key type to be used */
	oid_t locality,		    /* locality to be used */
	uint64_t ordering,	    /* ordering to be used */
	oid_t objectid,		    /* objectid to be used */
	uint64_t offset)	    /* offset to be used */
{
	aal_assert("umka-665", key != NULL);
	aal_assert("umka-666", key->plug != NULL);

	return objcall(key, build_generic, type, locality, 
		       ordering, objectid, offset);
}

/* Builds full directory key. */
void reiser4_key_build_hashed(
	reiser4_key_t *key,	    /* key to be built */
	reiser4_hash_plug_t *hash,  /* hash plugin to be used */
	reiser4_fibre_plug_t *fibre,/* fibre plugin to be used */
	oid_t locality,		    /* loaclity to be used */
	oid_t objectid,		    /* objectid to be used */
	char *name)	            /* entry name to be hashed */
{
	aal_assert("umka-668", key != NULL);
	aal_assert("umka-670", name != NULL);
	aal_assert("umka-669", key->plug != NULL);
    
	objcall(key, build_hashed, hash, fibre, locality, objectid, name);
}

/* Sets key type */
errno_t reiser4_key_set_type(
	reiser4_key_t *key,	    /* key type will be updated in */
	uint32_t type)		    /* new key type */
{
	aal_assert("umka-686", key != NULL);
	aal_assert("umka-687", key->plug != NULL);

	objcall(key, set_type, type);
	return 0;
}

/* Updates key objectid */
errno_t reiser4_key_set_objectid(
	reiser4_key_t *key,	    /* key objectid will be updated in */
	oid_t objectid)	            /* new objectid */
{
	aal_assert("umka-694", key != NULL);
	aal_assert("umka-695", key->plug != NULL);
    
	objcall(key, set_objectid, objectid);

	return 0;
}

/* Updates key locality */
errno_t reiser4_key_set_locality(
	reiser4_key_t *key,	    /* key locality will be updated in */
	oid_t locality)	            /* new locality */
{
	aal_assert("umka-696", key != NULL);
	aal_assert("umka-697", key->plug != NULL);
    
	objcall(key, set_locality, locality);
	return 0;
}

/* Updates key ordering */
errno_t reiser4_key_set_ordering(
	reiser4_key_t *key,	    /* key, ordering will be updated in */
	uint64_t ordering)          /* new ordering */
{
	aal_assert("umka-2337", key != NULL);
	aal_assert("umka-2338", key->plug != NULL);
    
	objcall(key, set_ordering, ordering);
	return 0;
}
#endif
/* Sets key offset */
errno_t reiser4_key_set_offset(
	reiser4_key_t *key,	    /* key to be updated */
	uint64_t offset)	    /* new offset */
{
	aal_assert("umka-688", key != NULL);
	aal_assert("umka-689", key->plug != NULL);
    
	objcall(key, set_offset, offset);
	return 0;
}


/* Returns key offset */
uint64_t reiser4_key_get_offset(reiser4_key_t *key) {
	aal_assert("umka-700", key != NULL);
	aal_assert("umka-701", key->plug != NULL);

	return objcall(key, get_offset);
}

/* Increases key's offset by passed @value */
void reiser4_key_inc_offset(reiser4_key_t *key, uint64_t value) {
	reiser4_key_set_offset(key, reiser4_key_get_offset(key) + value);
}

#ifndef ENABLE_MINIMAL
/* Gets key type */
uint32_t reiser4_key_get_type(reiser4_key_t *key) {
	aal_assert("umka-698", key != NULL);
	aal_assert("umka-699", key->plug != NULL);

	return objcall(key, get_type);
}

/* Returns key objectid */
oid_t reiser4_key_get_objectid(reiser4_key_t *key) {
	aal_assert("umka-702", key != NULL);
	aal_assert("umka-703", key->plug != NULL);

	return objcall(key, get_objectid);
}

/* Returns key locality */
oid_t reiser4_key_get_locality(reiser4_key_t *key) {
	aal_assert("umka-704", key != NULL);
	aal_assert("umka-705", key->plug != NULL);

	return objcall(key, get_locality);
}

/* Returns key locality */
uint64_t reiser4_key_get_ordering(reiser4_key_t *key) {
	aal_assert("umka-2335", key != NULL);
	aal_assert("umka-2336", key->plug != NULL);

	return objcall(key, get_ordering);
}
#endif

/* Returns the maximal possible key  */
void reiser4_key_maximal(reiser4_key_t *key) {
	reiser4_key_t *entity;
    
	aal_assert("vpf-185", key != NULL);
	aal_assert("vpf-186", key->plug != NULL);

	entity = plugcall(key->plug, maximal);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

#ifndef ENABLE_MINIMAL
/* Returns the minimal possible key */
void reiser4_key_minimal(reiser4_key_t *key) {
	reiser4_key_t *entity;
    
	aal_assert("vpf-187", key != NULL);
	aal_assert("vpf-188", key->plug != NULL);

	entity = plugcall(key->plug, minimal);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

/* Sets key hash component */
errno_t reiser4_key_set_hash(
	reiser4_key_t *key,	    /* key hash will be updated in */
	uint64_t hash)		    /* new hash value */
{
	aal_assert("umka-706", key != NULL);
	aal_assert("umka-707", key->plug != NULL);
    
	objcall(key, set_hash, hash);
	return 0;
}

/* Returns key hash */
uint64_t reiser4_key_get_hash(reiser4_key_t *key) {
	aal_assert("umka-708", key != NULL);
	aal_assert("umka-709", key->plug != NULL);

	return objcall(key, get_hash);
}

void reiser4_key_print(reiser4_key_t *key, 
		       aal_stream_t *stream,
		       uint16_t options)
{
	aal_assert("vpf-189", key != NULL);
	aal_assert("vpf-190", key->plug != NULL);

	objcall(key, print, stream, options);
}
#endif
