/*
  bitops.c -- bitops functions.
  Some parts of this code stolen somewhere from linux.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

inline int aal_set_bit(unsigned long long nr, void *addr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p |= mask;

	return retval;
}

inline int aal_clear_bit(unsigned long long nr, void *addr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p &= ~mask;

	return retval;
}

inline int aal_test_bit(unsigned long long nr, const void *addr) {
	unsigned char *p, mask;
  
	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);
	return ((mask & *p) != 0);
}

inline unsigned long long aal_find_first_zero_bit(const void *vaddr, 
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
		if (!aal_test_bit(res, p)) break;
    
	return (p - addr) * 8 + res;
}

inline unsigned long long aal_find_next_zero_bit(const void *vaddr, 
						 unsigned long long size,
						 unsigned long long offset) 
{
	int bit = offset & 7, res;
	const unsigned char *addr = vaddr;
	const unsigned char *p = addr + (offset >> 3);
  
	if (offset >= size)
		return size;
  
	if (bit) {
		for (res = bit; res < 8; res++)
			if (!aal_test_bit (res, p))
				return (p - addr) * 8 + res;
		p++;
	}

	res = aal_find_first_zero_bit(p, size - 8 * (p - addr));
	return (p - addr) * 8 + res;
}

static inline int aal_find_next_zero_bit_in_byte(unsigned int byte,
						 int start)
{
        int i = start;
        unsigned int mask = 1 << start;

        while ((byte & mask) != 0) {
                mask <<= 1;
                if (++i >= 8)
                        break;
        }

        return i;
}

inline unsigned long long aal_find_next_set_bit(const void *vaddr, 
						unsigned long long size, 
						unsigned long long offset)
{
        const unsigned char *addr = vaddr;
        unsigned int byte_nr = offset >> 3;
        unsigned int bit_nr = offset & 0x7;
        unsigned int max_byte_nr = (size - 1) >> 3;

        if (bit_nr != 0) {
		unsigned int b = ~(unsigned int)addr[byte_nr];
                unsigned int nzb = aal_find_next_zero_bit_in_byte(b, bit_nr);

                if (nzb < 8)
                        return (byte_nr << 3) + nzb;

                ++byte_nr;
        }

        while (byte_nr <= max_byte_nr) {
                if (addr[byte_nr] != 0) {
			unsigned int b = ~(unsigned int)addr[byte_nr];
			unsigned int nzb = aal_find_next_zero_bit_in_byte(b, 0);
			
                        return (byte_nr << 3) + nzb;
                }

                ++byte_nr;
        }

        return size;
}

inline void aal_clear_bits(void *vaddr, 
			   unsigned long long start, 
			   unsigned long long end)
{
	char *addr = vaddr;
        int first_byte;
        int last_byte;

        unsigned char first_byte_mask = 0xff;
        unsigned char last_byte_mask = 0xff;

        first_byte = start >> 3;
        last_byte = (end - 1) >> 3;

        if (last_byte > first_byte + 1) {
                aal_memset(addr + first_byte + 1, 0,
			   last_byte - first_byte - 1);
	}

        first_byte_mask >>= 8 - (start & 0x7);
        last_byte_mask <<= ((end - 1) & 0x7) + 1;

        if (first_byte == last_byte) {
                addr[first_byte] &= (first_byte_mask | last_byte_mask);
        } else {
                addr[first_byte] &= first_byte_mask;
                addr[last_byte] &= last_byte_mask;
        }
}

inline void aal_set_bits(void *vaddr, 
			 unsigned long long start, 
			 unsigned long long end)
{
        int last_byte;
        int first_byte;
	char *addr = vaddr;

        char first_byte_mask = 0xff;
        char last_byte_mask = 0xff;

        first_byte = start >> 3;
        last_byte = (end - 1) >> 3;

        if (last_byte > first_byte + 1) {
                aal_memset(addr + first_byte + 1, 0xff,
			   last_byte - first_byte - 1);
	}

        first_byte_mask <<= start & 0x7;
        last_byte_mask >>= 7 - ((end - 1) & 0x7);

        if (first_byte == last_byte) {
                addr[first_byte] |= (first_byte_mask & last_byte_mask);
        } else {
                addr[first_byte] |= first_byte_mask;
                addr[last_byte] |= last_byte_mask;
        }
}

inline void aal_find_zero_bits(void *vaddr,
			       unsigned long long size,
			       unsigned long long *start,
			       unsigned long long *count)
{
}

inline void aal_find_set_bits(void *vaddr,
			      unsigned long long size,
			      unsigned long long *start,
			      unsigned long long *count)
{
}
