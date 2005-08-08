/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bitmap.c -- bitmap functions. Bitmap is used by bitmap-based block allocator
   plugin. */

#ifndef ENABLE_MINIMAL
#include <aux/bitmap.h>

/* This macros is used for checking whether given block is inside of allowed
   range or not. It is used in all bitmap functions. */
#define aux_bitmap_bound_check(bitmap, bit, action)		        \
do {								        \
    if (bit >= bitmap->total) {					        \
	action;								\
    }									\
} while (0)

/* Checks whether passed block is inside of bitmap and marks it. This function
   also increases marked block counter. */
void aux_bitmap_mark(
	aux_bitmap_t *bitmap,	    /* bitmap, bit will be marked in */
	uint64_t bit)		    /* bit to be marked */
{
	aal_assert("umka-336", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, bit, return);
	
	if (aal_test_bit(bitmap->map, bit))
		return;
	
	aal_set_bit(bitmap->map, bit);
	bitmap->marked++;
}

/* Checks whether passed block is inside of bitmap and clears it. This function
  also descreases marked block counter. */
void aux_bitmap_clear(
	aux_bitmap_t *bitmap,	    /* bitmap, passed blk will be marked in */
	uint64_t bit)		    /* bit to be cleared */
{
	aal_assert("umka-337", bitmap != NULL);
	aux_bitmap_bound_check(bitmap, bit, return);
	
	if (!aal_test_bit(bitmap->map, bit))
		return;
	
	aal_clear_bit(bitmap->map, bit);
	bitmap->marked--;
}

/* Checks whether passed block is inside of bitmap and test it. Returns TRUE if
   block is marked, FALSE otherwise. */
int aux_bitmap_test(
	aux_bitmap_t *bitmap,	    /* bitmap, passed blk will be tested */
	uint64_t bit)		    /* bit to be tested */
{
	aal_assert("umka-338", bitmap != NULL);
	
	aux_bitmap_bound_check(bitmap, bit, return 0);
	return aal_test_bit(bitmap->map, bit);
}

/* Makes loop through bitmap and calculates the number of marked/cleared blocks
   in it. This function is used for checking the bitmap on validness. Also it is
   used for calculating marked blocks of bitmap in aux_bitmap_open function. See
   bellow for details. */
static uint64_t aux_bitmap_calc(
	aux_bitmap_t *bitmap,	   /* bitmap will be used for calculating bits */
	uint64_t start,		   /* start bit, calculating should be performed from */
	uint64_t count,		   /* end bit, calculating should be stoped on */
	int marked)		   /* flag for kind of calculating (marked or cleared) */
{
	uint64_t i, bits = 0;
	
	for (i = start; i < start + count; i++)
		bits += aux_bitmap_test(bitmap, i) ? marked : !marked;

	return bits;
}

/* Checks whether passed range of blocks is inside of bitmap and marks
   blocks. This function also increseas marked block counter. */
void aux_bitmap_mark_region(
	aux_bitmap_t *bitmap,	    /* bitmap for working with */
	uint64_t start,		    /* start bit of the region */
	uint64_t count)		    /* bit count to be marked */
{
	uint64_t num;
	
	aal_assert("vpf-472", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, start, return);
	aux_bitmap_bound_check(bitmap, start + count - 1, return);
	
	num = aux_bitmap_calc(bitmap, start, count, 0);
	aal_set_bits(bitmap->map, start, count);
	bitmap->marked += num;
}

/* Checks whether passed range of blocks is inside of bitmap and clears
   blocks. This function also descreases marked block counter. */
void aux_bitmap_clear_region(
	aux_bitmap_t *bitmap,	    /* bitmap range of blocks will be cleared in */
	uint64_t start,		    /* start bit of the range */
	uint64_t count)		    /* bit count to be clean */
{
	uint64_t num;
	
	aal_assert("vpf-473", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, start, return);
	aux_bitmap_bound_check(bitmap, start + count - 1, return);
	
	num = aux_bitmap_calc(bitmap, start, count, 1);
	aal_clear_bits(bitmap->map, start, count);
	bitmap->marked -= num;
}

/* Finds first cleared bit in bitmap, starting from passed "start" */
uint64_t aux_bitmap_find_cleared(
	aux_bitmap_t *bitmap,	    /* bitmap, clear bit will be searched in */
	uint64_t start)		    /* start bit, search should be performed from */
{
	uint64_t bit;
	
	aal_assert("umka-339", bitmap != NULL);
	aux_bitmap_bound_check(bitmap, start, return INVAL_BLK);

	bit = aal_find_next_zero_bit(bitmap->map,
				     bitmap->total, start);
	
	if (bit >= bitmap->total)
		return INVAL_BLK;

	return bit;
}

