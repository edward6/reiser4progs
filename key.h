/*
 * Declarations of key-related data-structures and operations on keys.
 *
 * Copyright 2000, 2001, 2002 by Hans Reiser, licensing governed by reiserfs/README
 *
 */

#if !defined( __REISER4_KEY_H__ )
#define __REISER4_KEY_H__

/* Operations on keys in reiserfs tree */

/* No access to any of these fields shall be done except via a
   wrapping macro/function, and that wrapping macro/function shall
   convert to little endian order.  Compare keys will consider cpu byte order. */

/* The difference between a file and an attribute, is that all of the
   attributes of a file are near in key to all of the other attributes
   for all of the files within that directory.  It is interesting to
   consider whether this is the wrong approach, and whether there
   should be no difference at all. */

/* possible values for minor packing locality (4 bits required) */
typedef enum {
	/** file name */
	KEY_FILE_NAME_MINOR = 0,
	/** stat-data */
	KEY_SD_MINOR        = 1,
	/** file attribute name */
        KEY_ATTR_NAME_MINOR = 2,
	/** file attribute value */
	KEY_ATTR_BODY_MINOR = 3,
	/** file body (tail or extent) */
	KEY_BODY_MINOR      = 4,
} key_minor_locality;

/* everything stored in the tree has a unique key, which means that
   the tree is (logically) fully ordered by key.  Physical order is
   determined by dynamic heuristics that attempt to reflect key order
   when allocating available space, and by a repacker which is not yet
   written.  It is stylistically better to put aggregation information
   into the key.  Thus, if you want to segregate extents from tails,
   it is better to give them distinct minor packing localities rather
   than changing block_alloc.c to check the node type when deciding
   where to allocate the node.  The need to randomly displace new
   directories and large files disturbs this symmetry unfortunately.
   However, if tails have a different minor packing locality from
   extents, and no files have both extents and tails, maybe symmetry
   can be had. */

/* 

Arbitrary major packing localities can be assigned to objects using
the reiser4(filenameA/..packing<=some_number) system call.

In reiser4, the creat() syscall creates a directory

* whose default flow (that which is referred to if the directory is
  read as a file) is the traditional unix file body.

* whose directory plugin is the 'filedir'

* whose major packing locality is that of the parent of the object created.

The static_stat item is a particular commonly used directory
compression (the one for normal unix files).

The filedir plugin checks to see if the static_stat item exists.
There is a unique key for static_stat.  If yes, then it uses the
static_stat item for all of the values that it contains.  The
static_stat item contains a flag for each stat it contains which
indicates whether one should look outside the static_stat item for its
contents.

*/

/** offset of fields in reiser4_key. Value of each element of this enum
    is index within key (thought as array of __u64's) where this field
    is. */
typedef enum {
	/** major "locale", aka dirid. Sits in 1st element */
	KEY_LOCALITY_INDEX   = 0,
	/** minor "locale", aka item type. Sits in 1st element */
	KEY_TYPE_INDEX       = 0,
	/** "object band". Sits in 2nd element */
	KEY_BAND_INDEX       = 1,
	/** objectid. Sits in 2nd element */
	KEY_OBJECTID_INDEX   = 1,
	/** Offset. Sits in 3rd element */
	KEY_OFFSET_INDEX     = 2,
	/** Name hash. Sits in 3rd element */
	KEY_HASH_INDEX       = 2,
	KEY_LAST_INDEX       = 3
} reiser4_key_field_index;

/** key in reiser4 internal "balanced" tree. It is just array of three
    64bit integers in disk byte order (little-endian by default). This
    array is actually indexed by reiser4_key_field.  Each __u64 within
    this array is called "element". Logical key component encoded within
    elements are called "fields". 

    We declare this as union with second component dummy to suppress
    inconvenient array<->pointer casts implied in C. */
union reiser4_key {
	d64 el[ KEY_LAST_INDEX ];
	int pad;
};

/** bitmasks showing where within reiser4_key particular key is
    stored. */
typedef enum {
	/** major locality occupies higher 60 bits of the first element */
	KEY_LOCALITY_MASK    = 0xfffffffffffffff0ull,
	/** minor locality occupies lower 4 bits of the first element */
	KEY_TYPE_MASK        = 0xfull,
	/** controversial band occupies higher 4 bits of the 2nd element */
	KEY_BAND_MASK        = 0xf000000000000000ull,
	/** objectid occupies lower 60 bits of the 2nd element */
	KEY_OBJECTID_MASK    = 0x0fffffffffffffffull,
	/** offset is just 3rd L.M.Nt itself */
	KEY_OFFSET_MASK      = 0xffffffffffffffffull,
	/** hash occupies 56 higher bits of 3rd element */
	KEY_HASH_MASK        = 0xffffffffffffff00ull,
	/** generation counter occupies lower 8 bits of 3rd element */
	KEY_GEN_MASK         = 0xffull,
} reiser4_key_field_mask;

