/*
 * Declarations of debug macros.
 *
 * Copyright 2000, 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 *
 */

#if !defined( __FS_REISER4_DEBUG_H__ )
#define __FS_REISER4_DEBUG_H__

/** basic debug/logging output macro. "label" is unfamous "maintainer-id" */

/** generic function to produce formatted output, decorating it with
    whatever standard prefixes/postfixes we want. "Fun" is a function
    that will be actually called, can be printk, panic etc.
    This is for use by other debugging macros, not by users. */
#define DCALL( lev, fun, label, format, args... )				\
         do { fun( lev "reiser4[%.16s(%i)]: %s (%s:%i)[%s]: " format "\n",	\
		       no_context ? "interrupt" : current_pname,		\
		       no_context ? -1 : current_pid,				\
		       __func__, __FILE__, __LINE__, label ,  ##args );		\
		      } while( 0 )

/** panic. Print backtrace and die */
#define rpanic( label, format, args... )		\
	DCALL( KERN_EMERG, reiser4_panic, label, format , ##args )
/** print message with indication of current process, file, line and
    function */
#define rlog( label, format, args... ) 				\
	DCALL( KERN_DEBUG, printk, label, format , ##args )
/** use info() for output without any kind of prefix like
    when doing output in several chunks. */
#define info( format, args... ) printk( format , ##args )

/** Assertion checked during compilation. 
    If "cond" is false (0) we get duplicate case label in switch.
    Use this to check something like famous 
       cassert (sizeof(struct reiserfs_journal_commit) == 4096) ;
    in 3.x journal.c. If cassertion fails you get compiler error,
    so no "maintainer-id". 
    From post by Andy Chou <acc@CS.Stanford.EDU> at lkml. */
#define cassert( cond ) ({ switch( -1 ) { case ( cond ): case 0: break; } })

#ifndef __KERNEL__
#define CONFIG_REISER4_CHECK
#endif

#ifdef __KERNEL__
# if CONFIG_SMP
#  define ON_SMP( e ) e
# else
#  define ON_SMP( e )
# endif
#else
# define ON_SMP( e ) e
#endif

#ifndef REISER4_DEBUG

#if defined( CONFIG_REISER4_CHECK )

/* have assert to check condition only */
#define REISER4_DEBUG (1)
/* have assert to check condition and check stack */
/*#define REISER4_DEBUG (2)*/
/* have assert to do everything */
/*#define REISER4_DEBUG (3)*/

#else

#define REISER4_DEBUG (0)

#endif /* #if defined( CONFIG_REISER4_CHECK ) */

#endif /* REISER4_DEBUG */

#define REISER4_DEBUG_MODIFY 0 /* this significantly slows down testing, but we should run
				* our testsuite through with this every once in a
				* while. */

#define noop   do {;} while( 0 )

#if REISER4_DEBUG
/** version of info that only actually prints anything when _d_ebugging
    is on */
#define dinfo( format, args... ) info( format , ##args )
/** macro to catch logical errors. Put it into `default' clause of
    switch() statement. */
#define impossible( label, format, args... ) 			\
         rpanic( label, "impossible: " format , ##args )
/** stub for something you are planning to implement in a future */
#define not_implemented( label, format, args... )	\
         rpanic( label, "not implemented: " format , ##args )
/**
 * assert assures that @cond is true. If it is not, rpanic() is
 * called. Use this for checking logical consistency and _never_ call
 * this to check correctness of external data: disk blocks and user-input .
 */
#define assert( label, cond )					\
({								\
	check_preempt();					\
	check_stack();						\
	check_spinlocks_array();				\
	if( unlikely( !( cond ) ) )				\
		rpanic( label, "assertion failed: " #cond );	\
})

/**
 * like assertion, but @expr is evaluated even if REISER4_DEBUG is off.
 */
#define check_me( label, expr )	assert( label, ( expr ) )

#define ON_DEBUG( exp ) exp

typedef struct lock_counters_info {
	int                   spin_locked_jnode;
	int                   spin_locked_tree;
	int                   spin_locked_dk;
	int                   spin_locked_txnh;
	int                   spin_locked_atom;
	int                   spin_locked_stack;
	int                   spin_locked_txnmgr;
	int                   spin_locked_inode;
	int                   spin_locked;
	int                   long_term_locked_znode;

	int                   d_refs;
	int                   x_refs;
	int                   t_refs;
} lock_counters_info;

extern lock_counters_info *lock_counters(void);

/**
 * flags controlling debugging behavior. Are set through debug=N mount option.
 */
typedef enum {
	/**
	 * print a lot of information during panic.
	 */
	REISER4_VERBOSE_PANIC     = 0x00000001,
	/**
	 * print a lot of information during umount
	 */
	REISER4_VERBOSE_UMOUNT    = 0x00000002
} reiser4_debug_flags;

extern int reiser4_is_debugged( struct super_block *super, __u32 flag );
extern int reiser4_are_all_debugged( struct super_block *super, __u32 flags );

#else

#define dinfo( format, args... ) noop
#define impossible( label, format, args... ) noop
#define not_implemented( label, format, args... ) noop
#define assert( label, cond ) noop
#define check_me( label, expr )	( ( void ) ( expr ) )
#define ON_DEBUG( exp )

#define reiser4_is_debugged( super, flag )       (0)
#define reiser4_are_all_debugged( super, flags ) (0)

/* REISER4_DEBUG */
#endif

#if REISER4_DEBUG_MODIFY
#define ON_DEBUG_MODIFY( exp ) exp
#else
#define ON_DEBUG_MODIFY( exp )
#endif

#ifndef __KERNEL__
#define wprint( args... ) ( fprintf( stderr , ##args ) )
#else
#define wprint( args... ) ( printk( ##args ) )
#endif

#define wrong_return_value( label, function )				\
	impossible( label, "wrong return value from " function )
#define warning( label, format, args... )					\
	DCALL( KERN_WARNING, wprint, label, "WARNING: " format , ##args )
#define not_yet( label, format, args... )				\
	rpanic( label, "NOT YET IMPLEMENTED: " format , ##args )

/** tracing facility.

    REISER4_DEBUG doesn't necessary implies tracing, because tracing is
    only meaningful during debugging and can produce big amonts of
    output useless for average user.
*/

#ifndef REISER4_TRACE
#define REISER4_TRACE (1)
#endif

#if REISER4_TRACE
/* helper macro for tracing, see trace_stamp() below. */
#define trace_if( flags, e ) 							\
	if( get_current_context() && get_current_trace_flags() & (flags) ) e
#else
#define trace_if( flags, e ) noop
#endif

/*
 * tracing flags.
 */
typedef enum {
	/*
	 * trace nothing
	 */
	NO_TRACE         =     0,
	/*
	 * trace vfs interaction functions from vfs_ops.c
	 */
	TRACE_VFS_OPS    =     (1 << 0),     /* 0x00000001 */
	/*
	 * trace plugin handling functions
	 */
	TRACE_PLUGINS    =     (1 << 1),     /* 0x00000002 */
	/*
	 * trace tree traversals
	 */
	TRACE_TREE       =     (1 << 2),     /* 0x00000004 */
	/*
	 * trace znode manipulation functions
	 */
	TRACE_ZNODES     =     (1 << 3),     /* 0x00000008 */
	/*
	 * trace node layout functions
	 */
	TRACE_NODES      =     (1 << 4),     /* 0x00000010 */
	/*
	 * trace directory functions
	 */
	TRACE_DIR        =     (1 << 5),     /* 0x00000020 */
	/*
	 * trace flush code verbosely
	 */
	TRACE_FLUSH_VERB   =     (1 << 6),     /* 0x00000040 */
	/*
	 * trace flush code
	 */
	TRACE_FLUSH       =     (1 << 7),     /* 0x00000080 */
	/*
	 * trace carry
	 */
	TRACE_CARRY      =     (1 << 8),     /* 0x00000100 */
	/*
	 * trace how tree (web) of znodes if maintained through tree
	 * balancings.
	 */
	TRACE_ZWEB       =     (1 << 9),     /* 0x00000200 */
	/*
	 * trace transactions.
	 */
	TRACE_TXN        =     (1 << 10),     /* 0x00000400 */
	/*
	 * trace object id allocation/releasing
	 */
	TRACE_OIDS       =     (1 << 11),     /* 0x00000800 */
	/*
	 * trace item shifts
	 */
	TRACE_SHIFT      =     (1 << 12),     /* 0x00001000 */
	/*
	 * trace page cache
	 */
	TRACE_PCACHE      =    (1 << 13),     /* 0x00002000 */
	/*
	 * trace extents
	 */
	TRACE_EXTENTS     =    (1 << 14),     /* 0x00004000 */
	/*
	 * trace locks
	 */
	TRACE_LOCKS       =    (1 << 15),     /* 0x00008000 */
	/*
	 * trace coords
	 */
	TRACE_COORDS      =    (1 << 16),     /* 0x00010000 */
	/*
	 * trace read-IO functions
	 */
	TRACE_IO_R        =     (1 << 17),     /* 0x00020000 */
	/*
	 * trace write-IO functions
	 */
	TRACE_IO_W        =     (1 << 18),     /* 0x00040000 */

	/*
	 * trace log writing
	 */
	TRACE_LOG         =     (1 << 19),     /* 0x00080000 */

	/*
	 * trace journal replaying
	 */
	TRACE_REPLAY      =     (1 << 20),     /* 0x00100000 */

	/*
	 * trace space allocation
	 */
	TRACE_ALLOC       =     (1 << 21),     /* 0x00200000 */

	/*
	 * vague section: used to trace bugs. Use it to issue optional prints
	 * at arbitrary points of code.
	 */
	TRACE_BUG        =     (1 << 31),     /* 0x80000000 */
	/*
	 * trace everything above
	 */
	TRACE_ALL        =     0xffffffffu
} reiser4_trace_flags;

extern __u32 reiser4_current_trace_flags;

/** just print where we are: file, function, line */
#define trace_stamp( f )   trace_if( f, rlog( "trace", "" ) )
/** print value of "var" */
#define trace_var( f, format, var ) 				\
        trace_if( f, rlog( "trace", #var ": " format, var ) )
/** print output only if appropriate trace flag(s) is on */
#define trace_on( f, args... )   trace_if( f, dinfo( ##args ) )

#ifndef REISER4_STATS
#define REISER4_STATS (1)
#endif

#if REISER4_STATS

/* following macros update counters from &reiser4_stat below, which
   see */

#define ON_STATS( e ) e
#define STS ( get_super_private_nocheck( reiser4_get_current_sb() ) -> stats )
#define ST_INC_CNT( field ) ( ++ STS.field )

/*
 * Macros to gather statistical data. If REISER4_STATS is disabled, they
 * are preprocessed to nothing.
 *
 * reiser4_stat_foo_add( counter ) increases by one counter in foo section of 
 * &reiser4_stat - big struct used to collect all statistical data.
 *
 */

#define reiser4_stat_key_add( stat ) ST_INC_CNT( key. ## stat )
#define	reiser4_stat_tree_add( stat ) ST_INC_CNT( tree. ## stat )
#define reiser4_stat_znode_add( stat ) ST_INC_CNT( znode. ## stat )
#define reiser4_stat_dir_add( stat ) ST_INC_CNT( dir. ## stat )
#define reiser4_stat_file_add( stat ) ST_INC_CNT( file. ## stat )
#define reiser4_stat_flush_add( stat ) ST_INC_CNT( flush. ## stat )
#define reiser4_stat_pool_add( stat ) ST_INC_CNT( pool. ## stat )
#define reiser4_stat_seal_add( stat ) ST_INC_CNT( seal. ## stat )

#define	reiser4_stat_add_at_level( lev, stat )				\
({									\
	tree_level level;						\
									\
	assert ("green-10", lev >= LEAF_LEVEL );			\
	level = ( lev ) - LEAF_LEVEL;					\
	if( ( lev ) < REAL_MAX_ZTREE_HEIGHT ) {				\
		ST_INC_CNT( level[ level ]. ## stat );			\
		ST_INC_CNT( level[ level ]. total_hits_at_level );	\
	}								\
})

#define	reiser4_stat_level_add( l, stat )			\
	reiser4_stat_add_at_level( ( l ) -> level_no, stat )

#define MAX_CNT( field, value )						\
({									\
	if( get_super_private_nocheck( reiser4_get_current_sb() ) &&	\
	    ( value ) > STS.field )					\
		STS.field = ( value );					\
})

#define reiser4_stat_nuniq_max( gen )			\
({							\
	ST_INC_CNT( non_uniq );				\
	MAX_CNT( non_uniq_max, gen );			\
})

#define reiser4_stat_stack_check_max( gap ) MAX_CNT( stack_size_max, gap )

/* statistics gathering features. */

/*
 * type of statistics counters
 */
typedef unsigned long long stat_cnt;

/*
 * set of statistics counter. This is embedded into super-block when
 * REISER4_STATS is on.
 */
typedef struct reiser4_statistics {
	struct {
		/*
		 * calls to coord_by_key
		 */
		stat_cnt cbk;
		/*
		 * calls to coord_by_key that found requested key
		 */
		stat_cnt cbk_found;
		/*
		 * calls to coord_by_key that didn't find requested key
		 */
		stat_cnt cbk_notfound;
		/*
		 * number of times calls to coord_by_key restarted
		 */
		stat_cnt cbk_restart;
		/*
		 * calls to coord_by_key that found key in coord cache
		 */
		stat_cnt cbk_cache_hit;
		/*
		 * calls to coord_by_key that didn't find key in coord
		 * cache
		 */
		stat_cnt cbk_cache_miss;
		/*
		 * cbk cache search found item at the edge of a node with
		 * possibly non-unique key
		 */
		stat_cnt cbk_cache_utmost;
		/*
		 * search for key in coord cache raced against parallel
		 * balancing and lose. This should be rare. If not,
		 * update cbk_cache_search() according to comment
		 * therewithin.
		 */
		stat_cnt cbk_cache_race;
		/*
		 * number of times coord of child in its parent, cached
		 * in a former, was reused.
		 */
		stat_cnt pos_in_parent_hit;
		/*
		 * number of time binary search for child position in
		 * its parent had to be redone.
		 */
		stat_cnt pos_in_parent_miss;
		/*
		 * number of times position of child in its parent was
		 * cached in the former
		 */
		stat_cnt pos_in_parent_set;
		/*
		 * how many times carry() was skipped by doing "fast
		 * insertion path". See
		 * fs/reiser4/plugin/node/node.h:->fast_insert() method.
		 */
		stat_cnt fast_insert;
		/*
		 * how many times carry() was skipped by doing "fast
		 * paste path". See
		 * fs/reiser4/plugin/node/node.h:->fast_paste() method.
		 */
		stat_cnt fast_paste;
		/*
		 * how many times carry() was skipped by doing "fast
		 * cut path". See
		 * fs/reiser4/plugin/node/node.h:->cut_insert() method.
		 */
		stat_cnt fast_cut;
		/*
		 * children reparented due to shifts at the parent level
		 */
		stat_cnt reparenting;
		/*
		 * right delimiting key is not exact
		 */
		stat_cnt rd_key_skew;
		/*
		 * how many times lookup_multikey() has to restart from the
		 * beginning because of the broken seal.
		 */
		stat_cnt multikey_restart;
		stat_cnt check_left_nonuniq;
		stat_cnt left_nonuniq_found;
	} tree;
	struct {
		/*
		 * carries restarted due to deadlock avoidance algorithm
		 */
		stat_cnt carry_restart;
		/*
		 * carries performed
		 */
		stat_cnt carry_done;
		/*
		 * how many times carry, trying to find left neighbor of
		 * a given node, found it already in a carry set.
		 */
		stat_cnt carry_left_in_carry;
		/*
		 * how many times carry, trying to find left neighbor of
		 * a given node, found it already in a memory.
		 */
		stat_cnt carry_left_in_cache;
		/*
		 * how many times carry, trying to find left neighbor of
		 * a given node, found that left neighbor either doesn't
		 * exist (we are at the left border of the tree
		 * already), or that there is extent on the left.
		 */
		stat_cnt carry_left_not_avail;
		/*
		 * how many times carry, trying to find left neighbor of
		 * a given node, gave this up to avoid deadlock
		 */
		stat_cnt carry_left_refuse;
		/*
		 * how many times carry, trying to find right neighbor of
		 * a given node, found it already in a carry set.
		 */
		stat_cnt carry_right_in_carry;
		/*
		 * how many times carry, trying to find right neighbor of
		 * a given node, found it already in a memory.
		 */
		stat_cnt carry_right_in_cache;
		/*
		 * how many times carry, trying to find right neighbor
		 * of a given node, found that right neighbor either
		 * doesn't exist (we are at the right border of the tree
		 * already), or that there is extent on the right.
		 */
		stat_cnt carry_right_not_avail;
		/*
		 * how many times insertion has to look into the left
		 * neighbor, searching for the free space.
		 */
		stat_cnt insert_looking_left;
		/*
		 * how many times insertion has to look into the right
		 * neighbor, searching for the free space.
		 */
		stat_cnt insert_looking_right;
		/*
		 * how many times insertion has to allocate new node,
		 * searching for the free space.
		 */
		stat_cnt insert_alloc_new;
		/*
		 * how many times insertion has to allocate several new
		 * nodes in a row, searching for the free space.
		 */
		stat_cnt insert_alloc_many;
		/*
		 * how many insertions were performed by carry.
		 */
		stat_cnt insert;
		/*
		 * how many deletions were performed by carry.
		 */
		stat_cnt delete;
		/*
		 * how many cuts were performed by carry.
		 */
		stat_cnt cut;
		/*
		 * how many pastes (insertions into existing items) were
		 * performed by carry.
		 */
		stat_cnt paste;
		/*
		 * how many extent insertions were done by carry.
		 */
		stat_cnt extent;
		/*
		 * how many paste operations were restarted as insert.
		 */
		stat_cnt paste_restarted;
		/*
		 * how many updates of delimiting keys were performed
		 * by carry.
		 */
		stat_cnt update;
		/*
		 * how many times carry notified parent node about
		 * updates in its child.
		 */
		stat_cnt modify;
		/*
		 * how many times node was found reparented at the time
		 * when its parent has to be updated.
		 */
		stat_cnt half_split_race;
		/*
		 * how many times new node was inserted into sibling list
		 * after concurrent balancing modified right delimiting key if
		 * its left neighbor.
		 */
		stat_cnt dk_vs_create_race;
		/*
		 * how many times insert or paste ultimately went into
		 * node different from original target
		 */
		stat_cnt track_lh;
		/*
		 * how many times sibling lookup required getting that high in
		 * a tree
		 */
		stat_cnt sibling_search;
		/*
		 * key was moved out of node while thread was waiting
		 * for the lock
		 */		
		stat_cnt cbk_key_moved;
		/*
		 * node was moved out of tree while thread was waiting
		 * for the lock
		 */		
		stat_cnt cbk_met_ghost;
		stat_cnt total_hits_at_level;
	} level[ REAL_MAX_ZTREE_HEIGHT ];
	struct {
		/*
		 * calls to zload()
		 */
		stat_cnt zload;
		/*
		 * calls to zload() that actually had to read data
		 */
		stat_cnt zload_read;
		/*
		 * calls to lock_znode()
		 */
		stat_cnt lock_znode;
		/*
		 * number of times loop inside lock_znode() was executed
		 */
		stat_cnt lock_znode_iteration;
		/*
		 * calls to lock_neighbor()
		 */
		stat_cnt lock_neighbor;
		/*
		 * number of times loop inside lock_neighbor() was
		 * executed
		 */
		stat_cnt lock_neighbor_iteration;
	} znode;
	struct {
	} dir;
	struct {
		stat_cnt wait_on_page;
		stat_cnt fsdata_alloc;
		stat_cnt private_data_alloc;
		/*
		 * reads performed
		 */
		stat_cnt reads;
		/*
		 * writes performed
		 */
		stat_cnt writes;
		/*
		 * how many times extent_write asked to repeat
		 * search_by_key
		 */
		stat_cnt write_repeats;
		/* number of tail conversions */
		stat_cnt tail2extent;
		stat_cnt extent2tail;
		/* how many times find_next_item was called */
		stat_cnt find_items;
		/* how many times find_next_item had to call coord_by_key */
		stat_cnt full_find_items;
		/* pointers to unformatted nodes added */
		stat_cnt pointers;

	} file;
	struct {
		/*
		 * how many nodes were squeezed
		 */
		stat_cnt squeeze;
		/*
		 * how many times batches carry queue was flushed during
		 * squeezing
		 */
		stat_cnt flush_carry;
		/*
		 * how many nodes were squeezed to left neighbor completely
		 */
		stat_cnt squeezed_completely;
	} flush;
	struct {
		/*
		 * how many carry objects were allocated
		 */
		stat_cnt pool_alloc;
		/*
		 * how many "extra" carry objects were allocated by
		 * kmalloc.
		 */
		stat_cnt pool_kmalloc;
	} pool;
	struct {
		/*
		 * seals that were found pristine
		 */
		stat_cnt perfect_match;
		/*
		 * how many times key drifted from sealed node
		 */
		stat_cnt key_drift;
		/*
		 * how many times node under seal was out of cache
		 */
		stat_cnt out_of_cache;
		/*
		 * how many times wrong node was found under seal
		 */
		stat_cnt wrong_node;
		/*
		 * how many times coord was found in exactly the same position
		 * under seal
		 */
		stat_cnt didnt_move;
		/*
		 * how many times key was actually found under seal
		 */
		stat_cnt found;
	} seal;
	/*
	 * how many non-unique keys were scanned into tree
	 */
	stat_cnt non_uniq;
	/*
	 * maximal length of sequence of items with identical keys found
	 * in a tree
	 */
	stat_cnt non_uniq_max;
	/*
	 * maximal stack size ever consumed by reiser4 thread.
	 */
	stat_cnt stack_size_max;
	struct {
		/*
		 * how many times keycmp() was called
		 */
		stat_cnt eq0;
		/*
		 * how many times keycmp() found first 64 bits of a key
		 * equal
		 */
		stat_cnt eq1;
		/*
		 * how many times keycmp() found first 128 bits of a key
		 * equal
		 */
		stat_cnt eq2;
		/*
		 * how many times keycmp() found equal keys
		 */
		stat_cnt eq3;
	} key;
} reiser4_stat;

#else

#define ON_STATS( e ) noop

#define	reiser4_stat_key_add( stat ) noop
#define	reiser4_stat_tree_add( stat ) noop
#define	reiser4_stat_tree_level_add( level, stat ) noop
#define reiser4_stat_znode_add( stat ) noop
#define reiser4_stat_dir_add( stat ) noop
#define reiser4_stat_flush_add( stat ) noop
#define reiser4_stat_pool_add( stat ) noop
#define reiser4_stat_file_add( stat ) noop
#define	reiser4_stat_add_at_level( lev, stat ) noop
#define	reiser4_stat_level_add( l, stat ) noop
#define reiser4_stat_nuniq_max( gen ) noop
#define reiser4_stat_stack_check_max( gap ) noop
#define reiser4_stat_seal_add( stat ) noop
typedef struct {} reiser4_stat;

#endif

extern void reiser4_panic( const char *format, ... ) 
__attribute__( ( noreturn, format( printf, 1, 2 ) ) );

extern void check_preempt( void );
extern int  preempt_point( void );
extern void reiser4_print_stats( void );

extern void *reiser4_kmalloc( size_t size, int gfp_flag );
extern void  reiser4_kfree( void *area, size_t size );
extern __u32 get_current_trace_flags( void );

#if REISER4_DEBUG
extern int no_counters_are_held(void);
extern void check_stack( void );
extern void check_spinlocks_array( void );
extern void print_lock_counters( const char *prefix, lock_counters_info *info );
#else
#define print_lock_counters( p, i ) noop
#endif

#define REISER4_STACK_ABORT          (8192 - sizeof( struct task_struct ) - 30)
#define REISER4_STACK_GAP            (REISER4_STACK_ABORT - 100)

/* __FS_REISER4_DEBUG_H__ */
#endif

/* 
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