/* Finds first marked in bitmap block, starting from passed "start" */
uint64_t aux_bitmap_find_marked(
	aux_bitmap_t *bitmap,	    /* bitmap, marked bit to be searched in */
	uint64_t start)		    /* start bit, search should be started at */
{
	uint64_t bit;
	
	aal_assert("vpf-457", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, start, return INVAL_BLK);
	bit = aal_find_next_set_bit(bitmap->map, bitmap->total, start);

	if (bit >= bitmap->total)
		return INVAL_BLK;

	return bit;
}

/* Tests if all bits of the interval [start, count] are cleared in the
   bitmap. */
bool_t aux_bitmap_test_region(
	aux_bitmap_t *bitmap,	    /* bitmap for working with */
	uint64_t start,		    /* start bit of the range */
	uint64_t count,		    /* bit count to be clean */
	int marked)
{
	blk_t next;
	
	aal_assert("vpf-471", bitmap != NULL);
	aal_assert("vpf-728", count > 0);
	
	aux_bitmap_bound_check(bitmap, start, return 0);
	aux_bitmap_bound_check(bitmap, start + count - 1, return 0);

	if (marked)
		next = aux_bitmap_find_cleared(bitmap, start);
	else
		next = aux_bitmap_find_marked(bitmap, start);

	if (next >= start && next < start + count)
		return 0;

	return 1;
}

uint64_t aux_bitmap_find_region(
	aux_bitmap_t *bitmap,	    /* bitmap, clear bit will be searched in */
	uint64_t *start,	    /* start of clean region will be stored */
	uint64_t count,             /* blocks requested */
	int marked)                 /* find marked region or clean */
{
	aal_assert("umka-1773", bitmap != NULL);

	if (marked) {
		return aal_find_set_bits(bitmap->map,
					 bitmap->total,
					 start, count);
	} else {
		return aal_find_zero_bits(bitmap->map,
					  bitmap->total,
					  start, count);
	}
}

/* Public wrapper for previous function */
uint64_t aux_bitmap_calc_marked(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	aal_assert("umka-340", bitmap != NULL);
	bitmap->marked = aux_bitmap_calc(bitmap, 0, bitmap->total, 1);
	return bitmap->marked;
}

/* The same as previous one */
uint64_t aux_bitmap_calc_cleared(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	aal_assert("vpf-1320", bitmap != NULL);
	return aux_bitmap_calc(bitmap, 0, bitmap->total, 0);
}

/* Retuns stored value of marked blocks from specified bitmap */
uint64_t aux_bitmap_marked(
	aux_bitmap_t *bitmap)	/* bitmap marked block number will be obtained
				   from */
{
	aal_assert("umka-343", bitmap != NULL);
	return bitmap->marked;
}

/* Retuns stored value of clear blocks from specified bitmap */
uint64_t aux_bitmap_cleared(
	aux_bitmap_t *bitmap)	/* bitmap unsuded blocks will be obtained
				   from */
{
	aal_assert("umka-344", bitmap != NULL);
	return bitmap->total - bitmap->marked;
}

/* Creates instance of bitmap */
aux_bitmap_t *aux_bitmap_create(uint64_t len) {
	aux_bitmap_t *bitmap;
	    
	if (!(bitmap = aal_calloc(sizeof(*bitmap), 0)))
		return NULL;
	
	bitmap->marked = 0;
	bitmap->total = len;
	bitmap->size = (len + 7) / 8;
    
	if (!(bitmap->map = aal_calloc(bitmap->size, 0))) {
		aal_free(bitmap);
		return NULL;
	}
    
	return bitmap;
}

/* Makes clone of specified bitmap. Returns it to caller */
aux_bitmap_t *aux_bitmap_clone(
	aux_bitmap_t *bitmap)	    /* bitmap clone of which will be created */
{
	aux_bitmap_t *clone;

	aal_assert("umka-358", bitmap != NULL);

	if (!(clone = aux_bitmap_create(bitmap->total)))
		return NULL;
	
	clone->marked = bitmap->marked;
	aal_memcpy(clone->map, bitmap->map, clone->size);
    
	return clone;
}

