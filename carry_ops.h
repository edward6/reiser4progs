/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * implementation of carry operations
 */

#if !defined( __CARRY_OPS_H__ )
#define __CARRY_OPS_H__

/*
 * carry operation handlers
 */

typedef int ( *carry_op_handler )( carry_op *op,
				   carry_level *doing, carry_level *todo );

/**
 * This is dispatch table for carry operations. It can be trivially
 * abstracted into useful plugin: tunable balancing policy is a good
 * thing.
 **/
extern carry_op_handler op_dispatch_table[ COP_LAST_OP ];

unsigned int space_needed( const znode *node, const new_coord *coord,
			   const reiser4_item_data *data, int inserting );
extern carry_node *find_left_carry( carry_node *node, carry_level *level );
extern carry_node *find_right_carry( carry_node *node, carry_level *level );



/* __CARRY_OPS_H__ */
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
