/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */
/*
 * Coords
 */

#if !defined( __REISER4_NEW_COORD_H__ )
#define __REISER4_NEW_COORD_H__

#include "forward.h"
#include "debug.h"
#include "dformat.h"

/**
 * insertions happen between coords in the tree, so we need some means
 * of specifying the sense of betweenness.
 */
typedef enum {
	BEFORE_UNIT, /* Note: we/init_coord depends on this value being zero. */
	AT_UNIT,
	AFTER_UNIT,
	BEFORE_ITEM,
	AFTER_ITEM ,
	INVALID_COORD,
	EMPTY_NODE,
} between_enum;

/**
 * location of coord w.r.t. its node
 */
typedef enum {
	COORD_ON_THE_LEFT  = -1,
	COORD_ON_THE_RIGHT = +1,
	COORD_INSIDE       = 0
} coord_wrt_node;

typedef enum {
	COORD_CMP_SAME = 0, COORD_CMP_ON_LEFT = -1, COORD_CMP_ON_RIGHT = +1
} coord_cmp;

struct coord {
	/* node in a tree */
	znode        *node;

	/* position of item within node */
	pos_in_node item_pos;
	/* position of unit within item */
	pos_in_item   unit_pos;
	/** 
	 * position of coord w.r.t. to neighboring items and/or units. 
	 * Values are taken from &between_enum above.
	 */
	between_enum  between;
	/*
	 * optimization: plugin of item is stored in coord_t. Until this was
	 * implemented, item_plugin_by_coord() was major CPU consumer. ->iplug
	 * is invalidated (set to NULL) on each modification of ->item_pos,
	 * and all such modifications are funneled through coord_*_item_pos()
	 * functions below.
	 */
	item_plugin  *iplug;
};

#define MUTABLE_POS( c ) ( *( pos_in_node * )( &( c ) -> item_pos ) )

static inline void coord_set_item_pos( coord_t *coord, pos_in_node pos )
{
	assert( "nikita-2478", coord != NULL );
	MUTABLE_POS( coord ) = pos;
	coord -> iplug = NULL;
}

static inline void coord_dec_item_pos( coord_t *coord )
{
	assert( "nikita-2480", coord != NULL );
	-- MUTABLE_POS( coord );
	coord -> iplug = NULL;
}

static inline void coord_inc_item_pos( coord_t *coord )
{
	assert( "nikita-2481", coord != NULL );
	++ MUTABLE_POS( coord );
	coord -> iplug = NULL;
}

static inline void coord_add_item_pos( coord_t *coord, int delta )
{
	assert( "nikita-2482", coord != NULL );
	MUTABLE_POS( coord ) += delta;
	coord -> iplug = NULL;
}

/* Reverse a direction. */
static inline sideof sideof_reverse (sideof side)
{
	return side == LEFT_SIDE ? RIGHT_SIDE : LEFT_SIDE;
}

/* NOTE: There is a somewhat odd mixture of the following opposed terms:
 *
 * "first" and "last"
 * "next" and "prev"
 * "before" and "after"
 * "leftmost" and "rightmost"
 *
 * But I think the chosen names are decent the way they are.
 */

/*****************************************************************************************/
/*				    COORD INITIALIZERS                                   */
/*****************************************************************************************/

/* Initialize an invalid coordinate. */
extern void coord_init_invalid (coord_t *coord, const znode *node);

/* Initialize a coordinate to point at the first unit of the first item.  If the node is
 * empty, it is positioned at the EMPTY_NODE. */
extern void coord_init_first_unit (coord_t *coord, const znode *node);

/* Initialize a coordinate to point at the last unit of the last item.  If the node is
 * empty, it is positioned at the EMPTY_NODE. */
extern void coord_init_last_unit (coord_t *coord, const znode *node);

/* Initialize a coordinate to before the first item.  If the node is empty, it is
 * positioned at the EMPTY_NODE. */
extern void coord_init_before_first_item (coord_t *coord, const znode *node);

/* Initialize a coordinate to after the last item.  If the node is empty, it is positioned
 * at the EMPTY_NODE. */
extern void coord_init_after_last_item (coord_t *coord, const znode *node);

/* Initialize a coordinate to after last unit in the item. Coord must be set
 * already to existing item */