/* Resizes the @bitmap to the given @len.  */
void aux_bitmap_resize(aux_bitmap_t *bitmap, uint64_t len) {
	void *map;
	uint32_t size;
	bool_t enlarge;
	uint64_t i, total;

	size = (len + 7) / 8;
	enlarge = size > bitmap->size ? 1 : 0;
	
	if (!(map = aal_calloc(size, 0)))
		return;

	aal_memcpy(map, bitmap->map, enlarge ? bitmap->size : size);

	if (enlarge) {
		/* Fix bits that were out of bounds. */
		total = bitmap->size * 8;
		for (i = bitmap->total; i < total; i++)
			aal_clear_bit(map, i);
	}

	aal_free(bitmap->map);
	bitmap->map = map;
	bitmap->total = len;
	bitmap->size = size;
	
	if (!enlarge) {
		aux_bitmap_calc_marked(bitmap);
	}
}

/* Frees all memory assigned to bitmap */
void aux_bitmap_close(
	aux_bitmap_t *bitmap)	    /* bitmap to be closed */
{
	aal_assert("umka-354", bitmap != NULL);
	aal_assert("umka-1082", bitmap->map != NULL);
	
	aal_free(bitmap->map);
	aal_free(bitmap);
}

/* Inverts the bitmap data. */
void aux_bitmap_invert(aux_bitmap_t *bitmap) {
	uint64_t i, total;
	
	aal_assert("vpf-1421", bitmap != NULL);

	for (i = 0; i < bitmap->size; i++)
		bitmap->map[i] = ~bitmap->map[i];

	/* Fix bits that are out of bounds. */
	total = bitmap->size * 8;
	for (i = bitmap->total; i < total; i++) 
		aal_clear_bit(bitmap->map, i);

	bitmap->marked = bitmap->total - bitmap->marked;
}

/* Packs the bitmap. */
errno_t aux_bitmap_pack(aux_bitmap_t *bitmap, aal_stream_t *stream) {
	uint64_t i, count;
	int set;
	
	aal_assert("vpf-1431", bitmap != NULL);
	aal_assert("vpf-1432", stream != NULL);
	
	aal_stream_write(stream, AUX_BITMAP_MAGIC, sizeof(AUX_BITMAP_MAGIC));
	aal_stream_write(stream, &bitmap->total, sizeof(bitmap->total));

	i = count = 0;
	set = 1;

	while (1) {
		if (set)
			i = aux_bitmap_find_cleared(bitmap, count);
		else
			i = aux_bitmap_find_marked(bitmap, count);
		
		if (i == INVAL_BLK)
			break;
		
		i -= count;
		
		/* Write the @count. */ 
		aal_stream_write(stream, &i, sizeof(i));
		
		count += i;
		set = !set;

	}
	
	i = bitmap->total - count;
	
	/* Write the last @count and @extents. */
	aal_stream_write(stream, &i, sizeof(i));
	
	return 0;
}

/* Creates bitmap by passed @stream. */
aux_bitmap_t *aux_bitmap_unpack(aal_stream_t *stream) {
	int32_t size, set;
	uint64_t total, count, bit;
	aux_bitmap_t *bitmap = NULL;
	char *buf[sizeof(AUX_BITMAP_MAGIC)];
	
	aal_assert("vpf-1434", stream != NULL);

	size = sizeof(AUX_BITMAP_MAGIC);
	
	/* Read and check the magic. */
	if (aal_stream_read(stream, buf, size) != size)
		goto error_eostream;
	
	if (aal_memcmp(buf, AUX_BITMAP_MAGIC, size)) {
		aal_error("Can't unpack the bitmap. "
			  "Wrong magic found.");
		return NULL;
	}
	
	/* Read the bitmap size. */
	if (aal_stream_read(stream, &total,
			    sizeof(total)) != sizeof(total))
	{
		goto error_eostream;
	}
	
	if (!(bitmap = aux_bitmap_create(total)))
		return NULL;
	
	for (bit = 0, set = 1; 1; set = !set, bit += count) {
		uint32_t read;
		
		read = aal_stream_read(stream, &count,
				       sizeof(count));
		
		if (read != sizeof(count))
			break;

		if (bit + count > total) {
			aal_error("Stream with the bitmap looks corrupted.");
			goto error_free_bitmap;
		}
		
		if (set)
			aux_bitmap_mark_region(bitmap, bit, count);
	}
	
	if (bit != total)
		goto error_eostream;
	
	return bitmap;
 error_eostream:
	aal_error("Can't unpack the bitmap. Stream is over?");

 error_free_bitmap:
	if (bitmap) {
		aux_bitmap_close(bitmap);
	}
	
	return NULL;
}
#endif
