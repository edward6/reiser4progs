/*
  bitmap.c -- bitmap functions. Bitmap is used by bitmap-based block allocator
  plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/bitmap.h>
#include <aal/aal.h>

/* 
   This macros is used for checking whether given block is inside of allowed
   range or not. It is used in all bitmap functions.
*/
#define aux_bitmap_range_check(bitmap, bit, action)		        \
do {								        \
    if (bit >= bitmap->total) {					        \
	aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,	        \
	    "Block %llu is out of range (0-%llu)", bit, bitmap->total); \
	action;								\
    }									\
} while (0)

/* 
   Checks whether passed block is inside of bitmap and marks it. This
   function also increses marked block counter.
*/
void aux_bitmap_mark(
	aux_bitmap_t *bitmap,	    /* bitmap instance passed bit will be marked in */
	uint64_t bit)		    /* bit to be marked */
{
	aal_assert("umka-336", bitmap != NULL, return);

	aux_bitmap_range_check(bitmap, bit, return);
	if (aal_test_bit(bit, bitmap->map))
		return;
	
	aal_set_bit(bit, bitmap->map);
	bitmap->marked++;
}

/* 
   Checks whether passed block is inside of bitmap and clears it. This
   function also descreases marked block counter.
*/
void aux_bitmap_clear(
	aux_bitmap_t *bitmap,	    /* bitmap, passed blk will be marked in */
	uint64_t bit)		    /* bit to be cleared */
{
	aal_assert("umka-337", bitmap != NULL, return);

	aux_bitmap_range_check(bitmap, bit, return);
	if (!aal_test_bit(bit, bitmap->map))
		return;
	
	aal_clear_bit(bit, bitmap->map);
	bitmap->marked--;
}

/* 
   Checks whether passed block is inside of bitmap and test it. Returns TRUE if
   block is marked, FALSE otherwise.
*/
int aux_bitmap_test(
	aux_bitmap_t *bitmap,	    /* bitmap, passed blk will be tested */
	uint64_t bit)		    /* bit to be tested */
{
	aal_assert("umka-338", bitmap != NULL, return 0);
	aux_bitmap_range_check(bitmap, bit, return 0);
	return aal_test_bit(bit, bitmap->map);
}

/* 
   Checks whether passed range of blocks is inside of bitmap and marks blocks. 
   This function also increses marked block counter.
*/
void aux_bitmap_mark_range(
	aux_bitmap_t *bitmap,	    /* bitmap range of bits to be marked in */
	uint64_t start,		    /* start bit of the range */
	uint64_t end)		    /* end bit of the range, excluding */
{
	aal_assert("vpf-472", bitmap != NULL, return);
	aal_assert("vpf-458", start < end, return);

	aux_bitmap_range_check(bitmap, start, return);
	aux_bitmap_range_check(bitmap, end, return);
	
	aal_set_bits(bitmap->map, start, end);
	bitmap->marked += (end - start);
}

/* 
   Checks whether passed range of blocks is inside of bitmap and clears blocks. 
   This function also descreases marked block counter.
*/
void aux_bitmap_clear_range(
	aux_bitmap_t *bitmap,	    /* bitmap range of blocks will be cleared in */
	uint64_t start,		    /* start bit of the range */
	uint64_t end)		    /* end bit of the range, excluding */
{
	aal_assert("vpf-473", bitmap != NULL, return);
	aal_assert("vpf-459", start < end, return);

	aux_bitmap_range_check(bitmap, start, return);
	aux_bitmap_range_check(bitmap, end, return);
	
	aal_clear_bits(bitmap->map, start, end);
	
	bitmap->marked -= (end - start);
}

/* Tests if all bits of the interval [start,end) are cleared in the bitmap. */
int aux_bitmap_test_range_cleared(
	aux_bitmap_t *bitmap,	    /* bitmap, range of blocks to be tested in */
	uint64_t start,		    /* start bit of the range */
	uint64_t end)		    /* end bit of the range, excluding */
{
	blk_t next;
	aal_assert("vpf-471", bitmap != NULL, return 0);
	aal_assert("vpf-470", start < end, return 0);
	
	aux_bitmap_range_check(bitmap, start, return 0);
	aux_bitmap_range_check(bitmap, end, return 0);
	
	next = aux_bitmap_find_marked(bitmap, start);

	if (next >= start && next < end)
		return 0;

	return 1;
}

/* Tests if all bits of the interval [start,end) are marked in the bitmap. */
int aux_bitmap_test_range_marked(
	aux_bitmap_t *bitmap,	    /* bitmap, range of blocks to be tested in */
	uint64_t start,		    /* start bit of the range */
	uint64_t end)		    /* end bit of the range, excluding */
{
	blk_t next;
	aal_assert("vpf-474", bitmap != NULL, return 0);
	aal_assert("vpf-475", start < end, return 0);
	
	aux_bitmap_range_check(bitmap, start, return 0);
	aux_bitmap_range_check(bitmap, end, return 0);
	
	next = aux_bitmap_find_cleared(bitmap, start);

	if (next >= start && next < end)
		return 0;

	return 1;
}
/* Finds first cleared bit in bitmap, starting from passed "start" */
uint64_t aux_bitmap_find_cleared(
	aux_bitmap_t *bitmap,	    /* bitmap, clear bit will be searched in */
	uint64_t start)		    /* start bit, search should be performed from */
{
	uint64_t bit;
	
	aal_assert("umka-339", bitmap != NULL, return FAKE_BLK);
	
	aux_bitmap_range_check(bitmap, start, return FAKE_BLK);

	if ((bit = aal_find_next_zero_bit(bitmap->map, 
					  bitmap->total, start)) >= bitmap->total)
		return FAKE_BLK;

	return bit;
}

