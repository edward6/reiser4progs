/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key.c -- reiser4 common key code. */  

#include <reiser4/libreiser4.h>

reiser4_key_t *reiser4_key_clone(reiser4_key_t *key) {
	reiser4_key_t *clone;
	
	aal_assert("umka-2358", key != NULL);

	if (!(clone = aal_calloc(sizeof(*key), 0)))
		return NULL;

	reiser4_key_assign(clone, key);
	return clone;
}

void reiser4_key_free(reiser4_key_t *key) {
	aal_free(key);
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
	aal_assert("umka-906", key2->plug != NULL);

	return plug_call(key1->plug->o.key_ops, 
			 compfull, key1, key2);
}

int reiser4_key_compshort(
	reiser4_key_t *key1,	    /* the first key for comparing */
	reiser4_key_t *key2)	    /* the second one */
{
	aal_assert("umka-764", key1 != NULL);
	aal_assert("umka-765", key2 != NULL);

	aal_assert("umka-906", key1->plug != NULL);
	aal_assert("umka-906", key2->plug != NULL);

	return plug_call(key1->plug->o.key_ops, 
			 compshort, key1, key2);
}


/* Makes copy src key to dst one */
errno_t reiser4_key_assign(
	reiser4_key_t *dst,	    /* destination key */
	reiser4_key_t *src)	    /* source key */
{
	aal_assert("umka-1112", dst != NULL);
	aal_assert("umka-1113", src != NULL);
	aal_assert("umka-1114", src->plug != NULL);

	dst->plug = src->plug;
	
	return plug_call(src->plug->o.key_ops,
			 assign, dst, src);
}

/* Cleans specified key */
void reiser4_key_clean(
	reiser4_key_t *key)	    /* key to be clean */
{
	aal_assert("umka-675", key != NULL);
	aal_assert("umka-676", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, clean, key);
} 

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

	return plug_call(key->plug->o.key_ops, build_generic, key,
			 type, locality, ordering, objectid, offset);
}

/* Builds full directory key. */
errno_t reiser4_key_build_hashed(
	reiser4_key_t *key,	    /* key to be built */
	reiser4_plug_t *plug,       /* hash plugin to be used */
	oid_t locality,		    /* loaclity to be used */
	oid_t objectid,		    /* objectid to be used */
	char *name)	            /* entry name to be hashed */
{
	aal_assert("umka-668", key != NULL);
	aal_assert("umka-670", name != NULL);
	aal_assert("umka-669", key->plug != NULL);
    
	return plug_call(key->plug->o.key_ops, build_hashed,
			 key, plug, locality, objectid, name);
}

/* Sets key type */
errno_t reiser4_key_set_type(
	reiser4_key_t *key,	    /* key type will be updated in */
	uint32_t type)		    /* new key type */
{
	aal_assert("umka-686", key != NULL);
	aal_assert("umka-687", key->plug != NULL);

	plug_call(key->plug->o.key_ops, set_type, key, type);
	return 0;
}

/* Sets key offset */
errno_t reiser4_key_set_offset(
	reiser4_key_t *key,	    /* key to be updated */
	uint64_t offset)	    /* new offset */
{
	aal_assert("umka-688", key != NULL);
	aal_assert("umka-689", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, set_offset, key, offset);
    
	return 0;
}

/* Updates key objectid */
errno_t reiser4_key_set_objectid(
	reiser4_key_t *key,	    /* key objectid will be updated in */
	oid_t objectid)	            /* new objectid */
{
	aal_assert("umka-694", key != NULL);
	aal_assert("umka-695", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, set_objectid,
		  key, objectid);

	return 0;
}

/* Updates key locality */
errno_t reiser4_key_set_locality(
	reiser4_key_t *key,	    /* key locality will be updated in */
	oid_t locality)	            /* new locality */
{
	aal_assert("umka-696", key != NULL);
	aal_assert("umka-697", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, set_locality,
		  key, locality);

	return 0;
}

/* Updates key ordering */
errno_t reiser4_key_set_ordering(
	reiser4_key_t *key,	    /* key, ordering will be updated in */
	uint64_t ordering)          /* new ordering */
{
	aal_assert("umka-2337", key != NULL);
	aal_assert("umka-2338", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, set_ordering,
		  key, ordering);

	return 0;
}

/* Returns key offset */
uint64_t reiser4_key_get_offset(reiser4_key_t *key) {
	aal_assert("umka-700", key != NULL);
	aal_assert("umka-701", key->plug != NULL);

	return plug_call(key->plug->o.key_ops,
			 get_offset, key);
}

/* Increases key's offset by passed @value */
void reiser4_key_inc_offset(reiser4_key_t *key, uint64_t value) {
	reiser4_key_set_offset(key, reiser4_key_get_offset(key) + value);
}

#ifndef ENABLE_STAND_ALONE
/* Gets key type */
uint32_t reiser4_key_get_type(reiser4_key_t *key) {
	aal_assert("umka-698", key != NULL);
	aal_assert("umka-699", key->plug != NULL);

	return plug_call(key->plug->o.key_ops,
			 get_type, key);
}

/* Returns key objectid */
oid_t reiser4_key_get_objectid(reiser4_key_t *key) {
	aal_assert("umka-702", key != NULL);
	aal_assert("umka-703", key->plug != NULL);

	return plug_call(key->plug->o.key_ops,
			 get_objectid, key);
}

/* Returns key locality */
oid_t reiser4_key_get_locality(reiser4_key_t *key) {
	aal_assert("umka-704", key != NULL);
	aal_assert("umka-705", key->plug != NULL);

	return plug_call(key->plug->o.key_ops,
			 get_locality, key);
}

/* Returns key locality */
uint64_t reiser4_key_get_ordering(reiser4_key_t *key) {
	aal_assert("umka-2335", key != NULL);
	aal_assert("umka-2336", key->plug != NULL);

	return plug_call(key->plug->o.key_ops,
			 get_ordering, key);
}
#endif

/* Returns the maximal possible key  */
void reiser4_key_maximal(reiser4_key_t *key) {
	key_entity_t *entity;
    
	aal_assert("vpf-185", key != NULL);
	aal_assert("vpf-186", key->plug != NULL);

	entity = plug_call(key->plug->o.key_ops, maximal);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

/* Returns the minimal possible key */
void reiser4_key_minimal(reiser4_key_t *key) {
	key_entity_t *entity;
    
	aal_assert("vpf-187", key != NULL);
	aal_assert("vpf-188", key->plug != NULL);

	entity = plug_call(key->plug->o.key_ops, minimal);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

#ifndef ENABLE_STAND_ALONE
/* Sets key hash component */
errno_t reiser4_key_set_hash(
	reiser4_key_t *key,	    /* key hash will be updated in */
	uint64_t hash)		    /* new hash value */
{
	aal_assert("umka-706", key != NULL);
	aal_assert("umka-707", key->plug != NULL);
    
	plug_call(key->plug->o.key_ops, set_hash, key, hash);
    
	return 0;
}

/* Returns key hash */
uint64_t reiser4_key_get_hash(reiser4_key_t *key) {
	aal_assert("umka-708", key != NULL);
	aal_assert("umka-709", key->plug != NULL);

	return plug_call(key->plug->o.key_ops, get_hash, key);
}

errno_t reiser4_key_print(reiser4_key_t *key, 
			  aal_stream_t *stream, 
			  uint16_t options)
{
	aal_assert("vpf-189", key != NULL);
	aal_assert("vpf-190", key->plug != NULL);

	return plug_call(key->plug->o.key_ops, print,
			 key, stream, options); 
}
#endif
