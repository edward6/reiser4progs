/*
 * Copyright 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Tree Access Pointers.
 */

#if !defined( __REISER4_TAP_H__ )
#define __REISER4_TAP_H__

TS_LIST_DECLARE( tap );

struct tree_access_pointer {
	coord_t         *coord;
	lock_handle     *lh;
	znode_lock_mode  mode;
	int              loaded;
	tap_list_link    linkage;
};

TS_LIST_DEFINE( tap, tap_t, linkage );

typedef int ( *go_actor_t )( tap_t *tap );

extern int  tap_load    ( tap_t *tap );
extern void tap_relse   ( tap_t *tap );
extern void tap_init    ( tap_t *tap, coord_t *coord, 
			  lock_handle *lh, znode_lock_mode mode );
extern void tap_monitor ( tap_t *tap );
extern void tap_done    ( tap_t *tap );
extern int  tap_move    ( tap_t *tap, lock_handle *target );
extern int  tap_to      ( tap_t *tap, znode *target );
extern int  tap_to_coord( tap_t *tap, coord_t *target );

extern int  go_dir_el   ( tap_t *tap, sideof dir, int units_p );
extern int  go_next_unit( tap_t *tap );
extern int  go_prev_unit( tap_t *tap );
extern int  rewind_to   ( tap_t *tap, go_actor_t actor, int shift );
extern int  rewind_right( tap_t *tap, int shift );
extern int  rewind_left ( tap_t *tap, int shift );

extern tap_list_head *taps_list( void );

#define for_all_taps( tap )				\
	for (tap = tap_list_front ( taps_list() ); 	\
	         ! tap_list_end   ( taps_list(), tap );	\
	     tap = tap_list_next  ( tap ) )


/* __REISER4_TAP_H__ */
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
