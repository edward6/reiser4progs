/*
  hash.c -- small hash table using example.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <stdio.h>
#include <aal/aal.h>

static uint32_t hash_func(const void *key) {
	return (uint32_t)key;
}

static int comp_func(const void *key1,
		     const void *key2,
		     void *data)
{
	uint32_t k1 = (uint32_t)key1;
	uint32_t k2 = (uint32_t)key2;

	if (k1 < k2)
		return -1;

	if (k1 > k2)
		return 1;

	return 0;
}

int main(int argc, char *argv[]) {
	uint32_t i;
	aal_hash_table_t *table;

	if (!(table = aal_hash_table_alloc(hash_func, comp_func)))
		return -1;

	for (i = 1; i < 1000000; i++) {
		aal_hash_table_insert(table, (void *)i,
				      (void *)(i * 100));
	}

	for (i = 1; i < 1000000; i++)
		aal_hash_table_remove(table, (void *)i);
	
	aal_hash_table_free(table);
	return 0;
}

