/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_short.h -- reiser4 short key structures. */

#ifndef KEY_SHORT_H
#define KEY_SHORT_H

#include <aal/libaal.h>
#include <aux/aux.h>

#include <reiser4/plugin.h>
#include "plugin/key/key_common/key_common.h"

typedef enum {
	/* Major "locale", aka dirid. Sits in 1st element */
	KEY_SHORT_LOCALITY_INDEX   = 0,
    
	/* Minor "locale", aka item type. Sits in 1st element */
	KEY_SHORT_TYPE_INDEX       = 0,
    
	/* Object band. Sits in 2nd element */
	KEY_SHORT_BAND_INDEX       = 1,
    
	/* Object id. Sits in 2nd element */
	KEY_SHORT_OBJECTID_INDEX   = 1,
	KEY_SHORT_FOBJECTID_INDEX  = 1,
    
	/* Offset. Sits in 3rd element */
	KEY_SHORT_OFFSET_INDEX     = 2,
    
	/* Name hash. Sits in 3rd element */
	KEY_SHORT_HASH_INDEX       = 2,
	KEY_SHORT_LAST_INDEX
    
} key_short_field_t;

union key_short {
	d64_t el[KEY_SHORT_LAST_INDEX];
	int pad;
};

typedef union key_short key_short_t;

typedef enum {
	/* Major locality occupies higher 60 bits of the first element */
	KEY_SHORT_LOCALITY_MASK    = 0xfffffffffffffff0ull,
    
	/* Minor locality occupies lower 4 bits of the first element */
	KEY_SHORT_TYPE_MASK        = 0xfull,
    
	/* Controversial band occupies higher 4 bits of the 2nd element */
	KEY_SHORT_BAND_MASK        = 0xf000000000000000ull,
    
	/* Objectid occupies lower 60 bits of the 2nd element */
	KEY_SHORT_OBJECTID_MASK    = 0x0fffffffffffffffull,
	KEY_SHORT_FOBJECTID_MASK   = 0xffffffffffffffffull,
    
	/* Offset is just 3rd L.M.Nt itself */
	KEY_SHORT_OFFSET_MASK      = 0xffffffffffffffffull,
    
	/* Hash occupies 56 higher bits of 3rd element */
	KEY_SHORT_HASH_MASK        = 0xffffffffffffff00ull,
} key_short_mask_t;

#define OFFSET_CHARS		(sizeof(uint64_t))
#define OBJECTID_CHARS		(sizeof(uint64_t) - 1)

#define HASHED_NAME_MASK	0x0100000000000000ull
#define FIBRE_MASK		0xff00000000000000ull
#define FIBRE_SHIFT		57

typedef enum {
	KEY_SHORT_LOCALITY_SHIFT   = 4,
	KEY_SHORT_TYPE_SHIFT       = 0,
	KEY_SHORT_BAND_SHIFT       = 60,
	KEY_SHORT_OBJECTID_SHIFT   = 0,
	KEY_SHORT_FOBJECTID_SHIFT  = 0,
	KEY_SHORT_OFFSET_SHIFT     = 0,
	KEY_SHORT_HASH_SHIFT       = 8,
	KEY_SHORT_GEN_SHIFT        = 0,
} key_short_shift_t;

#ifndef ENABLE_STAND_ALONE
extern void key_short_set_offset(reiser4_key_t *key, 
				 uint64_t offset);

extern uint64_t key_short_get_offset(reiser4_key_t *key);

extern void key_short_set_objectid(reiser4_key_t *key, 
				   uint64_t objectid);

extern uint64_t key_short_get_objectid(reiser4_key_t *key);

extern key_type_t key_short_get_type(reiser4_key_t *key);

extern void key_short_set_locality(reiser4_key_t *key,
				   uint64_t locality);

extern uint64_t key_short_get_locality(reiser4_key_t *key);

extern void key_short_set_fobjectid(reiser4_key_t *key,
				    uint64_t objectid);

extern uint64_t key_short_get_fobjectid(reiser4_key_t *key);
#endif

static inline uint64_t ks_get_el(const key_short_t *key,
				 key_short_field_t off)
{
	return LE64_TO_CPU(key->el[off]);
}

static inline void ks_set_el(key_short_t *key,
			     key_short_field_t off,
			     uint64_t value)
{
	key->el[off] = CPU_TO_LE64(value);
}

static inline int ks_comp_el(void *k1, void *k2, int off) {
	uint64_t e1 = ks_get_el(k1, off);
	uint64_t e2 = ks_get_el(k2, off);

	return (e1 < e2 ? -1 : (e1 == e2 ? 0 : 1));
}

/* Macro to define key_short getter and setter functions for field F with type
   T. It is used for minimize code. */
								    
#define KEY_SHORT_FIELD_HANDLER(L, U, T)			    \
static inline T ks_get_##L (const key_short_t *key) {               \
        return (T) ((ks_get_el(key, KEY_SHORT_##U##_INDEX) &	    \
	            KEY_SHORT_##U##_MASK) >> KEY_SHORT_##U##_SHIFT);\
}								    \
								    \
static inline void ks_set_##L(key_short_t *key, T loc) {	    \
        uint64_t el;						    \
								    \
        el = ks_get_el(key, KEY_SHORT_##U##_INDEX);		    \
								    \
        el &= ~KEY_SHORT_##U##_MASK;				    \
								    \
        el |= (loc << KEY_SHORT_##U##_SHIFT);			    \
        ks_set_el(key, KEY_SHORT_##U##_INDEX, el);		    \
}

KEY_SHORT_FIELD_HANDLER(locality, LOCALITY, uint64_t);
KEY_SHORT_FIELD_HANDLER(minor, TYPE, key_minor_t);
KEY_SHORT_FIELD_HANDLER(band, BAND, uint64_t);
KEY_SHORT_FIELD_HANDLER(objectid, OBJECTID, uint64_t);
KEY_SHORT_FIELD_HANDLER(fobjectid, FOBJECTID, uint64_t);
KEY_SHORT_FIELD_HANDLER(offset, OFFSET, uint64_t);
KEY_SHORT_FIELD_HANDLER(hash, HASH, uint64_t);
#endif
