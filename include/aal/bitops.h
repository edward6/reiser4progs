/*
  bitops.h -- bitops functions
  Some parts of this code was stolen somewhere from linux.
*/

#ifndef BITOPS_H
#define BITOPS_H

#define _ROUND_UP(x, n) (((x)+(n) - 1u) & ~((n) - 1u))
#define ROUND_UP(x) _ROUND_UP(x, 8ll)

extern inline int aal_set_bit(unsigned long long nr, void *addr);
extern inline int aal_clear_bit(unsigned long long nr, void *addr);
extern inline int aal_test_bit(unsigned long long nr, const void *addr);

extern inline void aal_clear_bits(void *vaddr, 
				  unsigned long long  start,
				  unsigned long long end);

extern inline void aal_set_bits(void *vaddr, 
				unsigned long long start,
				unsigned long long end);

extern inline unsigned long long aal_find_first_zero_bit(const void *vaddr,
							 unsigned long long size);

extern inline unsigned long long aal_find_next_zero_bit(const void *vaddr,
							unsigned long long size,
							unsigned long long offset);

extern inline unsigned long long aal_find_next_set_bit(const void *vaddr,
						       unsigned long long size,
						       unsigned long long offset);
#endif
