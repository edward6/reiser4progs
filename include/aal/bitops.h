/*
    bitops.h -- bitops functions
    Some parts of this code was stolen somewhere from linux.
*/

#ifndef BITOPS_H
#define BITOPS_H

#define _ROUND_UP(x, n) (((x)+(n) - 1u) & ~((n) - 1u))
#define ROUND_UP(x) _ROUND_UP(x, 8ll)

extern inline int aal_set_bit (unsigned long nr, void *addr);
extern inline int aal_clear_bit (unsigned long nr, void *addr);
extern inline int aal_test_bit(unsigned long nr, const void *addr);

extern inline int aal_find_first_zero_bit (const void *vaddr, 
    unsigned long size);

extern inline int aal_find_next_zero_bit (const void *vaddr, 
    unsigned long size, unsigned long offset);

#endif

