/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bitmap.c -- bitmap functions. Bitmap is used by bitmap-based block allocator
   plugin. */

#ifndef ENABLE_STAND_ALONE
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

/* Checks whether passed range of blocks is inside of bitmap and marks
   blocks. This function also increseas marked block counter. */
void aux_bitmap_mark_region(
	aux_bitmap_t *bitmap,	    /* bitmap for working with */
	uint64_t start,		    /* start bit of the region */
	uint64_t count)		    /* bit count to be marked */
{
	aal_assert("vpf-472", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, start, return);
	aux_bitmap_bound_check(bitmap, start + count - 1, return);
	
	aal_set_bits(bitmap->map, start, count);
	bitmap->marked += count;
}

/* Checks whether passed range of blocks is inside of bitmap and clears
   blocks. This function also descreases marked block counter. */
void aux_bitmap_clear_region(
	aux_bitmap_t *bitmap,	    /* bitmap range of blocks will be cleared in */
	uint64_t start,		    /* start bit of the range */
	uint64_t count)		    /* bit count to be clean */
{
	aal_assert("vpf-473", bitmap != NULL);

	aux_bitmap_bound_check(bitmap, start, return);
	aux_bitmap_bound_check(bitmap, start + count - 1, return);
	
	aal_clear_bits(bitmap->map, start, count);
	bitmap->marked -= count;
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
	
	aux_bitmap_bound_check(bitmap, start, return FALSE);
	aux_bitmap_bound_check(bitmap, start + count - 1, return FALSE);

	if (marked)
		next = aux_bitmap_find_cleared(bitmap, start);
	else
		next = aux_bitmap_find_marked(bitmap, start);

	if (next >= start && next < start + count)
		return FALSE;

	return TRUE;
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
	
	aal_assert("umka-340", bitmap != NULL);
	
	aux_bitmap_bound_check(bitmap, start, return INVAL_BLK);
	aux_bitmap_bound_check(bitmap, start + count - 1, return INVAL_BLK);
	
	for (i = start; i < start + count; i++)
		bits += aux_bitmap_test(bitmap, i) ? marked : !marked;

	return bits;
}

/* Public wrapper for previous function */
uint64_t aux_bitmap_calc_marked(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	bitmap->marked = aux_bitmap_calc(bitmap, 0, 
					 bitmap->total, 1);

	return bitmap->marked;
}

/* The same as previous one */
uint64_t aux_bitmap_calc_cleared(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	return aux_bitmap_calc(bitmap, 0,
			       bitmap->total, 0);
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
    
	if (!(bitmap->map = aal_calloc(bitmap->size, 0)))
		goto error_free_bitmap;
    
	return bitmap;
    
 error_free_bitmap:
	aal_free(bitmap);
	return NULL;
}

errno_t aux_bitmap_resize(aux_bitmap_t *bitmap,
			  uint64_t len)
{
	aal_assert("umka-1962", bitmap != NULL);
	
	bitmap->total = len;
	bitmap->size = (len + 7) / 8;

	return aal_realloc((void *)&bitmap->map,
			   bitmap->size);
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

/* Frees all memory assigned to bitmap */
void aux_bitmap_close(
	aux_bitmap_t *bitmap)	    /* bitmap to be closed */
{
	aal_assert("umka-354", bitmap != NULL);
	aal_assert("umka-1082", bitmap->map != NULL);
	
	aal_free(bitmap->map);
	aal_free(bitmap);
}

/* Return bitmap's map (memory chunk, bits array lies in) for direct access */
char *aux_bitmap_map(
	aux_bitmap_t *bitmap)	    /* bitmap, the bit array will be obtained from */
{
	aal_assert("umka-356", bitmap != NULL);
	return bitmap->map;
}

#endif
