/*
    joint.c -- the personalisation of the reiser4 on-disk node. The libreiser4
    internal in-memory tree consists of reiser4_joint_t structures.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <reiser4/reiser4.h>

/* Creates joint instance based on passed node */
reiser4_joint_t *reiser4_joint_create(
    reiser4_node_t *node	/* the first component of joint */
) {
    reiser4_joint_t *joint;

    aal_assert("umka-1268", node != NULL, return NULL);
    
    /* Allocating memory for instance of joint */
    if (!(joint = aal_calloc(sizeof(*joint), 0)))
	return NULL;

    joint->node = node;
    return joint;
}

/* Makes duplicate of the passed @joint */
errno_t reiser4_joint_dup(
    reiser4_joint_t *joint,	/* joint to be duplicated */
    reiser4_joint_t *dup	/* the clone will be saved */
) {
    aal_assert("umka-1264", joint != NULL, return -1);
    aal_assert("umka-1265", dup != NULL, return -1);

    *dup = *joint;
    return 0;
}

/* Freeing passed joint */
void reiser4_joint_close(
    reiser4_joint_t *joint	/* joint to be freed */
) {
    aal_list_t *children;
    
    aal_assert("umka-793", joint != NULL, return);
    
    children = joint->children ? 
	aal_list_first(joint->children) : NULL;

    if (children) {
	aal_list_t *walk;
	
	/* Recurcive calling the same function in order to free all children too */
	aal_list_foreach_forward(walk, children)
	    reiser4_joint_close((reiser4_joint_t *)walk->data);

	aal_list_free(children);
	joint->children = NULL;
    }
    
    /* Uninitializing all fields */
    if (joint->left)
	joint->left->right = NULL;
    
    if (joint->right)
	joint->right->left = NULL;
    
    joint->left = NULL;
    joint->right = NULL;
    joint->parent = NULL;
    
    reiser4_node_close(joint->node);
    aal_free(joint);
}

/* Helper for comparing during finding in the children list */
static inline int callback_comp_key(
    const void *item,		/* joint find will operate on */
    const void *key,		/* key to be find */
    void *data			/* user-specified data */
) {
    reiser4_key_t lkey;
    reiser4_joint_t *joint;

    joint = (reiser4_joint_t *)item;
    reiser4_node_lkey(joint->node, &lkey);
    
    return reiser4_key_compare(&lkey, (reiser4_key_t *)key) == 0;
}

/* Finds children by its left delimiting key */
reiser4_joint_t *reiser4_joint_find(
    reiser4_joint_t *joint,	/* joint to be greped */
    reiser4_key_t *key		/* left delimiting key */
) {
    aal_list_t *found;
    
    if (!joint->children)
	return NULL;
    
    /* Using aal_list find function */
    if (!(found = aal_list_find_custom(aal_list_first(joint->children), 
	    (void *)key, callback_comp_key, NULL)))
	return NULL;

    return (reiser4_joint_t *)found->data;
}

/* Returns left or right neighbor key for passed joint */
static errno_t reiser4_joint_neighbour_key(
    reiser4_joint_t *joint,	/* joint for working with */
    direction_t direction,	/* direction (left or right) */
    reiser4_key_t *key		/* key pointer result should be stored */
) {
    reiser4_pos_t pos;
    
    aal_assert("umka-770", joint != NULL, return -1);
    aal_assert("umka-771", key != NULL, return -1);
    
    if (reiser4_joint_pos(joint, &pos))
	return -1;
    
    /* Checking for position */
    if (direction == D_LEFT) {
	    
	if (pos.item == 0) 
	    return -1;
	
    } else {
	/* Checking and proceccing the special case called "shaft" */
	if (pos.item == reiser4_node_count(joint->parent->node) - 1) {

    	    if (!joint->parent->parent)
		return -1;
		
	    return reiser4_joint_neighbour_key(joint->parent->parent, 
		direction, key);
	}
    }
    
    pos.item += (direction == D_RIGHT ? 1 : -1);
    reiser4_node_get_key(joint->parent->node, &pos, key);
    
    return 0;
}

/* Returns position of passed joint in parent node */
errno_t reiser4_joint_pos(
    reiser4_joint_t *joint,	/* joint position will be obtained for */
    reiser4_pos_t *pos		/* pointer result will be stored in */
) {
    reiser4_key_t lkey;
    reiser4_key_t parent_key;
    
    aal_assert("umka-869", joint != NULL, return -1);
    aal_assert("umka-1266", pos != NULL, return -1);
    
    if (!joint->parent)
	return -1;

    reiser4_node_lkey(joint->node, &lkey);
    return -(reiser4_node_lookup(joint->parent->node, &lkey, pos) != 1);
}

