/*
  bitops.c -- bitops functions.
  Some parts of this code stolen somewhere from linux.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* 
   These functions are standard bitopts functions for both (big and little
   endian) systems. As endianess of system is determining durring configure of
   package, we are using WORDS_BIGENDIAN macro for complilation time determinig
   system endianess. This way is more preffered as system endianess doesn't
   changing in runtime :)
*/

#ifndef WORDS_BIGENDIAN

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

#else

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
#ifndef WORDS_BIGENDIAN    
	return aal_le_set_bit(nr, addr);
#else
	return aal_be_set_bit(nr, addr);
#endif 
}

inline int aal_clear_bit(unsigned long long nr, void *addr) {
#ifndef WORDS_BIGENDIAN    
	return aal_le_clear_bit(nr, addr);
#else 
	return aal_be_clear_bit(nr, addr);
#endif 
}

inline int aal_test_bit(unsigned long long nr, const void *addr) {
#ifndef WORDS_BIGENDIAN    
	return aal_le_test_bit(nr, addr);
#else 
	return aal_be_test_bit(nr, addr);
#endif 
}

inline unsigned long long aal_find_first_zero_bit(const void *vaddr, 
						  unsigned long long size) 
{
#ifndef WORDS_BIGENDIAN    
	return aal_le_find_first_zero_bit(vaddr, size);
#else 
	return aal_be_find_first_zero_bit(vaddr, size);
#endif 
}

inline unsigned long long aal_find_next_zero_bit(const void *vaddr, 
						 unsigned long long  size,
						 unsigned long long offset) 
{
#ifndef WORDS_BIGENDIAN    
	return aal_le_find_next_zero_bit(vaddr, size, offset);
#else 
	return aal_be_find_next_zero_bit(vaddr, size, offset);
#endif 
}