/* Finds first marked in bitmap block, starting from passed "start" */
uint64_t aux_bitmap_find_marked(
	aux_bitmap_t *bitmap,	    /* bitmap, marked bit to be searched in */
	uint64_t start)		    /* start bit, search should be started at */
{
	uint64_t bit;
	
	aal_assert("vpf-457", bitmap != NULL, return FAKE_BLK);
	
	aux_bitmap_range_check(bitmap, start, return FAKE_BLK);

	if ((bit = aal_find_next_set_bit(bitmap->map, 
					 bitmap->total, start)) >= bitmap->total)
		return FAKE_BLK;

	return bit;
}

/*
  Makes loop through bitmap and calculates the number of marked/cleared blocks in
  it. If it is possible it tries to find contiguous bitmap areas (64 bit) and in
  this maner increases performance. This function is used for checking the
  bitmap on validness. Imagine, we have a number of free blocks in the super
  block or somewhere else. And we can easily check whether this number equal to
  actual returned one or not. Also it is used for calculating marked blocks of
  bitmap in aux_bitmap_open function. See bellow for details.
*/
static uint64_t aux_bitmap_calc(
	aux_bitmap_t *bitmap,	   /* bitmap will be used for calculating bits */
	uint64_t start,		   /* start bit, calculating should be performed from */
	uint64_t end,		   /* end bit, calculating should be stoped on */
	int flag)		   /* flag for kind of calculating (marked or cleared) */
{
	uint64_t i, bits = 0;
	
	aal_assert("umka-340", bitmap != NULL, return FAKE_BLK);
	
	aux_bitmap_range_check(bitmap, start, return FAKE_BLK);
	aux_bitmap_range_check(bitmap, end - 1, return FAKE_BLK);
	
	for (i = start; i < end; i++)
		bits += aux_bitmap_test(bitmap, i) ? flag : !flag;

	return bits;
}

/* Public wrapper for previous function */
uint64_t aux_bitmap_calc_marked(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	return (bitmap->marked = aux_bitmap_calc(bitmap, 0, 
					       bitmap->total, 1));
}

/* The same as previous one */
uint64_t aux_bitmap_calc_cleared(
	aux_bitmap_t *bitmap)	 /* bitmap, calculating will be performed in */
{
	return aux_bitmap_calc(bitmap, 0, bitmap->total, 0);
}

/* 
   Yet another wrapper. It counts the number of marked/cleared blocks in specified
   region.
*/
uint64_t aux_bitmap_calc_marked_in_area(
	aux_bitmap_t *bitmap,	/* bitmap calculation will be performed in */
	uint64_t start,		/* start bit (block) */
	uint64_t end)		/* end bit (block) */
{
	return aux_bitmap_calc(bitmap, start, end, 1);
}

/* The same as previous one */
uint64_t aux_bitmap_calc_cleared_in_area(
	aux_bitmap_t *bitmap,	/* bitmap calculation will be performed in */
	uint64_t start,	        /* start bit */
	uint64_t end)		/* end bit */
{
	return aux_bitmap_calc(bitmap, start, end, 0);
}

/* Retuns stored value of marked blocks from specified bitmap */
uint64_t aux_bitmap_marked(
	aux_bitmap_t *bitmap)	/* bitmap marked block number will be obtained from */
{
	aal_assert("umka-343", bitmap != NULL, return FAKE_BLK);
	return bitmap->marked;
}

/* Retuns stored value of clear blocks from specified bitmap */
uint64_t aux_bitmap_cleared(
	aux_bitmap_t *bitmap)	/* bitmap unsuded blocks will be obtained from */
{
	aal_assert("umka-344", bitmap != NULL, return FAKE_BLK);
	return bitmap->total - bitmap->marked;
}

/* Creates instance of bitmap */
aux_bitmap_t *aux_bitmap_create(uint64_t len) {
	aux_bitmap_t *bitmap;
	    
	if (!(bitmap = (aux_bitmap_t *)aal_calloc(sizeof(*bitmap), 0)))
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

/* Makes clone of specified bitmap. Returns it to caller */
aux_bitmap_t *aux_bitmap_clone(
	aux_bitmap_t *bitmap)	    /* bitmap clone of which will be created */
{
	aux_bitmap_t *clone;

	aal_assert("umka-358", bitmap != NULL, return NULL);

	if (!(clone = aux_bitmap_create(bitmap->total)))
		return NULL;
	
	clone->marked = bitmap->marked;
	aal_memcpy(clone->map, bitmap->map, clone->size);
    
	return clone;
}

/* Frees all assosiated with bitmap memory */
void aux_bitmap_close(
	aux_bitmap_t *bitmap)	    /* bitmap to be closed */
{
	aal_assert("umka-354", bitmap != NULL, return);
	aal_assert("umka-1082", bitmap->map != NULL, return);
	
	aal_free(bitmap->map);
	aal_free(bitmap);
}

/* Returns bitmap's map (memory chunk, bits array placed in) for direct
 * access */
char *aux_bitmap_map(
	aux_bitmap_t *bitmap)	    /* bitmap, the bit array will be obtained from */
{
	aal_assert("umka-356", bitmap != NULL, return NULL);
	return bitmap->map;
}