/* 
    This function raises up both neighbours of the passed joint. This is used
    by shifting code in tree.c
*/
errno_t reiser4_joint_realize(
    reiser4_joint_t *joint	/* joint for working with */
) {
    uint32_t level;
    reiser4_key_t key;
    
    aal_assert("umka-776", joint != NULL, return -1);

    if (!joint->parent)
	return 0;
    
    /* 
	Initializing stop level for tree lookup function. Here tree lookup function is
	used as instrument for reflecting the part of tree into libreiser4 tree cache.
	So, connecting to the stop level for lookup we need to map the part of the tree
	from the root (tree height) to the level of passed node, because we should make
	sure, that needed neighbour will be mapped into cache and will be accesible by
	joint->left or joint->right pointers.
    */
    level = LEAF_LEVEL;
    
    /* Rasing the right neighbour */
    if (!joint->left) {
	if (!reiser4_joint_neighbour_key(joint, D_LEFT, &key)) {
	    if (reiser4_tree_lookup(joint->tree, &key, level, NULL) != 1) {
		aal_exception_error("Can't find left neighbour key when "
		    "raising left neigbour.");
		return -1;
	    }
	}
    }

    /* Raising the right neighbour */
    if (!joint->right) {
	if (!reiser4_joint_neighbour_key(joint, D_RIGHT, &key)) {
	    if (reiser4_tree_lookup(joint->tree, &key, level, NULL) != 1) {
		aal_exception_error("Can't find right neighbour key when "
		    "raising right neigbour.");
		return -1;
	    }
	}
    }
    
    return 0;
}

/* Helper function for registering in joint */
static int callback_comp_joint(
    const void *item1,		/* the first joint inetance for comparing */
    const void *item2,		/* the second one */
    void *data			/* user-specified data */
) {
    reiser4_key_t lkey1, lkey2;

    reiser4_joint_t *joint1 = (reiser4_joint_t *)item1;
    reiser4_joint_t *joint2 = (reiser4_joint_t *)item2;
    
    reiser4_node_lkey(joint1->node, &lkey1);
    reiser4_node_lkey(joint2->node, &lkey2);
    
    return reiser4_key_compare(&lkey1, &lkey2);
}

/*
    Connects children into sorted children list of specified node. Sets up both
    neighbours and parent pointer.
*/
errno_t reiser4_joint_attach(
    reiser4_joint_t *joint,	/* joint child will be connected to */
    reiser4_joint_t *child	/* child joint for registering */
) {
    aal_list_t *children;
    reiser4_key_t key, lkey;
    
    reiser4_joint_t *left;
    reiser4_joint_t *right;
    
    aal_assert("umka-561", joint != NULL, return -1);
    aal_assert("umka-564", child != NULL, return -1);
    
    /* Inserting passed joint into right position */
    children = joint->children ? 
	aal_list_first(joint->children) : NULL;

    joint->children = aal_list_insert_sorted(children, child, 
        callback_comp_joint, NULL);
    
    left = joint->children->prev ? 
	joint->children->prev->data : NULL;
    
    right = joint->children->next ? 
	joint->children->next->data : NULL;
   
    child->parent = joint;
    child->tree = joint->tree;
    
    /* Setting up neighbours */
    if (left) {
	
	reiser4_node_lkey(left->node, &lkey);
	    
	/* Getting left neighbour key */
	if (!reiser4_joint_neighbour_key(child, D_LEFT, &key))
	    child->left = (reiser4_key_compare(&key, &lkey) == 0 ? left : NULL);
    
	if (child->left)
	    child->left->right = child;
    }
   
    if (right) {
	
	reiser4_node_lkey(right->node, &lkey);
	
	/* Getting right neighbour key */
	if (!reiser4_joint_neighbour_key(child, D_RIGHT, &key))
	    child->right = (reiser4_key_compare(&key, &lkey) == 0 ? right : NULL);

	if (child->right)
	    child->right->left = child;
    }
    
    return 0;
}