void coord_init_after_item_end (coord_t *coord);

/* Calls either coord_init_first_unit or coord_init_last_unit depending on sideof argument. */
extern void coord_init_sideof_unit (coord_t *coord, const znode *node, sideof dir);

/* Initialize a parent hint pointer. (parent hint pointer is a field in znode,
 * look for comments there)
 * FIXME-VS: added by vs (2002, june, 8) */
extern void coord_init_parent_hint (coord_t *coord, const znode *node);

/* Initialize a coordinate by 0s. Used in places where init_coord was used and
 * it was not clear how actually 
 * FIXME-VS: added by vs (2002, june, 8) */
extern void coord_init_zero (coord_t *coord);

/*****************************************************************************************/
/*				      COORD METHODS                                      */
/*****************************************************************************************/

/* after shifting of node content, coord previously set properly may become
 * invalid, try to "normalize" it. */
void coord_normalize (coord_t * coord);

/* Copy a coordinate. */
extern void coord_dup (coord_t *coord, const coord_t *old_coord);

/* Copy a coordinate without check. */
void coord_dup_nocheck (coord_t *coord, const coord_t *old_coord);

/* Return the number of items at the present node.  Asserts coord->node != NULL. */
extern unsigned coord_num_items (const coord_t *coord);

/* Return the number of units at the present item.  Asserts coord_is_existing_item(). */
extern unsigned coord_num_units (const coord_t *coord);

/* Return the last valid unit number at the present item (i.e., coord_num_units() - 1). */
extern unsigned coord_last_unit_pos (const coord_t *coord);

#if REISER4_DEBUG
/* For assertions only, checks for a valid coordinate. */
extern int  coord_check (const coord_t *coord);
#endif

/* Returns true if two coordinates are consider equal.  Coordinates that are between units
 * or items are considered equal. */
extern int coord_eq (const coord_t *c1, const coord_t *c2);

#if REISER4_DEBUG_OUTPUT
/* For debugging, error messages. */
extern void print_coord (const char * mes, 
			 const coord_t * coord, int print_node);
#else
#define print_coord(m,c,n) noop
#endif

/* If coord_is_after_rightmost return NCOORD_ON_THE_RIGHT, if coord_is_after_leftmost
 * return NCOORD_ON_THE_LEFT, otherwise return NCOORD_INSIDE. */
extern coord_wrt_node coord_wrt (const coord_t *coord);

/* Returns true if the coordinates are positioned at adjacent units, regardless of
 * before-after or item boundaries. */
extern int  coord_are_neighbors (coord_t *c1, coord_t *c2);

/* Assuming two coordinates are positioned in the same node, return NCOORD_CMP_ON_RIGHT,
 * NCOORD_CMP_ON_LEFT, or NCOORD_CMP_SAME depending on c1's position relative to c2.  */
extern coord_cmp coord_compare (coord_t * c1, coord_t * c2);
/* Returns the current item positions.  Asserts non-empty. */
extern unsigned coord_item_pos (const coord_t *coord);

/*****************************************************************************************/
/*				     COORD PREDICATES                                    */
/*****************************************************************************************/

/* Returns true if the coord was initializewd by coord_init_invalid (). */
extern int coord_is_invalid (const coord_t *coord);

/* Returns true if the coordinate is positioned at an existing item, not before or after
 * an item.  It may be placed at, before, or after any unit within the item, whether
 * existing or not.  If this is true you can call methods of the item plugin.  */
extern int coord_is_existing_item (const coord_t *coord);

/* Returns true if the coordinate is positioned after a item, before a item, after the
 * last unit of an item, before the first unit of an item, or at an empty node. */
extern int coord_is_between_items (const coord_t *coord);

/* Returns true if the coordinate is positioned at an existing unit, not before or after a
 * unit. */
extern int coord_is_existing_unit (const coord_t *coord);

/* Returns true if the coordinate is positioned at an empty node. */
extern int coord_is_empty (const coord_t *coord);

/* Returns true if the coordinate is positioned at the first unit of the first item.  Not
 * true for empty nodes nor coordinates positioned before the first item. */
extern int coord_is_leftmost_unit (const coord_t *coord);

/* Returns true if the coordinate is positioned at the last unit of the last item.  Not
 * true for empty nodes nor coordinates positioned after the last item. */
