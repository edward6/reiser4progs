/*
  bitops.c -- bitops functions.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

inline int aal_set_bit(void *addr, bit_t nr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p |= mask;

	return retval;
}

inline int aal_clear_bit(void *addr, bit_t nr) {
	unsigned char *p, mask;
	int retval;

	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p &= ~mask;

	return retval;
}

inline int aal_test_bit(void *addr, bit_t nr) {
	unsigned char *p, mask;
  
	p = (unsigned char *)addr;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);
	return ((mask & *p) != 0);
}

inline bit_t aal_find_first_zero_bit(void *vaddr, 
				     bit_t size) 
{
	int res;
	unsigned char *p = vaddr;
	unsigned char *addr = vaddr;

	if (!size)
		return 0;

	size = (size >> 3) + ((size & 0x7) > 0);
	
	while (*p++ == 255) {
		if (--size == 0)
			return (p - addr) << 3;
	}
  
	--p;
	for (res = 0; res < 8; res++) {
		if (!aal_test_bit(p, res))
			break;
	}
    
	return (p - addr) * 8 + res;
}

inline bit_t aal_find_next_zero_bit(void *vaddr, 
				    bit_t size,
				    bit_t offset) 
{
	int bit = offset & 7, res;
	unsigned char *addr = vaddr;
	unsigned char *p = addr + (offset >> 3);
  
	if (offset >= size)
		return size;
  
	if (bit) {
		for (res = bit; res < 8; res++)
			if (!aal_test_bit (p, res))
				return (p - addr) * 8 + res;
		p++;
	}

	res = aal_find_first_zero_bit(p, size - 8 * (p - addr));
	return (p - addr) * 8 + res;
}

/* Finds next zero bit in byte */
static inline int aal_find_nzb(unsigned char byte, int start) {
        int i = start;
        unsigned char mask = 1 << start;

        while ((byte & mask) != 0) {
                mask <<= 1;
                if (++i >= 8)
                        break;
        }

        return i;
}

inline bit_t aal_find_next_set_bit(void *vaddr, 
				   bit_t size, 
				   bit_t offset)
{
        unsigned char *addr = vaddr;
        unsigned int byte_nr = offset >> 3;
        unsigned int bit_nr = offset & 0x7;
        unsigned int max_byte_nr = (size - 1) >> 3;

        if (bit_nr != 0) {
		unsigned int b = ~(unsigned int)addr[byte_nr];
                unsigned int nzb = aal_find_nzb(b, bit_nr);

                if (nzb < 8)
                        return (byte_nr << 3) + nzb;

                ++byte_nr;
        }

        while (byte_nr <= max_byte_nr) {
                if (addr[byte_nr] != 0) {
			unsigned int b = ~(unsigned int)addr[byte_nr];
			unsigned int nzb = aal_find_nzb(b, 0);
			
                        return (byte_nr << 3) + nzb;
                }

                ++byte_nr;
        }

        return size;
}

inline void aal_clear_bits(void *vaddr, 
			   bit_t start, 
			   bit_t end)
{
	int end_byte;
	int start_byte;
	char *addr = vaddr;

        start_byte = start >> 3;
        end_byte = (end - 1) >> 3;

        if (end_byte > start_byte + 1) {
                aal_memset(addr + start_byte + 1, 0,
			   end_byte - start_byte - 1);
	}

        if (start_byte == end_byte) {
		bit_t bits;
		
		bits = start - (start_byte * 8);
		
		addr[start_byte] &= 0xff >>
			(0x8 - (bits + (end - start)));
        } else {
		bit_t bits;
		
		bits = start - (start_byte * 8);
		addr[start_byte] &= (0xff >> bits << bits);

		bits = end - (end_byte * 8);
		addr[end_byte] &= (0xff >> bits);
        }
}

inline void aal_set_bits(void *vaddr, 
			 bit_t start, 
			 bit_t end)
{
	int end_byte;
	int start_byte;
	char *addr = vaddr;

	start_byte = start >> 3;
	end_byte = (end - 1) >> 3;

	if (end_byte > start_byte + 1) {
		aal_memset(addr + start_byte + 1, 0xff,
			   end_byte - start_byte - 1);
	}

        if (start_byte == end_byte) {
		bit_t bits;
		
		bits = start - (start_byte * 8);
		
		addr[start_byte] |= 0xff >>
			(0x8 - (bits + (end - start)));
        } else {
		bit_t bits;
		
		bits = start - (start_byte * 8);
		addr[start_byte] |= (0xff >> bits << bits);

		bits = end - (end_byte * 8);
		addr[end_byte] |= (0xff >> bits);
        }
}

inline bit_t aal_find_zero_bits(void *vaddr, bit_t size,
				bit_t *start, bit_t count)
{
	bit_t prev;
	bit_t next;

	prev = aal_find_next_zero_bit(vaddr, size, 0);
	next = aal_find_next_zero_bit(vaddr, size, prev + 1);

	count--;
	*start = prev;

	while (next - prev == 1 && count--) {
		prev = next;
		next = aal_find_next_zero_bit(vaddr, size, next + 1);
	}

	return next - *start;
}

inline bit_t aal_find_set_bits(void *vaddr, bit_t size,
			       bit_t *start, bit_t count)
{
	bit_t prev;
	bit_t next;

	prev = aal_find_next_set_bit(vaddr, size, 0);
	next = aal_find_next_set_bit(vaddr, size, prev + 1);

	count--;
	*start = prev;

	while (next - prev == 1 && count--) {
		prev = next;
		next = aal_find_next_set_bit(vaddr, size, next + 1);
	}

	return next - *start;
}