/** how many bits key element should be shifted to left to get particular field */
typedef enum {
	KEY_LOCALITY_SHIFT   = 4,
	KEY_TYPE_SHIFT       = 0,
	KEY_BAND_SHIFT       = 60,
	KEY_OBJECTID_SHIFT   = 0,
	KEY_OFFSET_SHIFT     = 0,
	KEY_HASH_SHIFT       = 8,
	KEY_GEN_SHIFT        = 0,
} reiser4_key_field_shift;

static inline __u64 get_key_el( const reiser4_key *key, 
				reiser4_key_field_index off )
{
	assert( "nikita-753", key != NULL );
	assert( "nikita-754", off < KEY_LAST_INDEX );
	return d64tocpu( &key -> el[ off ] );
}

static inline void set_key_el( reiser4_key *key, 
			       reiser4_key_field_index off, __u64 value )
{
	assert( "nikita-755", key != NULL );
	assert( "nikita-756", off < KEY_LAST_INDEX );
	cputod64( value, &key -> el[ off ] );
}

/** macro to define getter and setter functions for field F with type T */
#define DEFINE_KEY_FIELD( L, U, T )					\
static inline T get_key_ ## L ( const reiser4_key *key )		\
{									\
	assert( "nikita-750", key != NULL );				\
	return ( T ) ( get_key_el( key, KEY_ ## U ## _INDEX ) &		\
		 KEY_ ## U ## _MASK ) >> KEY_ ## U ## _SHIFT;		\
}									\
									\
static inline void set_key_ ## L ( reiser4_key *key, T loc )		\
{									\
	__u64 el;							\
									\
	assert( "nikita-752", key != NULL );				\
									\
	el = get_key_el( key, KEY_ ## U ## _INDEX );			\
	/* clear field bits in the key */				\
	el &= ~KEY_ ## U ## _MASK;					\
	/* actually it should be					\
									\
	   el |= ( loc << KEY_ ## U ## _SHIFT ) & KEY_ ## U ## _MASK;	\
									\
	   but we trust user to never pass values that wouldn't fit	\
	   into field. Clearing extra bits is one operation, but this	\
	   function is time-critical.					\
	   But check this in assertion. */				\
	assert( "nikita-759", ( ( loc << KEY_ ## U ## _SHIFT ) &	\
		~KEY_ ## U ## _MASK ) == 0 );				\
	el |= ( loc << KEY_ ## U ## _SHIFT );				\
	set_key_el( key, KEY_ ## U ## _INDEX, el );			\
}

typedef __u64 oid_t;

/* define get_key_locality(), set_key_locality() */
DEFINE_KEY_FIELD( locality, LOCALITY, oid_t );
/* define get_key_type(), set_key_type() */
DEFINE_KEY_FIELD( type, TYPE, key_minor_locality );
/* define get_key_band(), set_key_band() */
DEFINE_KEY_FIELD( band, BAND, __u64 );
/* define get_key_objectid(), set_key_objectid() */
DEFINE_KEY_FIELD( objectid, OBJECTID, oid_t );
/* define get_key_offset(), set_key_offset() */
DEFINE_KEY_FIELD( offset, OFFSET, __u64 );
/* define get_key_hash(), set_key_hash() */
DEFINE_KEY_FIELD( hash, HASH, __u64 );

/** key comparison result */
typedef enum { LESS_THAN = -1,	/* if first key is less than second */
	       EQUAL_TO = 0,	/* if keys are equal */
	       GREATER_THAN = +1 /* if first key is greater than second */
} cmp_t;

void key_init( reiser4_key *key );

/** minimal possible key in the tree. Return pointer to the static storage. */
extern const reiser4_key *min_key( void );
extern const reiser4_key *max_key( void );
extern cmp_t keycmp( const reiser4_key *k1, const reiser4_key *k2 );
extern int   keyeq ( const reiser4_key *k1, const reiser4_key *k2 );
extern int   keylt ( const reiser4_key *k1, const reiser4_key *k2 );
extern int   keyle ( const reiser4_key *k1, const reiser4_key *k2 );
extern int   keygt ( const reiser4_key *k1, const reiser4_key *k2 );
extern int   keyge ( const reiser4_key *k1, const reiser4_key *k2 );

extern void print_key( const char *prefix, const reiser4_key *key );
extern int  sprintf_key( char *buffer, const reiser4_key *key );

/* __FS_REISERFS_KEY_H__ */
#endif

/* 
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
