/*
  key.c -- reiser4 common key code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/  

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

static errno_t callback_guess_key(
	reiser4_plugin_t *plugin,	    /* plugin to be checked */
	void *data)			    /* item ot be checked */
{
	if (plugin->h.type != KEY_PLUGIN_TYPE)
		return 0;
	
	return plugin_call(plugin->o.key_ops, confirm,
			   (key_entity_t *)data);
}

errno_t reiser4_key_guess(reiser4_key_t *key) {
	aal_assert("umka-907", key != NULL);

	key->plugin = libreiser4_factory_cfind(callback_guess_key,
					       (void *)key);

	if (key->plugin == NULL)
		return -EINVAL;
	
	return 0;
}

/* 
   Compares two keys in plugin independent maner by means of using one of passed
   keys plugin.
*/
int reiser4_key_compare(
	reiser4_key_t *key1,	    /* the first key for comparing */
	reiser4_key_t *key2)	    /* the second one */
{
	aal_assert("umka-764", key1 != NULL);
	aal_assert("umka-765", key2 != NULL);

	aal_assert("umka-906", key1->plugin != NULL);
	aal_assert("umka-906", key2->plugin != NULL);

	return plugin_call(key1->plugin->o.key_ops, 
		compare, key1, key2);
}

/* Makes copy src key to dst one */
errno_t reiser4_key_assign(
	reiser4_key_t *dst,	    /* destination key */
	reiser4_key_t *src)	    /* source key */
{
	aal_assert("umka-1112", dst != NULL);
	aal_assert("umka-1113", src != NULL);
	aal_assert("umka-1114", src->plugin != NULL);

	dst->plugin = src->plugin;
	
	return plugin_call(src->plugin->o.key_ops, assign, dst, src);
}

/* Cleans specified key */
void reiser4_key_clean(
	reiser4_key_t *key)	    /* key to be clean */
{
	aal_assert("umka-675", key != NULL);
	aal_assert("umka-676", key->plugin != NULL);
    
	plugin_call(key->plugin->o.key_ops, clean, key);
} 

/* Builds full non-directory key */
errno_t reiser4_key_build_generic(
	reiser4_key_t *key,	    /* key to be built */
	uint32_t type,		    /* key type to be used */
	oid_t locality,	    /* locality to be used */
	oid_t objectid,	    /* objectid to be used */
	uint64_t offset)	    /* offset to be used */
{
	aal_assert("umka-665", key != NULL);
	aal_assert("umka-666", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, build_generic,
			   key, type, locality, objectid, offset);
}

/* Builds full directory key */
errno_t reiser4_key_build_entry(
	reiser4_key_t *key,	    /* key to be built */
	reiser4_plugin_t *plugin,   /* hash plugin to be used */
	oid_t locality,	    /* loaclity to be used */
	oid_t objectid,	    /* objectid to be used */
	const char *name)	    /* entry name to be hashed */
{
	aal_assert("umka-668", key != NULL);
	aal_assert("umka-670", name != NULL);
	aal_assert("umka-669", key->plugin != NULL);
    
	return plugin_call(key->plugin->o.key_ops, build_entry, key,
			   plugin, locality, objectid, name);
}

/* Sets key type */
errno_t reiser4_key_set_type(
	reiser4_key_t *key,	    /* key type will be updated in */
	uint32_t type)		    /* new key type */
{
	aal_assert("umka-686", key != NULL);
	aal_assert("umka-687", key->plugin != NULL);

	plugin_call(key->plugin->o.key_ops, set_type, key, type);
    
	return 0;
}

/* Sets key offset */
errno_t reiser4_key_set_offset(
	reiser4_key_t *key,	    /* key to be updated */
	uint64_t offset)	    /* new offset */
{
	aal_assert("umka-688", key != NULL);
	aal_assert("umka-689", key->plugin != NULL);
    
	plugin_call(key->plugin->o.key_ops, set_offset, key, offset);
    
	return 0;
}