/*
    Remove specified childern from the node. Updates all neighbour pointers and 
    parent pointer.
*/
void reiser4_joint_detach(
    reiser4_joint_t *joint,	/* joint child will be detached from */
    reiser4_joint_t *child	/* pointer to child to be deleted */
) {
    uint32_t count;
    aal_list_t *children;
    
    aal_assert("umka-562", joint != NULL, return);
    aal_assert("umka-563", child != NULL, return);

    if (!joint->children)
	return;
    
    children = aal_list_first(joint->children);
    
    /* Deleteing passed child from children list of specified joint */
    count = aal_list_length(children);
	
    if (child->left)
        child->left->right = NULL;
    
    if (child->right)
        child->right->left = NULL;
    
    child->left = NULL;
    child->right = NULL;
    child->parent = NULL;
    child->tree = NULL;
	
    aal_list_remove(children, child);
	
    if (count == 1)
        joint->children = NULL;
}

#ifndef ENABLE_COMPACT

/*
    Synchronizes passed joint by using resursive pass though all childrens. This
    method will be used when memory pressure occurs. There is possible to pass
    as parameter of this function the root joint pointer. In this case the whole
    tree will be flushed onto device, tree lies on.
*/
errno_t reiser4_joint_sync(
    reiser4_joint_t *joint	/* joint to be synchronized */
) {
    aal_list_t *children;
    
    aal_assert("umka-124", joint != NULL, return 0);
    
    children = joint->children ? aal_list_first(joint->children) : NULL;
    
    /*
	Walking through the list of childrens and calling reiser4_joint_sync
	function for each element.
    */
    if (children) {
	aal_list_t *walk;
	
	aal_list_foreach_forward(walk, children) {
	    if (reiser4_joint_sync((reiser4_joint_t *)walk->data))
		return -1;
	}
    }
    
/*#ifdef ENABLE_DEBUG
    {
	uint32_t used = aal_block_size(joint->node->block) - 
	    reiser4_node_space(joint->node);    
	
	uint32_t percents = (used * 100) / aal_block_size(joint->node->block);
	
	aal_exception_info("Node %llu packing factor: %u%%", 
	    aal_block_number(joint->node->block), percents);
    }
#endif*/
    
    /* Synchronizing joint itself */
    if (reiser4_node_sync(joint->node)) {
	aal_device_t *device = joint->node->block->device;

	aal_exception_error("Can't synchronize node %llu to device. %s.", 
	    aal_block_number(joint->node->block), device);

	return -1;
    }
    
    return 0;
}

errno_t reiser4_joint_update_key(reiser4_joint_t *joint, 
    reiser4_pos_t *pos, reiser4_key_t *key)
{
    reiser4_pos_t parent_pos;
    
    aal_assert("umka-999", joint != NULL, return -1);
    aal_assert("umka-1000", pos != NULL, return -1);
    aal_assert("umka-1001", key != NULL, return -1);
    
    aal_assert("umka-1002", 
	reiser4_node_count(joint->node) > 0, return -1);
    
    if (reiser4_node_set_key(joint->node, pos, key))
        return -1;
    
    if (pos->item == 0 && (pos->unit == ~0ul || pos->unit == 0)) {
	
	if (joint->parent) {
	    if (reiser4_joint_pos(joint, &parent_pos))
		return -1;
	    
	    if (reiser4_joint_update_key(joint->parent, &parent_pos, key))
		return -1;
	}
    }
    
    return 0;
}

/* 
    Inserts item or unit into cached node. Keeps track of changes of the left
    delimiting key.
*/
errno_t reiser4_joint_insert(
    reiser4_joint_t *joint,	    /* joint item will be inserted in */
    reiser4_pos_t *pos,	    	    /* pos item will be inserted at */
    reiser4_item_hint_t *hint	    /* item hint to be inserted */
) {
    reiser4_pos_t parent_pos;
    
    aal_assert("umka-990", joint != NULL, return -1);
    aal_assert("umka-991", pos != NULL, return -1);
    aal_assert("umka-992", hint != NULL, return -1);

    /* Saving the joint in parent */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (joint->parent) {
	    if (reiser4_joint_pos(joint, &parent_pos))
		return -1;
	}
    }
    
    /* Inserting item */
    if (reiser4_node_insert(joint->node, pos, hint))
	return -1;

    /* Updating ldkey in parent joint */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (joint->parent) {
	    if (reiser4_joint_update_key(joint->parent, &parent_pos, &hint->key))
		return -1;
	}
    }

    return 0;
}

