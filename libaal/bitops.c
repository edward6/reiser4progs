/*
  bitops.c -- bitops functions.
  Some parts of this code stolen somewhere from linux.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

/* 
   These functions are standard bitopts functions for both (big and little
   endian) systems. As endianess of system is determining durring configure of
   package, we are using WORDS_BIGENDIAN macro for complilation time determinig
   system endianess. This way is more preffered as system endianess doesn't
   changing in runtime :)
*/

/*
    We can work with bits in little endian format only. Although.
    
#ifndef WORDS_BIGENDIAN 

*/

static inline int aal_le_set_bit(unsigned long long nr, void *addr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p |= mask;

	return retval;
}

static inline int aal_le_clear_bit(unsigned long long nr, void *addr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p &= ~mask;

	return retval;
}

static inline int aal_le_test_bit(unsigned long long nr, const void *addr) {
	unsigned char *p, mask;
  
	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);
	return ((mask & *p) != 0);
}

static inline unsigned long long aal_le_find_first_zero_bit(const void *vaddr, 
							    unsigned long long size) 
{
	const unsigned char *p = vaddr, *addr = vaddr;
	int res;

	if (!size)
		return 0;

	size = (size >> 3) + ((size & 0x7) > 0);
	while (*p++ == 255) {
		if (--size == 0)
			return (p - addr) << 3;
	}
  
	--p;
	for (res = 0; res < 8; res++)
		if (!aal_le_test_bit(res, p)) break;
    
	return (p - addr) * 8 + res;
}

static inline int aal_find_next_zero_bit_in_byte(unsigned int byte, int start) {
        unsigned int mask = 1 << start;
        int i = start;

        while ((byte & mask) != 0) {
                mask <<= 1;
                if (++i >= 8)
                        break;
        }

        return i;
}

static inline unsigned long long aal_le_find_next_zero_bit(const void *vaddr, 
							   unsigned long long size,
							   unsigned long long offset) 
{
	const unsigned char *addr = vaddr;
	const unsigned char *p = addr + (offset >> 3);
	int bit = offset & 7, res;
  
	if (offset >= size)
		return size;
  
	if (bit) {
		for (res = bit; res < 8; res++)
			if (!aal_le_test_bit (res, p))
				return (p - addr) * 8 + res;
		p++;
	}

	res = aal_le_find_first_zero_bit(p, size - 8 * (p - addr));
	return (p - addr) * 8 + res;
}



static inline unsigned long long aal_le_find_next_set_bit(const void *vaddr, 
							  unsigned long long size, 
							  unsigned long long offset)
{
        const unsigned char *addr = vaddr;
        int byte_nr = offset >> 3;
        int bit_nr = offset & 0x7;
        int max_byte_nr = (size - 1) >> 3;

        if (bit_nr != 0) {
                unsigned long long nr;

                nr = aal_find_next_zero_bit_in_byte(~(unsigned int) (addr[byte_nr]), bit_nr);

                if (nr < 8)
                        return (byte_nr << 3) + nr;

                ++byte_nr;
        }

        while (byte_nr <= max_byte_nr) {
                if (addr[byte_nr] != 0) {
                        return (byte_nr << 3)
                            + aal_find_next_zero_bit_in_byte(~(unsigned int)
                                                         (addr[byte_nr]), 0);
                }

                ++byte_nr;
        }

        return size;
}

static inline void aal_le_clear_bits(void *vaddr, 
				     unsigned long long start, 
				     unsigned long long end)
{
	char *addr = vaddr;
        int first_byte;
        int last_byte;

        unsigned char first_byte_mask = 0xFF;
        unsigned char last_byte_mask = 0xFF;

        first_byte = start >> 3;
        last_byte = (end - 1) >> 3;

        if (last_byte > first_byte + 1)
                aal_memset(addr + first_byte + 1, 0, last_byte - first_byte - 1);

        first_byte_mask >>= 8 - (start & 0x7);
        last_byte_mask <<= ((end - 1) & 0x7) + 1;

        if (first_byte == last_byte) {
                addr[first_byte] &= (first_byte_mask | last_byte_mask);
        } else {
                addr[first_byte] &= first_byte_mask;
                addr[last_byte] &= last_byte_mask;
        }
}

