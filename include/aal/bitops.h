/*
  bitops.h -- bitops functions.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_BITOPS_H
#define AAL_BITOPS_H

typedef unsigned long long bit_t;

extern inline int aal_set_bit(void *map, bit_t nr);
extern inline int aal_clear_bit(void *map, bit_t nr);
extern inline int aal_test_bit(void *map, bit_t nr);

extern inline bit_t aal_find_first_zero_bit(void *map, bit_t size);

extern inline bit_t aal_find_next_zero_bit(void *map, bit_t size,
					   bit_t offset);

extern inline bit_t aal_find_next_set_bit(void *map, bit_t size,
					  bit_t offset);

extern inline bit_t aal_find_zero_bits(void *map, bit_t size,
				       bit_t *start, bit_t count);

extern inline bit_t aal_find_set_bits(void *map, bit_t size,
				      bit_t *start, bit_t count);

extern inline void aal_clear_bits(void *map, bit_t start,
				  bit_t count);

extern inline void aal_set_bits(void *map, bit_t start,
				bit_t count);

#endif