/* Updates key objectid */
errno_t reiser4_key_set_objectid(
	reiser4_key_t *key,	    /* key objectid will be updated in */
	oid_t objectid)	    /* new objectid */
{
	aal_assert("umka-694", key != NULL);
	aal_assert("umka-695", key->plugin != NULL);
    
	plugin_call(key->plugin->o.key_ops, set_objectid,
		    key, objectid);

	return 0;
}

/* Updates key locality */
errno_t reiser4_key_set_locality(
	reiser4_key_t *key,	    /* key locality will be updated in */
	oid_t locality)	    /* new locality */
{
	aal_assert("umka-696", key != NULL);
	aal_assert("umka-697", key->plugin != NULL);
    
	plugin_call(key->plugin->o.key_ops, set_locality, key, locality);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Gets key type */
uint32_t reiser4_key_get_type(reiser4_key_t *key) {
	aal_assert("umka-698", key != NULL);
	aal_assert("umka-699", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, get_type, key);
}

/* Returns key offset */
uint64_t reiser4_key_get_offset(reiser4_key_t *key) {
	aal_assert("umka-700", key != NULL);
	aal_assert("umka-701", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, get_offset, key);
}

/* Returns key objectid */
oid_t reiser4_key_get_objectid(reiser4_key_t *key) {
	aal_assert("umka-702", key != NULL);
	aal_assert("umka-703", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, get_objectid, key);
}

/* Returns key locality */
oid_t reiser4_key_get_locality(reiser4_key_t *key) {
	aal_assert("umka-704", key != NULL);
	aal_assert("umka-705", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, get_locality, key);
}
#endif

/* Returns the maximal possible key  */
void reiser4_key_maximal(reiser4_key_t *key) {
	key_entity_t *entity;
    
	aal_assert("vpf-185", key != NULL);
	aal_assert("vpf-186", key->plugin != NULL);

	entity = plugin_call(key->plugin->o.key_ops, maximal,);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

/* Returns the minimal possible key */
void reiser4_key_minimal(reiser4_key_t *key) {
	key_entity_t *entity;
    
	aal_assert("vpf-187", key != NULL);
	aal_assert("vpf-188", key->plugin != NULL);

	entity = plugin_call(key->plugin->o.key_ops, minimal,);
	aal_memcpy(key->body, entity->body, sizeof(key->body));
}

#ifndef ENABLE_STAND_ALONE
/* Sets key hash component */
errno_t reiser4_key_set_hash(
	reiser4_key_t *key,	    /* key hash will be updated in */
	uint64_t hash)		    /* new hash value */
{
	aal_assert("umka-706", key != NULL);
	aal_assert("umka-707", key->plugin != NULL);
    
	plugin_call(key->plugin->o.key_ops, set_hash, key, hash);
    
	return 0;
}

/* Returns key hash */
uint64_t reiser4_key_get_hash(reiser4_key_t *key) {
	aal_assert("umka-708", key != NULL);
	aal_assert("umka-709", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, get_hash, key);
}

errno_t reiser4_key_print(reiser4_key_t *key, aal_stream_t *stream) {
	aal_assert("vpf-189", key != NULL);
	aal_assert("vpf-190", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, print,
			   key, stream, 0); 
}

errno_t reiser4_key_string(reiser4_key_t *key, char *buff) {
	aal_stream_t stream;

	aal_stream_init(&stream);

	if (reiser4_key_print(key, &stream))
		goto error_free_stream;
	
	aal_strncpy(buff, stream.data, stream.offset);
	aal_stream_fini(&stream);

	return 0;

 error_free_stream:
	aal_stream_fini(&stream);
	return -EINVAL;
}

errno_t reiser4_key_valid(reiser4_key_t *key) {
	aal_assert("vpf-259", key != NULL);
	aal_assert("vpf-260", key->plugin != NULL);

	return plugin_call(key->plugin->o.key_ops, valid, key);
}

#endif