extern int coord_is_rightmost_unit (const coord_t *coord);

/* Returns true if the coordinate is positioned at any unit of the last item.  Not true
 * for empty nodes nor coordinates positioned after the last item. */
extern int coord_is_rightmost_item (const coord_t *coord);

/* Returns true if the coordinate is positioned after the last item or after the last unit
 * of the last item or it is an empty node. */
extern int coord_is_after_rightmost (const coord_t *coord);

/* Returns true if the coordinate is positioned before the first item or it is an empty
 * node. */
extern int coord_is_before_leftmost (const coord_t *coord);

/* Calls either coord_is_before_leftmost or coord_is_after_rightmost depending on sideof
 * argument. */
extern int coord_is_after_sideof_unit (coord_t *coord, sideof dir);

/* Returns true if coord is set to or before the first (if LEFT_SIDE) unit of the item and
 * to or after the last (if RIGHT_SIDE) unit of the item. */
extern int coord_is_delimiting (coord_t *coord, sideof dir);

/*****************************************************************************************/
/* 				      COORD MODIFIERS                                    */
/*****************************************************************************************/

/* Advances the coordinate by one unit to the right.  If empty, no change.  If
 * coord_is_rightmost_unit, advances to AFTER THE LAST ITEM.  Returns 0 if new position is
 * an existing unit. */
extern int coord_next_unit (coord_t *coord);

/* Advances the coordinate by one item to the right.  If empty, no change.  If
 * coord_is_rightmost_unit, advances to AFTER THE LAST ITEM.  Returns 0 if new position is
 * an existing item. */
extern int coord_next_item (coord_t *coord);

/* Advances the coordinate by one unit to the left.  If empty, no change.  If
 * coord_is_leftmost_unit, advances to BEFORE THE FIRST ITEM.  Returns 0 if new position
 * is an existing unit. */
extern int coord_prev_unit (coord_t *coord);

/* Advances the coordinate by one item to the left.  If empty, no change.  If
 * coord_is_leftmost_unit, advances to BEFORE THE FIRST ITEM.  Returns 0 if new position
 * is an existing item. */
extern int coord_prev_item (coord_t *coord);

/* If the coordinate is between items, shifts it to the right.  Returns 0 on success and
 * non-zero if there is no position to the right. */
extern int coord_set_to_right (coord_t *coord);

/* If the coordinate is between items, shifts it to the left.  Returns 0 on success and
 * non-zero if there is no position to the left. */
extern int coord_set_to_left (coord_t *coord);

/* return true if coord is set after last unit in an item. 0 - otherwise. It is
 * used mostly to avoid repeated tree traversals writing to a file
 * sequentially */
int coord_is_after_last_unit (coord_t *coord);

/* If the coordinate is before an item/unit, set to next item/unit.  If the coordinate is
 * after an item/unit, set to the previous item/unit.  Returns 0 on success and non-zero
 * if there is no position (i.e., if the coordinate is empty). */
extern int coord_set_to_unit (coord_t *coord);

/* If the coordinate is at an existing unit, set to after that unit.  Returns 0 on success
 * and non-zero if the unit did not exist. */
extern int coord_set_after_unit (coord_t *coord);

/* Calls either coord_next_unit or coord_prev_unit depending on sideof argument. */
extern int coord_sideof_unit (coord_t *coord, sideof dir);

/** iterate over all units in @node */
#define for_all_units( coord, node )					\
	for( coord_init_before_first_item( ( coord ), ( node ) ) ; 	\
	     coord_next_unit( coord ) == 0 ; )

/** iterate over all items in @node */
#define for_all_items( coord, node )					\
	for( coord_init_before_first_item( ( coord ), ( node ) ) ; 	\
	     coord_next_item( coord ) == 0 ; )

#if REISER4_DEBUG
extern const char * coord_tween_tostring (between_enum n);
#endif

/*****************************************************************************************/
/* 				     COORD/ITEM METHODS                                  */
/*****************************************************************************************/

extern int          item_utmost_child_real_block (const coord_t *coord, sideof side, reiser4_block_nr *blk);
extern int          item_utmost_child            (const coord_t *coord, sideof side, jnode **child);

/* __REISER4_NEW_COORD_H__ */
#endif

/* 
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * scroll-step: 1
 * End:
 */
