/* Copyright (C) 2001, 2002 Hans Reiser.  All rights reserved.
 */

/* TS stands for TYPE SAFETY */

/* Modify the header to make this clear and remove my comments. */
/* A hash table class that uses hash chains (singly-linked) and is parametrized to
 * provide type safety.  */

#ifndef __REISER4_TSHASH_H__
#define __REISER4_TSHASH_H__

/* FIXME_JMACD Nikita says this should use doubly linked list. -josh */

#include <asm/errno.h>
/* what are table and link objects? -Hans */
/* Step 1: Use TS_HASH_DECLARE() to define the TABLE and LINK objects
 * based on the object type.  You need to declare the item type before
 * this definition, define it after this definition. */
#define TS_HASH_DECLARE(PREFIX,ITEM_TYPE)                                                     \
                                                                                              \
typedef struct PREFIX##_hash_table_  PREFIX##_hash_table;                                     \
typedef struct PREFIX##_hash_link_   PREFIX##_hash_link;                                      \
                                                                                              \
struct PREFIX##_hash_table_                                                                   \
{                                                                                             \
  ITEM_TYPE **_table;                                                                         \
  __u32       _buckets;                                                                       \
};                                                                                            \
                                                                                              \
struct PREFIX##_hash_link_                                                                    \
{                                                                                             \
  ITEM_TYPE *_next;                                                                           \
}
/* what is an object type of the hash?  What is a prefix? */
/* Step 2: Define the object type of the hash: give it field of type
 * PREFIX_hash_link. */

/* Step 3: Use TS_HASH_DEFINE to define the hash table interface using
 * the type and field name used in step 3.  The arguments are:
 *

put this explanation at the top of this file.

 * ITEM_TYPE    The item type being hashed
 * KEY_TYPE     The type of key being hashed
 * KEY_NAME     The name of the key field within the item
 * LINK_NAME    The name of the link field within the item, which you must make type PREFIX_hash_link)
 * HASH_FUNC    The name of the hash function (or macro, takes const pointer to key)
sentence below is not enough.
 * EQ_FUNC      The name of the equality function (or macro, takes const pointer to two keys)
 *
 * It implements these functions:
 *
 * prefix_hash_init           Initialize the table given its size.
 * prefix_hash_insert         Insert an item
 * prefix_hash_insert_index   Insert an item w/ precomputed hash_index
 * prefix_hash_find           Find an item by key
 * prefix_hash_find_index     Find an item w/ precomputed hash_index
 * prefix_hash_remove         Remove an item, returns 1 if found, 0 if not found
 * prefix_hash_remove_index   Remove an item w/ precomputed hash_index
 *
 * If you'd like something to be done differently, feel free to ask me
 * for modifications.  Additional features that could be added but
 * have not been:
 *
 * prefix_hash_remove_key           Find and remove an item by key
 * prefix_hash_remove_key_index     Find and remove an item by key w/ precomputed hash_index
 *
 * The hash_function currently receives only the key as an argument,
 * meaning it must somehow know the number of buckets.  If this is a
 * problem let me know.
 *
 * This hash table uses a single-linked hash chain.  This means
 * insertion is fast but deletion requires searching the chain.

we do as much deletion as insertion....

 *
 * There is also the doubly-linked hash chain approach, under which
 * deletion requires no search but the code is longer and it takes two
 * pointers per item.
 *
 * The circularly-linked approach has the shortest code but requires
 * two pointers per bucket, doubling the size of the bucket array (in
 * addition to two pointers per item).
 */
#define TS_HASH_DEFINE(PREFIX,ITEM_TYPE,KEY_TYPE,KEY_NAME,LINK_NAME,HASH_FUNC,EQ_FUNC)        \
                                                                                              \
static __inline__ int                                                                         \
PREFIX##_hash_init (PREFIX##_hash_table *hash,                                                \
		    __u32                buckets)                                             \
{                                                                                             \
  hash->_table   = (ITEM_TYPE**) KMALLOC (sizeof (ITEM_TYPE*) * buckets);                     \
  hash->_buckets = buckets;                                                                   \
  if (hash->_table == NULL)                                                                   \
    {                                                                                         \
      return -ENOMEM;                                                                         \
    }                                                                                         \
  xmemset (hash->_table, 0, sizeof (ITEM_TYPE*) * buckets);                                   \
  return 0;                                                                                   \
}                                                                                             \
                                                                                              \
