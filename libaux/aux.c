/*
  aux.c -- miscellaneous useful code. 
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/  

#include <aux/aux.h>

/* 
   This implements binary search for "needle" among "count" elements.
    
   Return values: 
   1 - key on *pos found exact key on *pos position; 
   0 - exact key has not been found. key of *pos < then wanted.
*/
int aux_bin_search(
	void *array,		        /* opaque pointer to array item will be searched in */
	uint32_t count,		        /* array length */
	void *needle,		        /* array item to be found */
	aux_comp_func_t comp_func,      /* callback function for comparing items from the array */
	void *data,			/* user-specified data */
	uint64_t *pos)		        /* pointer result will be saved in */
{
	void *elem;
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
			if (i == 0)
				break;
	    
			right = i - 1;
			continue;
		} else {
			*pos = i;
			return 1;
		}	
	}

	*pos = left - (left > 0 ? 1 : 0);
	return 0;
}

/* Parse standard unix path function */
errno_t aux_parse_path(char *path, aux_pre_entry_t pre_func,
		       aux_post_entry_t post_func, void *data)
{
	char track[256];
	char *entry = NULL;

	aal_memset(track, 0, sizeof(track));
	
	while (1) {
		errno_t res;
		uint32_t len;

		len = aal_strlen(track);
			
		if (track[len - 1] != '/')
			track[len] = '/';
		
		if ((res = pre_func(track, entry, data)))
			return res;
    
		if (!(entry = aal_strsep(&path, "/")))
			return 0;

		if (!aal_strlen(entry))
			continue;
	
		aal_strncat(track, entry, aal_strlen(entry));

		if ((res = post_func(track, entry, data)))
			return res;
	}

	return 0;
}

/*
  Packing string into uint64_t value. This is used by key40 plugin for creating
  entry keys.
*/
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

/*
  Extracts the part of string from the 64bits value it was packed to. This
  function is needed in direntry40 plugin for unpacking keys of short entries.
*/
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

