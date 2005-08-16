/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large.h -- reiser4 large key structures. */

#ifndef KEY_LARGE_H
#define KEY_LARGE_H

#include <aal/libaal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>
#include "plugin/key/key_common/key_common.h"

typedef enum {
	/* Major "locale", aka dirid. Sits in 1st element */
	KEY_LARGE_LOCALITY_INDEX	= 0,
    
	/* Minor "locale", aka item type. Sits in 1st element */
	KEY_LARGE_TYPE_INDEX		= 0,

	/* Object ordering. Sits in 2nd element */
	KEY_LARGE_ORDERING_INDEX	= 1,
	
	/* Object band. Sits in 3nd element */
	KEY_LARGE_BAND_INDEX		= 2,
    
	/* Object id. Sits in 3nd element */
	KEY_LARGE_OBJECTID_INDEX	= 2,
	KEY_LARGE_FOBJECTID_INDEX	= 2,
    
	/* Offset. Sits in 4rd element */
	KEY_LARGE_OFFSET_INDEX		= 3,
    
	/* Name hash. Sits in 4rd element */
	KEY_LARGE_HASH_INDEX		= 3,
	KEY_LARGE_LAST_INDEX
} key_large_field_t;

typedef union key_large {
	d64_t el[KEY_LARGE_LAST_INDEX];
	int pad;
} key_large_t;

typedef enum {
	/* Major locality occupies higher 60 bits of the first element */
	KEY_LARGE_LOCALITY_MASK		= 0xfffffffffffffff0ull,
    
	/* Minor locality occupies lower 4 bits of the first element */
	KEY_LARGE_TYPE_MASK		= 0xfull,

	/* Ordering occupies the whole 2nd element. */
	KEY_LARGE_ORDERING_MASK		= 0xffffffffffffffffull,
    
	/* Controversial band occupies higher 4 bits of the 3rd element */
	KEY_LARGE_BAND_MASK		= 0xf000000000000000ull,
    
	/* Objectid occupies lower 60 bits of the 3rd element */
	KEY_LARGE_OBJECTID_MASK		= 0x0fffffffffffffffull,
	KEY_LARGE_FOBJECTID_MASK	= 0xffffffffffffffffull,
    
	/* Offset is just 4th L.M.Nt itself */
	KEY_LARGE_OFFSET_MASK		= 0xffffffffffffffffull,

	/* Hash occupies 56 higher bits of 4th element */
	KEY_LARGE_HASH_MASK		= 0xffffffffffffff00ull
} key_large_mask_t;

#define OFFSET_CHARS		(sizeof(uint64_t))
#define OBJECTID_CHARS		(sizeof(uint64_t))
#define ORDERING_CHARS		(sizeof(uint64_t) - 1)

#define INLINE_CHARS		(ORDERING_CHARS + OBJECTID_CHARS)

#define HASHED_NAME_MASK	0x0100000000000000ull
#define FIBRE_MASK		0xff00000000000000ull
#define FIBRE_SHIFT		57

typedef enum {
	KEY_LARGE_LOCALITY_SHIFT	= 4,
	KEY_LARGE_TYPE_SHIFT		= 0,
	KEY_LARGE_ORDERING_SHIFT	= 0,
	KEY_LARGE_BAND_SHIFT		= 60,
	KEY_LARGE_OBJECTID_SHIFT	= 0,
	KEY_LARGE_FOBJECTID_SHIFT	= 0,
	KEY_LARGE_OFFSET_SHIFT		= 0,
	KEY_LARGE_HASH_SHIFT		= 8,
	KEY_LARGE_GEN_SHIFT		= 0,
} key_large_shift_t;

#ifndef ENABLE_MINIMAL
extern void key_large_set_offset(reiser4_key_t *key, 
				 uint64_t offset);
extern uint64_t key_large_get_offset(reiser4_key_t *key);

extern void key_large_set_objectid(reiser4_key_t *key, 
				   uint64_t objectid);

extern uint64_t key_large_get_objectid(reiser4_key_t *key);

extern void key_large_set_ordering(reiser4_key_t *key,
				   uint64_t ordering);

extern uint64_t key_large_get_ordering(reiser4_key_t *key);

extern key_type_t key_large_get_type(reiser4_key_t *key);

extern void key_large_set_locality(reiser4_key_t *key,
				   uint64_t locality);

extern uint64_t key_large_get_locality(reiser4_key_t *key);

extern void key_large_set_fobjectid(reiser4_key_t *key,
				    uint64_t objectid);

extern uint64_t key_large_get_fobjectid(reiser4_key_t *key);
#endif

static inline uint64_t kl_get_el(const key_large_t *key,
				 key_large_field_t off)
{
	return LE64_TO_CPU(get_unaligned(key->el + off));
}

static inline void kl_set_el(key_large_t *key,
			     key_large_field_t off,
			     uint64_t value)
{
	put_unaligned(CPU_TO_LE64(value), key->el + off);
}

static inline int kl_comp_el(void *k1, void *k2, int off) {
	uint64_t e1 = kl_get_el(k1, off);
	uint64_t e2 = kl_get_el(k2, off);

	return (e1 < e2 ? -1 : (e1 == e2 ? 0 : 1));
}

/* Macro to define key_large getter and setter functions for field F with type
   T. It is used for minimize code. */
#define KEY_LARGE_FIELD_HANDLER(L, U, T)			    \
static inline T kl_get_##L (const key_large_t *key) {               \
        return (T) ((kl_get_el(key, KEY_LARGE_##U##_INDEX) &	    \
	            KEY_LARGE_##U##_MASK) >> KEY_LARGE_##U##_SHIFT);\
}								    \
								    \
static inline void kl_set_##L(key_large_t *key, T loc) {	    \
        uint64_t el;						    \
								    \
        el = kl_get_el(key, KEY_LARGE_##U##_INDEX);		    \
								    \
        el &= ~KEY_LARGE_##U##_MASK;				    \
								    \
        el |= (loc << KEY_LARGE_##U##_SHIFT);			    \
        kl_set_el(key, KEY_LARGE_##U##_INDEX, el);		    \
}

KEY_LARGE_FIELD_HANDLER(locality, LOCALITY, uint64_t);
KEY_LARGE_FIELD_HANDLER(minor, TYPE, key_minor_t);
KEY_LARGE_FIELD_HANDLER(ordering, ORDERING, uint64_t);
KEY_LARGE_FIELD_HANDLER(band, BAND, uint64_t);
KEY_LARGE_FIELD_HANDLER(objectid, OBJECTID, uint64_t);
KEY_LARGE_FIELD_HANDLER(fobjectid, FOBJECTID, uint64_t);
KEY_LARGE_FIELD_HANDLER(offset, OFFSET, uint64_t);
KEY_LARGE_FIELD_HANDLER(hash, HASH, uint64_t);
#endif