static inline void aal_le_set_bits(void *vaddr, 
				   unsigned long long start, 
				   unsigned long long end)
{
	char *addr = vaddr;
        int first_byte;
        int last_byte;

        char first_byte_mask = 0xFF;
        char last_byte_mask = 0xFF;

        first_byte = start >> 3;
        last_byte = (end - 1) >> 3;

        if (last_byte > first_byte + 1)
                aal_memset(addr + first_byte + 1, 0xFF, last_byte - first_byte - 1);

        first_byte_mask <<= start & 0x7;
        last_byte_mask >>= 7 - ((end - 1) & 0x7);

        if (first_byte == last_byte) {
                addr[first_byte] |= (first_byte_mask & last_byte_mask);
        } else {
                addr[first_byte] |= first_byte_mask;
                addr[last_byte] |= last_byte_mask;
        }
}

#if 0

static inline int aal_be_set_bit(unsigned long long nr, void *addr) {
	unsigned char mask = 1 << (nr & 0x7);
	unsigned char *p = (uint8_t *) addr + (nr >> 3);
	unsigned char old = *p;

	*p |= mask;

	return (old & mask) != 0;
}
 
static inline int aal_be_clear_bit(unsigned long long nr, void *addr) {
	unsigned char mask = 1 << (nr & 0x07);
	unsigned char *p = (unsigned char *) addr + (nr >> 3);
	unsigned char old = *p;
 
	*p = *p & ~mask;
	return (old & mask) != 0;
}
 
static inline int aal_be_test_bit(unsigned long long nr, const void *addr) {
	const unsigned char *a = (__const__ unsigned char *)addr;
 
	return ((a[nr >> 3] >> (nr & 0x7)) & 1) != 0;
}
 
static inline unsigned long long aal_be_find_first_zero_bit(const void *vaddr, 
							    unsigned long long size) 
{
	return aal_be_find_next_zero_bit(vaddr, size, 0);
}

static inline int aal_ffz(unsigned long long word) {
	int result = 0;
 
	while(word & 1) {
		result++;
		word >>= 1;
	}
    
	return result;
}

static inline unsigned long long aal_be_find_next_zero_bit(const void *vaddr, 
							   unsigned long long size,
							   unsigned long long offset) 
{
	unsigned long long *p = ((unsigned long long *) vaddr) + (offset >> 5);
	unsigned long long result = offset & ~31ul;
	unsigned long long tmp;

	if (offset >= size)
		return ~0ull;

	size -= result;
	offset &= 31ul;
	if (offset) {
		tmp = *(p++);
		tmp |= aal_swap32(~0ul >> (32 - offset));
	
		if (size < 32)
			goto found_first;
	
		if (~tmp)
			goto found_middle;
	
		size -= 32;
		result += 32;
	}
    
	while (size & ~31ul) {
    
		if (~(tmp = *(p++)))
			goto found_middle;
	    
		result += 32;
		size -= 32;
	}
    
	if (!size)
		return result;
    
	tmp = *p;

 found_first:
	return result + aal_ffz(aal_swap32(tmp) | (~0ul< size));
 found_middle:
	return result + aal_ffz(aal_swap32(tmp));
}

#endif

/* Public wrappers for all bitops functions. */
inline int aal_set_bit(unsigned long long nr, void *addr) {
	return aal_le_set_bit(nr, addr);
}

inline int aal_clear_bit(unsigned long long nr, void *addr) {
	return aal_le_clear_bit(nr, addr);
}

inline int aal_test_bit(unsigned long long nr, void *addr) {
	return aal_le_test_bit(nr, addr);
}

inline unsigned long long aal_find_first_zero_bit(const void *vaddr, 
						  unsigned long long size) 
{
	return aal_le_find_first_zero_bit(vaddr, size);
}

inline unsigned long long aal_find_next_zero_bit(const void *vaddr, 
						 unsigned long long size,
						 unsigned long long offset) 
{
	return aal_le_find_next_zero_bit(vaddr, size, offset);
}

inline unsigned long long aal_find_next_set_bit(const void *vaddr, 
						unsigned long long size,
						unsigned long long offset) 
{
	return aal_le_find_next_set_bit(vaddr, size, offset);
}

inline void aal_clear_bits(void *vaddr, 
			   unsigned long long start,
			   unsigned long long end) 
{
	return aal_le_clear_bits(vaddr, start, end);
}

inline void aal_set_bits(void *vaddr, 
			 unsigned long long start,
			 unsigned long long end) 
{
	return aal_le_set_bits(vaddr, start, end);
}