/* 
    Deletes item or unit from cached node. Keeps track of changes of the left
    delimiting key.
*/
errno_t reiser4_joint_remove(
    reiser4_joint_t *joint,	    /* joint item will be inserted in */
    reiser4_pos_t *pos		    /* pos item will be inserted at */
) {
    reiser4_key_t key;
    reiser4_pos_t parent_pos;

    aal_assert("umka-993", joint != NULL, return -1);
    aal_assert("umka-994", pos != NULL, return -1);

    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (joint->parent) {
	    if (reiser4_joint_pos(joint, &parent_pos))
		return -1;
	}
    }
    
    /* 
	Updating list of childrens of modified node in the case we modifying an 
	internal node.
    */
    if (joint->children) {
	reiser4_joint_t *child;
	
	reiser4_node_get_key(joint->node, pos, &key);
	child = reiser4_joint_find(joint, &key);
        reiser4_joint_detach(joint, child);
    }

    /* Removing item or unit */
    if (reiser4_node_remove(joint->node, pos))
	return -1;
    
    /* Updating left deleimiting key in all parent nodes */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (reiser4_node_count(joint->node) > 0) {
	    if (joint->parent) {
		reiser4_key_t lkey;

		reiser4_node_lkey(joint->node, &lkey);
		if (reiser4_joint_update_key(joint->parent, &parent_pos, &lkey))
		    return -1;
	    }
	} else {
	    if (joint->parent) {
		/* 
		    Removing cached node from the tree in the case it has not items 
		    anymore.
		*/
		if (reiser4_joint_remove(joint->parent, &parent_pos))
		    return -1;
	    }
	}
    }

    return 0;
}

/* Moves item or unit from src cached node to dst one */
errno_t reiser4_joint_move(
    reiser4_joint_t *dst_joint,	    /* destination cached node */
    reiser4_pos_t *dst_pos,	    /* destination pos */
    reiser4_joint_t *src_joint,	    /* source cached node */
    reiser4_pos_t *src_pos	    /* source pos */
) {
    reiser4_key_t lkey;
    reiser4_pos_t dst_parent_pos;
    reiser4_pos_t src_parent_pos;
    
    aal_assert("umka-995", dst_joint != NULL, return -1);
    aal_assert("umka-996", dst_pos != NULL, return -1);
    aal_assert("umka-997", src_joint != NULL, return -1);
    aal_assert("umka-998", src_pos != NULL, return -1);
    
    /* Saving the position in teh parent joint */
    if (dst_pos->item == 0 && (dst_pos->unit == ~0ul || dst_pos->unit == 0)) {
	if (dst_joint->parent) {
	    if (reiser4_joint_pos(dst_joint, &dst_parent_pos))
		return -1;
	}
    }
    
    if (src_pos->item == 0 && (src_pos->unit == ~0ul || src_pos->unit == 0)) {
	if (src_joint->parent) {
	    if (reiser4_joint_pos(src_joint, &src_parent_pos))
		return -1;
	}
    }
    
    if (src_joint->children) {
        reiser4_key_t key;
        reiser4_joint_t *child;

        reiser4_node_get_key(src_joint->node, src_pos, &key);
	
        if ((child = reiser4_joint_find(src_joint, &key))) {
	    reiser4_joint_detach(src_joint, child);
	    reiser4_joint_attach(dst_joint, child);
	}
    }
    
    if (reiser4_node_count(src_joint->node) == 1 && src_joint->parent) {
	    
        if (reiser4_joint_remove(src_joint->parent, &src_parent_pos))
	   return -1;
    }
    
    /* Moving items */
    if (reiser4_node_move(dst_joint->node, dst_pos, src_joint->node, src_pos))
	return -1;
    
    /* Updating ldkey in parent node for dst node */
    if (dst_pos->item == 0 && (dst_pos->unit == ~0ul || dst_pos->unit == 0)) {
	    
	reiser4_node_lkey(dst_joint->node, &lkey);
	if (dst_joint->parent) {
	    if (reiser4_joint_update_key(dst_joint->parent, &dst_parent_pos, &lkey))
		return -1;
	}
    }

    /* Updating ldkey in parent node for src node */
    if (reiser4_node_count(src_joint->node) > 0) {
	if (src_pos->item == 0 && (src_pos->unit == ~0ul || src_pos->unit == 0)) {
	    if (src_joint->parent) {
		reiser4_node_lkey(src_joint->node, &lkey);
		if (reiser4_joint_update_key(src_joint->parent, &src_parent_pos, &lkey))
		    return -1;
	    }
	}
	
    }
    
    return 0;
}

#endif


