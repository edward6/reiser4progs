/*
    bitmap.c -- bitmap functions. Bitmap is used by bitmap-based block allocator 
    plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <aux/bitmap.h>
#include <aal/aal.h>

/* 
    This macros is used for checking whether given block is inside of allowed 
    range or not. It is used in all bitmap functions.
*/
#define aux_bitmap_range_check(bitmap, bit, action)			\
do {									\
    if (bit >= bitmap->total) {						\
	aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,		\
	    "Block %llu is out of range (0-%llu)", bit, bitmap->total); \
	action;								\
    }									\
} while (0)

/* 
    Checks whether passed block is inside of bitmap and marks it as used. This
    function also increses used blocks counter. 
*/
void aux_bitmap_mark(
    aux_bitmap_t *bitmap,	/* bitmap instance passed bit will be marked in */
    uint64_t bit		/* bit to be marked as used */
) {
    aal_assert("umka-336", bitmap != NULL, return);

    if (aal_test_bit(bit, bitmap->map))
	return;
	
    aal_set_bit(bit, bitmap->map);
    bitmap->used++;
}

/* 
    Checks whether passed block is inside of bitmap and marks it as free. This
    function also descreases used blocks counter. 
*/
void aux_bitmap_clear(
    aux_bitmap_t *bitmap,	/* bitmap, passed blk will be marked in */
    uint64_t bit		/* bit to be marked as free */
) {
    aal_assert("umka-337", bitmap != NULL, return);

    aux_bitmap_range_check(bitmap, bit, return);
    if (!aal_test_bit(bit, bitmap->map))
	return;
	
    aal_clear_bit(bit, bitmap->map);
    bitmap->used--;
}

/* 
    Checks whether passed block is inside of bitmap and test it. Returns TRUE
    if block is used, FALSE otherwise.
*/
int aux_bitmap_test(
    aux_bitmap_t *bitmap,	/* bitmap, passed blk will be tested */
    uint64_t bit		/* bit to be tested */
) {
    aal_assert("umka-338", bitmap != NULL, return 0);
    aux_bitmap_range_check(bitmap, bit, return 0);
    return aal_test_bit(bit, bitmap->map);
}

/* Finds first unused in bitmap block, starting from passed "start" */
uint64_t aux_bitmap_find(
    aux_bitmap_t *bitmap,	/* bitmap, unused bit will be searched in */
    uint64_t start		/* start bit, search should be performed from */
) {
    uint64_t bit;
	
    aal_assert("umka-339", bitmap != NULL, return 0);
	
    aux_bitmap_range_check(bitmap, start, return 0);

    if ((bit = aal_find_next_zero_bit(bitmap->map, 
	    bitmap->total, start)) >= bitmap->total)
	return ~0ull;

    return bit;
}

/*
    Makes loop through bitmap and calculates the number of used/unused blocks
    in it. If it is possible it tries to find contiguous bitmap areas (64 bit) 
    and in this maner increases performance. This function is used for checking 
    the bitmap on validness. Imagine, we have a number of free blocks in the super 
    block or somewhere else. And we can easily check whether this number equal 
    to actual returned one or not. Also it is used for calculating used blocks of 
    bitmap in aux_bitmap_open function. See bellow for details.
*/
static uint64_t aux_bitmap_calc(
    aux_bitmap_t *bitmap,	/* bitmap will be used for calculating bits */
    uint64_t start,		/* start bit, calculating should be performed from */
    uint64_t end,		/* end bit, calculating should be stoped on */
    int flag			/* flag for kind of calculating (used or free) */
) {
    uint64_t i, bits = 0;
	
    aal_assert("umka-340", bitmap != NULL, return 0);
	
    aux_bitmap_range_check(bitmap, start, return 0);
    aux_bitmap_range_check(bitmap, end - 1, return 0);
	
    for (i = start; i < end; i++)
	bits += aux_bitmap_test(bitmap, i) ? flag : !flag;

    return bits;
}

/* Public wrapper for previous function */
uint64_t aux_bitmap_calc_used(
    aux_bitmap_t *bitmap	/* bitmap, calculating will be performed in */
) {
    return (bitmap->used = aux_bitmap_calc(bitmap, 0, 
	bitmap->total, 1));
}

/* The same as previous one */
uint64_t aux_bitmap_calc_free(
    aux_bitmap_t *bitmap	/* bitmap, calculating will be performed in */
) {
    return aux_bitmap_calc(bitmap, 0, bitmap->total, 0);
}

/* 
    Yet another wrapper. It counts the number of used/unused blocks in specified 
    region.
*/
uint64_t aux_bitmap_calc_used_in_area(
    aux_bitmap_t *bitmap,	/* bitmap calculation will be performed in */
    uint64_t start,		/* start bit (block) */
    uint64_t end		/* end bit (block) */
) {
    return aux_bitmap_calc(bitmap, start, end, 1);
}

/* The same as previous one */
uint64_t aux_bitmap_calc_free_in_area(
    aux_bitmap_t *bitmap,	/* bitmap calculation will be performed in */
    uint64_t start,		/* start bit */
    uint64_t end		/* end bit */
) {
    return aux_bitmap_calc(bitmap, start, end, 0);
}

/* Retuns stored value of used blocks from specified bitmap */
uint64_t aux_bitmap_used(
    aux_bitmap_t *bitmap	/* bitmap used blocks number will be obtained from */
) {
    aal_assert("umka-343", bitmap != NULL, return 0);
    return bitmap->used;
}

/* Retuns stored value of free blocks from specified bitmap */
uint64_t aux_bitmap_free(
    aux_bitmap_t *bitmap	/* bitmap unsuded blocks will be obtained from */
) {
    aal_assert("umka-344", bitmap != NULL, return 0);
    return bitmap->total - bitmap->used;
}

/* Creates instance of bitmap */
aux_bitmap_t *aux_bitmap_create(uint64_t len) {
    aux_bitmap_t *bitmap;
	    
    if (!(bitmap = (aux_bitmap_t *)aal_calloc(sizeof(*bitmap), 0)))
	return NULL;
	
    bitmap->used = 0;
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
    aux_bitmap_t *bitmap	    /* bitmap clone of which will be created */
) {
    aux_bitmap_t *clone;

    aal_assert("umka-358", bitmap != NULL, return 0);	

    if (!(clone = aux_bitmap_create(bitmap->total)))
	return NULL;
	
    clone->used = bitmap->used;
    aal_memcpy(clone->map, bitmap->map, clone->size);
    
    return clone;
}

/* Frees all assosiated with bitmap memory */
void aux_bitmap_close(
    aux_bitmap_t *bitmap	    /* bitmap to be closed */
) {
    aal_assert("umka-354", bitmap != NULL, return);
    aal_assert("umka-1082", bitmap->map != NULL, return);
	
    aal_free(bitmap->map);
    aal_free(bitmap);
}

/* Returns bitmap's map (memory chunk, bits array placed in) for direct access */
char *aux_bitmap_map(
    aux_bitmap_t *bitmap	    /* bitmap, the bit array will be obtained from */
) {
    aal_assert("umka-356", bitmap != NULL, return NULL);
    return bitmap->map;
}

