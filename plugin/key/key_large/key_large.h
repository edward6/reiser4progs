/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large.h -- reiser4 large key structures. */

#ifndef KEY_LARGE_H
#define KEY_LARGE_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

typedef enum {
	/* File name key type */
	KEY_LARGE_FILENAME_MINOR = 0,
    
	/* Stat-data key type */
	KEY_LARGE_STATDATA_MINOR = 1,
    
	/* File attribute name */
	KEY_LARGE_ATTRNAME_MINOR = 2,
    
	/* File attribute value */
	KEY_LARGE_ATTRBODY_MINOR = 3,
    
	/* File body (tail or extent) */
	KEY_LARGE_FILEBODY_MINOR = 4,
	KEY_LARGE_LAST_MINOR
} key_large_minor_t;

typedef enum {
	/* Major "locale", aka dirid. Sits in 1st element */
	KEY_LARGE_LOCALITY_INDEX   = 0,
    
	/* Minor "locale", aka item type. Sits in 1st element */
	KEY_LARGE_TYPE_INDEX       = 0,
    
	/* Object ordering. Sits in 2nd element */
	KEY_LARGE_ORDERING_INDEX   = 1,
	
	/* Object band. Sits in 3nd element */
	KEY_LARGE_BAND_INDEX       = 2,
    
	/* Object id. Sits in 3nd element */
	KEY_LARGE_OBJECTID_INDEX   = 2,
	KEY_LARGE_FOBJECTID_INDEX  = 2,
    
	/* Offset. Sits in 4rd element */
	KEY_LARGE_OFFSET_INDEX     = 3,
    
	/* Name hash. Sits in 4rd element */
	KEY_LARGE_HASH_INDEX       = 3,
	KEY_LARGE_LAST_INDEX
} key_large_field_t;

union key_large {
	d64_t el[KEY_LARGE_LAST_INDEX];
	int pad;
};

typedef union key_large key_large_t;

typedef enum {
	/* Major locality occupies higher 60 bits of the first element */
	KEY_LARGE_LOCALITY_MASK    = 0xfffffffffffffff0ull,
    
	/* Minor locality occupies lower 4 bits of the first element */
	KEY_LARGE_TYPE_MASK        = 0xfull,
    
	/* Controversial band occupies higher 4 bits of the 2nd element */
	KEY_LARGE_BAND_MASK        = 0xf000000000000000ull,
    
	/* Objectid occupies lower 60 bits of the 2nd element */
	KEY_LARGE_OBJECTID_MASK    = 0x0fffffffffffffffull,
	KEY_LARGE_FOBJECTID_MASK   = 0xffffffffffffffffull,
    
	/* Offset is just 3rd L.M.Nt itself */
	KEY_LARGE_OFFSET_MASK      = 0xffffffffffffffffull,

	/* Ordering mask */
	KEY_LARGE_ORDERING_MASK    = 0xffffffffffffffffull,
	
	/* Hash occupies 56 higher bits of 3rd element */
	KEY_LARGE_HASH_MASK        = 0xffffffffffffff00ull
} key_large_mask_t;

#define OFFSET_CHARS   (sizeof(uint64_t))
#define OBJECTID_CHARS (sizeof(uint64_t))
#define ORDERING_CHARS (sizeof(uint64_t) - 1)

#define INLINE_CHARS   (ORDERING_CHARS + OBJECTID_CHARS)

typedef enum {
	KEY_LARGE_LOCALITY_SHIFT   = 4,
	KEY_LARGE_TYPE_SHIFT       = 0,
	KEY_LARGE_BAND_SHIFT       = 60,
	KEY_LARGE_ORDERING_SHIFT   = 0,
	KEY_LARGE_OBJECTID_SHIFT   = 0,
	KEY_LARGE_FOBJECTID_SHIFT  = 0,
	KEY_LARGE_OFFSET_SHIFT     = 0,
	KEY_LARGE_HASH_SHIFT       = 8,
	KEY_LARGE_GEN_SHIFT        = 0,
} key_large_shift_t;

static inline uint64_t kl_get_el(const key_large_t *key,
				 key_large_field_t off)
{
	aal_assert("vpf-029", key != NULL);
	aal_assert("vpf-030", off < KEY_LARGE_LAST_INDEX);
	
	return LE64_TO_CPU(key->el[off]);
}

static inline void kl_set_el(key_large_t *key,
			     key_large_field_t off,
			     uint64_t value)
{
	aal_assert("vpf-031", key != NULL);
	aal_assert("vpf-032", off < KEY_LARGE_LAST_INDEX);
	
	key->el[off] = CPU_TO_LE64(value);
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
        aal_assert("vpf-036", key != NULL);                         \
        return (T) ((kl_get_el(key, KEY_LARGE_##U##_INDEX) &	    \
	            KEY_LARGE_##U##_MASK) >> KEY_LARGE_##U##_SHIFT);\
}								    \
								    \
static inline void kl_set_##L(key_large_t *key, T loc) {	    \
        uint64_t el;						    \
								    \
        aal_assert("vpf-033", key != NULL);                         \
								    \
        el = kl_get_el(key, KEY_LARGE_##U##_INDEX);		    \
								    \
        el &= ~KEY_LARGE_##U##_MASK;				    \
								    \
        el |= (loc << KEY_LARGE_##U##_SHIFT);			    \
        kl_set_el(key, KEY_LARGE_##U##_INDEX, el);		    \
}

KEY_LARGE_FIELD_HANDLER(locality, LOCALITY, uint64_t);
KEY_LARGE_FIELD_HANDLER(ordering, ORDERING, uint64_t);
KEY_LARGE_FIELD_HANDLER(minor, TYPE, key_large_minor_t);
KEY_LARGE_FIELD_HANDLER(band, BAND, uint64_t);
KEY_LARGE_FIELD_HANDLER(objectid, OBJECTID, uint64_t);
KEY_LARGE_FIELD_HANDLER(fobjectid, FOBJECTID, uint64_t);
KEY_LARGE_FIELD_HANDLER(offset, OFFSET, uint64_t);
KEY_LARGE_FIELD_HANDLER(hash, HASH, uint64_t);
#endif
