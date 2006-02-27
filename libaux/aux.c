/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   aux.c -- miscellaneous code. */  

#include <aux/aux.h>

/* This implements binary search for "needle" among "count" elements.
    
   Return values: 
   1 - key on *pos found exact key on *pos position; 
   0 - exact key has not been found. key of *pos < then wanted. */

int aux_bin_search(
	void *array,		        /* pointer to array for search in */
	uint32_t count,		        /* array length */
	void *needle,		        /* array item to be found */
	aux_comp_func_t comp_func,      /* function for comparing items */
	void *data,			/* user-specified data */
	uint32_t *pos)		        /* pointer result will be saved in */
{
	int res = 0;
	int left, right, i;

	if (count == 0) {
		*pos = 0;
		return 0;
	}

	left = 0;
	right = count - 1;

	for (i = (right + left) / 2; left <= right;
	     i = (right + left) / 2)
	{
		res = comp_func(array, i, needle, data);
		
		if (res == -1) {
			left = i + 1;
			continue;
		} else if (res == 1) {
			right = i - 1;
			continue;
		} else {
			*pos = i;
			return 1;
		}	
	}

	*pos = left;
	return 0;
}

#ifndef ENABLE_MINIMAL
#define MAX_PATH 1024
#else
#define MAX_PATH 256
#endif

/* Parse standard unix path. It uses two callback functions for notifying user
   what stage of parse is going on. */
errno_t aux_parse_path(char *path, aux_pre_entry_t pre_func,
		       aux_post_entry_t post_func, void *data)
{
	char local[MAX_PATH];
	char *pointer = NULL;
	char *entry = NULL;
	errno_t res;

	aal_memset(local, 0, sizeof(local));
	
	/* Initializing local variable path is stored in. */
	aal_strncpy(local, path, sizeof(local));
	
	if (local[0] == '/') {
		if ((res = post_func(NULL, NULL, data)))
			return res;
		
		pointer = &local[1];
	} else {
		pointer = local;
	}

	/* Loop until local is finished parse. */
	while (1) {
		if ((res = pre_func(path, entry, data)))
			return res;

		/* Using strsep() for parsing path with delimiting char
		   "/". This probably may be improved with bias do not 
		   use hardcoded "/" and use some macro instead. */
		while (1) {
			if (!(entry = aal_strsep(&pointer, "/")))
				return 0;

			if (aal_strlen(entry)) 
				break;
			
			if (!pointer || !aal_strlen(pointer))
				return 0;
			else
				continue;
		}
	
		if ((res = post_func(path, entry, data)))
			return res;
	}

	return 0;
}

/* Packing string into uint64_t value. This is used by key40 plugin for creating
   entry keys. */
uint64_t aux_pack_string(char *buff, 
			 uint32_t start)
{
	unsigned i;
	uint64_t value = 0;

	for (i = 0; (i < sizeof(value) - start) &&
		     buff[i]; ++i)
	{
		value <<= 8;
		value |= (unsigned char)buff[i];
	}
	
	return (value <<= (sizeof(value) - i - start) << 3);
}

/* Extracts the part of string from the 64bits value it was packed to. This
   function is needed in direntry40 plugin for unpacking keys of short
   entries. */
char *aux_unpack_string(uint64_t value,
			char *buff)
{
	do {
		*buff = value >> (64 - 8);
		if (*buff)
			buff++;
		value <<= 8;
		
	} while (value != 0);

	*buff = '\0';
	return buff; 
}

/* Calculates the adler32 checksum for the data pointed by "buff" of the length
   "n". This function was originally taken from zlib, version 1.1.3, July 9th,
   1998.

   Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler

   This software is provided 'as-is', without any express or implied warranty.
   In no event will the authors be held liable for any damages arising from the
   use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
   
   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
   
   3. This notice may not be removed or altered from any source distribution.

   Jean-loup Gailly        Mark Adler
   jloup@gzip.org          madler@alumni.caltech.edu

   The above comment is applyed to the only aal_alder32 function. */
#define ADLER_NMAX (5552)
#define ADLER_BASE (65521l)

unsigned int aux_adler32(unsigned int adler, char *buff, unsigned int n) {
	unsigned char *t = (unsigned char *)buff;
	unsigned int s1 = 1, s2 = 0;
	int k;

	adler = (adler) ? : 1;
	
	s1 = adler & 0xffff;
	s2 = adler >> 16;
	
	while (n > 0) {
		k = n < ADLER_NMAX ? n : ADLER_NMAX;
		n -= k;
	
		while (k--) {
			s1 += *t++; 
			s2 += s1;
		}
	
		s1 %= ADLER_BASE;
		s2 %= ADLER_BASE;
	}
	
	return (s2 << 16) | s1;
}