static __inline__ void                                                                        \
PREFIX##_hash_done (PREFIX##_hash_table *hash)                                                \
{                                                                                             \
  KFREE (hash->_table, sizeof (ITEM_TYPE*) * hash->_buckets);                                 \
  hash->_table = NULL;                                                                        \
}                                                                                             \
                                                                                              \
static __inline__ ITEM_TYPE*                                                                  \
PREFIX##_hash_find_index (PREFIX##_hash_table *hash,                                          \
			  __u32                hash_index,                                    \
			  KEY_TYPE const      *find_key)                                      \
{                                                                                             \
  ITEM_TYPE *item;                                                                            \
                                                                                              \
  for (item  = hash->_table[hash_index];                                                      \
       item != NULL;                                                                          \
       item  = item->LINK_NAME._next)                                                         \
    {                                                                                         \
      if (EQ_FUNC (& item->KEY_NAME, find_key))                                               \
        {                                                                                     \
          return item;                                                                        \
        }                                                                                     \
    }                                                                                         \
                                                                                              \
  return NULL;                                                                                \
}                                                                                             \
                                                                                              \
static __inline__ int                                                                         \
PREFIX##_hash_remove_index (PREFIX##_hash_table *hash,                                        \
			    __u32                hash_index,                                  \
			    ITEM_TYPE           *del_item)                                    \
{                                                                                             \
  ITEM_TYPE *hash_item = hash->_table[hash_index];                                            \
  ITEM_TYPE *last_item;                                                                       \
                                                                                              \
  if (del_item == hash_item)                                                                  \
    {                                                                                         \
      hash->_table[hash_index] = del_item->LINK_NAME._next;                                   \
      return 1;                                                                               \
    }                                                                                         \
                                                                                              \
  for (last_item  = hash_item, hash_item = hash_item->LINK_NAME._next;                        \
       hash_item != NULL;                                                                     \
       last_item  = hash_item, hash_item = hash_item->LINK_NAME._next)                        \
    {                                                                                         \
      if (hash_item == del_item)                                                              \
	{                                                                                     \
	  last_item->LINK_NAME._next = hash_item->LINK_NAME._next;                            \
	  return 1;                                                                           \
	}                                                                                     \
    }                                                                                         \
                                                                                              \
  return 0;                                                                                   \
}                                                                                             \
                                                                                              \
static __inline__ void                                                                        \
PREFIX##_hash_insert_index (PREFIX##_hash_table *hash,                                        \
			    __u32                hash_index,                                  \
			    ITEM_TYPE           *ins_item)                                    \
{                                                                                             \
  ins_item->LINK_NAME._next = hash->_table[hash_index];                                       \
  hash->_table[hash_index]  = ins_item;                                                       \
}                                                                                             \
                                                                                              \
static __inline__ ITEM_TYPE*                                                                  \
PREFIX##_hash_find (PREFIX##_hash_table *hash,                                                \
	            KEY_TYPE const      *find_key)                                            \
{                                                                                             \
  return PREFIX##_hash_find_index (hash, HASH_FUNC(find_key), find_key);                      \
}                                                                                             \
                                                                                              \
static __inline__ int                                                                         \
PREFIX##_hash_remove (PREFIX##_hash_table *hash,                                              \
		      ITEM_TYPE           *del_item)                                          \
{                                                                                             \
  return PREFIX##_hash_remove_index (hash, HASH_FUNC(&del_item->KEY_NAME), del_item);         \
}                                                                                             \
                                                                                              \
static __inline__ void                                                                        \
PREFIX##_hash_insert (PREFIX##_hash_table *hash,                                              \
		      ITEM_TYPE           *ins_item)                                          \
{                                                                                             \
  return PREFIX##_hash_insert_index (hash, HASH_FUNC(&ins_item->KEY_NAME), ins_item);         \
}


#define for_all_ht_buckets(table, head)					\
for ((head) = &(table) -> _table[ 0 ] ;					\
     (head) != &(table) -> _table[ (table) -> _buckets ] ; ++ (head))

#define for_all_in_bucket(bucket, item, next, field)				\
for ((item) = *(bucket), (next) = (item) ? (item) -> field._next : NULL ;	\
     (item) != NULL ;								\
     (item) = (next), (next) = (item) ? (item) -> field._next : NULL )

#endif /* __REISER4_TSHASH_H__ */

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
