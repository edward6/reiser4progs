/*
  bitops.c -- bitops functions. This file contains not only functions for
  getting/setting one bit at some position, but also functions for working with
  bit range.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aal/aal.h>

/* Turns on @nr bit in @map bitmap */
inline int aal_set_bit(void *map, bit_t nr) {
	int retval;
	unsigned char *p, mask;

	p = (unsigned char *)map;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p |= mask;

	return retval;
}

/* Turns off @nr bit in @map bitmap */
inline int aal_clear_bit(void *map, bit_t nr) {
	int retval;
	unsigned char *p, mask;

	p = (unsigned char *)map;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);

	retval = (mask & *p) != 0;
	*p &= ~mask;

	return retval;
}

/* Makes test of the @nr bit in @map bitmap */
inline int aal_test_bit(void *map, bit_t nr) {
	unsigned char *p, mask;
  
	p = (unsigned char *)map;
	p += nr >> 3;
	mask = 1 << (nr & 0x7);
	return ((mask & *p) != 0);
}

/* Finds first zero bit inside @map */
inline bit_t aal_find_first_zero_bit(void *map, 
				     bit_t size) 
{
	int res;
	unsigned char *p = map;
	unsigned char *addr = map;

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

/* Finds zero bit inside @map starting from @offset */
inline bit_t aal_find_next_zero_bit(void *map, 
				    bit_t size,
				    bit_t offset) 
{
	int bit = offset & 7, res;
	unsigned char *addr = map;
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

/* Finds zero bit in @byte starting from @offset */
static inline int aal_find_nzb(unsigned char byte, bit_t offset) {
        int i = offset;
        unsigned char mask = 1 << offset;

        while ((byte & mask) != 0) {
                mask <<= 1;
		
                if (++i >= 8)
                        break;
        }

        return i;
}

/* Finds set bit inside @map starting from @offset */
inline bit_t aal_find_next_set_bit(void *map, 
				   bit_t size, 
				   bit_t offset)
{
        unsigned char *addr = map;
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

/* Makes cleanup of bits inside range @start and @count */
inline void aal_clear_bits(void *map, 
			   bit_t start, 
			   bit_t count)
{
	int end_byte;
	int start_byte;
	char *addr = map;
	bit_t left, right;
	
        start_byte = start >> 3;
        end_byte = (start + count - 1) >> 3;

        if (end_byte > start_byte + 1) {
                aal_memset(addr + start_byte + 1, 0,
			   end_byte - start_byte - 1);
	}

	/* Work with start byte. */
	left = start - (start_byte * 8);
	right = start_byte == end_byte ? left + count : 8;
	
	addr[start_byte] &= ~((0xff << left) &
		(0xff >> (0x8 - right)));
	
	/* Work with end byte. */
        if (start_byte != end_byte) {
		right = start + count - (end_byte * 8);
		    
		addr[end_byte] &= ~(0xff >> (0x8 - right));
        }
}

/* Sets up the bits inside range @start and @count */
inline void aal_set_bits(void *map, 
			 bit_t start, 
			 bit_t count)
{
	int end_byte;
	int start_byte;
	char *addr = map;
	bit_t left, right;

	start_byte = start >> 3;
	end_byte = (start + count - 1) >> 3;

	if (end_byte > start_byte + 1) {
		aal_memset(addr + start_byte + 1, 0xff,
			   end_byte - start_byte - 1);
	}

	/* Work with start byte. */
	left = start - (start_byte * 8);
	right = start_byte == end_byte ? left + count : 8;
	
	addr[start_byte] |= ((0xff << left) &
		(0xff >> (0x8 - right)));
	
	/* Work with end byte. */
        if (start_byte != end_byte) {
		right = start + count - (end_byte * 8);
		    
		addr[end_byte] |= (0xff >> (0x8 - right));
        } 
}

/* Finds @count clear bits inside @map */
inline bit_t aal_find_zero_bits(void *map,
				bit_t size,
				bit_t *start,
				bit_t count)
{
	bit_t prev;
	bit_t next;

	prev = aal_find_next_zero_bit(map, size, *start);
	next = aal_find_next_zero_bit(map, size, prev + 1);

	count--;
	*start = prev;

	while (next - prev == 1 && count--) {
		bit_t curr;
		prev = next;
		
		curr = aal_find_next_zero_bit(map, size, next + 1);

		if (curr - prev != 1) {
			next++;
			break;
		}

		next = curr;
	}

	return next - *start;
}

/* Finds @count set bits inside @map */
inline bit_t aal_find_set_bits(void *map,
			       bit_t size,
			       bit_t *start,
			       bit_t count)
{
	bit_t prev;
	bit_t next;

	prev = aal_find_next_set_bit(map, size, *start);
	next = aal_find_next_set_bit(map, size, prev + 1);

	count--;
	*start = prev;

	while (next - prev == 1 && count--) {
		bit_t curr;
		prev = next;
		
		curr = aal_find_next_set_bit(map, size, next + 1);

		if (curr - prev != 1) {
			next++;
			break;
		}

		next = curr;
	}

	return next - *start;
}
