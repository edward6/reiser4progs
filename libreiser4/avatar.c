/*
    avatar.c -- the personalisation of the reiser4 on-disk node. The libreiser4
    internal in-memory tree consists of reiser4_avatar_t structures.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <reiser4/reiser4.h>

/* Creates avatar instance based on passed node */
reiser4_avatar_t *reiser4_avatar_create(
    reiser4_node_t *node	/* the first component of avatar */
) {
    reiser4_avatar_t *avatar;

    aal_assert("umka-1268", node != NULL, return NULL);
    
    /* Allocating memory for instance of avatar */
    if (!(avatar = aal_calloc(sizeof(*avatar), 0)))
	return NULL;

    avatar->node = node;
    return avatar;
}

/* Makes duplicate of the passed @avatar */
errno_t reiser4_avatar_dup(
    reiser4_avatar_t *avatar,	/* avatar to be duplicated */
    reiser4_avatar_t *dup	/* the clone will be saved */
) {
    aal_assert("umka-1264", avatar != NULL, return -1);
    aal_assert("umka-1265", dup != NULL, return -1);

    *dup = *avatar;
    return 0;
}

/* Freeing passed avatar */
void reiser4_avatar_close(
    reiser4_avatar_t *avatar	/* avatar to be freed */
) {
    aal_list_t *children;
    
    aal_assert("umka-793", avatar != NULL, return);
    
    children = avatar->children ? 
	aal_list_first(avatar->children) : NULL;

    if (children) {
	aal_list_t *walk;
	
	/* Recurcive calling the same function in order to free all children too */
	aal_list_foreach_forward(walk, children)
	    reiser4_avatar_close((reiser4_avatar_t *)walk->data);

	aal_list_free(children);
	avatar->children = NULL;
    }
    
    /* Uninitializing all fields */
    if (avatar->left)
	avatar->left->right = NULL;
    
    if (avatar->right)
	avatar->right->left = NULL;
    
    avatar->left = NULL;
    avatar->right = NULL;
    avatar->parent = NULL;
    
    reiser4_node_close(avatar->node);
    aal_free(avatar);
}

/* Helper for comparing during finding in the children list */
static inline int callback_comp_key(
    const void *item,		/* avatar find will operate on */
    const void *key,		/* key to be find */
    void *data			/* user-specified data */
) {
    reiser4_key_t lkey;
    reiser4_avatar_t *avatar;

    avatar = (reiser4_avatar_t *)item;
    reiser4_node_lkey(avatar->node, &lkey);
    
    return reiser4_key_compare(&lkey, (reiser4_key_t *)key) == 0;
}

/* Finds children by its left delimiting key */
reiser4_avatar_t *reiser4_avatar_find(
    reiser4_avatar_t *avatar,	/* avatar to be greped */
    reiser4_key_t *key		/* left delimiting key */
) {
    aal_list_t *found;
    
    if (!avatar->children)
	return NULL;
    
    /* Using aal_list find function */
    if (!(found = aal_list_find_custom(aal_list_first(avatar->children), 
	    (void *)key, callback_comp_key, NULL)))
	return NULL;

    return (reiser4_avatar_t *)found->data;
}

/* Returns left or right neighbor key for passed avatar */
static errno_t reiser4_avatar_neighbour_key(
    reiser4_avatar_t *avatar,	/* avatar for working with */
    direction_t direction,	/* direction (left or right) */
    reiser4_key_t *key		/* key pointer result should be stored */
) {
    reiser4_pos_t pos;
    
    aal_assert("umka-770", avatar != NULL, return -1);
    aal_assert("umka-771", key != NULL, return -1);
    
    if (reiser4_avatar_pos(avatar, &pos))
	return -1;
    
    /* Checking for position */
    if (direction == D_LEFT) {
	    
	if (pos.item == 0) 
	    return -1;
	
    } else {
	/* Checking and proceccing the special case called "shaft" */
	if (pos.item == reiser4_node_count(avatar->parent->node) - 1) {

    	    if (!avatar->parent->parent)
		return -1;
		
	    return reiser4_avatar_neighbour_key(avatar->parent->parent, 
		direction, key);
	}
    }
    
    pos.item += (direction == D_RIGHT ? 1 : -1);
    reiser4_node_get_key(avatar->parent->node, &pos, key);
    
    return 0;
}

/* Returns position of passed avatar in parent node */
errno_t reiser4_avatar_pos(
    reiser4_avatar_t *avatar,	/* avatar position will be obtained for */
    reiser4_pos_t *pos		/* pointer result will be stored in */
) {
    reiser4_key_t lkey;
    reiser4_key_t parent_key;
    
    aal_assert("umka-869", avatar != NULL, return -1);
    aal_assert("umka-1266", pos != NULL, return -1);
    
    if (!avatar->parent)
	return -1;

    reiser4_node_lkey(avatar->node, &lkey);
    return -(reiser4_node_lookup(avatar->parent->node, &lkey, pos) != 1);
}

/* 
    This function raises up both neighbours of the passed avatar. This is used
    by shifting code in tree.c
*/
errno_t reiser4_avatar_realize(
    reiser4_avatar_t *avatar	/* avatar for working with */
) {
    uint32_t level;
    reiser4_key_t key;
    
    aal_assert("umka-776", avatar != NULL, return -1);

    if (!avatar->parent)
	return 0;
    
    /* 
	Initializing stop level for tree lookup function. Here tree lookup function is
	used as instrument for reflecting the part of tree into libreiser4 tree cache.
	So, connecting to the stop level for lookup we need to map part of the tree
	from the root (tree height) to the level of passed node, because we should make
	sure, that needed neighbour will be mapped into cache and will be accesible by
	avatar->left or avatar->right pointers.
    */
    level = avatar->level;
    
    /* Rasing the right neighbour */
    if (!avatar->left) {
	if (!reiser4_avatar_neighbour_key(avatar, D_LEFT, &key)) {
	    if (reiser4_tree_lookup(avatar->tree, &key, level, NULL) != 1) {
		aal_exception_error("Can't find left neighbour key when "
		    "raising left neigbour.");
		return -1;
	    }
	}
    }

    /* Raising the right neighbour */
    if (!avatar->right) {
	if (!reiser4_avatar_neighbour_key(avatar, D_RIGHT, &key)) {
	    if (reiser4_tree_lookup(avatar->tree, &key, level, NULL) != 1) {
		aal_exception_error("Can't find right neighbour key when "
		    "raising right neigbour.");
		return -1;
	    }
	}
    }
    
    return 0;
}

/* Helper function for registering in avatar */
static int callback_comp_avatar(
    const void *item1,		/* the first avatar inetance for comparing */
    const void *item2,		/* the second one */
    void *data			/* user-specified data */
) {
    reiser4_key_t lkey1, lkey2;

    reiser4_avatar_t *avatar1 = (reiser4_avatar_t *)item1;
    reiser4_avatar_t *avatar2 = (reiser4_avatar_t *)item2;
    
    reiser4_node_lkey(avatar1->node, &lkey1);
    reiser4_node_lkey(avatar2->node, &lkey2);
    
    return reiser4_key_compare(&lkey1, &lkey2);
}

/*
    Connects children into sorted children list of specified node. Sets up both
    neighbours and parent pointer.
*/
errno_t reiser4_avatar_attach(
    reiser4_avatar_t *avatar,	/* avatar child will be connected to */
    reiser4_avatar_t *child	/* child avatar for registering */
) {
    aal_list_t *children;
    reiser4_key_t key, lkey;
    
    reiser4_avatar_t *left;
    reiser4_avatar_t *right;
    
    aal_assert("umka-561", avatar != NULL, return -1);
    aal_assert("umka-564", child != NULL, return -1);
    
    /* Inserting passed avatar into right position */
    children = avatar->children ? 
	aal_list_first(avatar->children) : NULL;

    avatar->children = aal_list_insert_sorted(children, child, 
        callback_comp_avatar, NULL);
    
    left = avatar->children->prev ? 
	avatar->children->prev->data : NULL;
    
    right = avatar->children->next ? 
	avatar->children->next->data : NULL;
   
    child->parent = avatar;
    child->tree = avatar->tree;
    
    /* Setting up neighbours */
    if (left) {
	
	reiser4_node_lkey(left->node, &lkey);
	    
	/* Getting left neighbour key */
	if (!reiser4_avatar_neighbour_key(child, D_LEFT, &key))
	    child->left = (reiser4_key_compare(&key, &lkey) == 0 ? left : NULL);
    
	if (child->left)
	    child->left->right = child;
    }
   
    if (right) {
	
	reiser4_node_lkey(right->node, &lkey);
	
	/* Getting right neighbour key */
	if (!reiser4_avatar_neighbour_key(child, D_RIGHT, &key))
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
void reiser4_avatar_detach(
    reiser4_avatar_t *avatar,	/* avatar child will be detached from */
    reiser4_avatar_t *child	/* pointer to child to be deleted */
) {
    uint32_t count;
    aal_list_t *children;
    
    aal_assert("umka-562", avatar != NULL, return);
    aal_assert("umka-563", child != NULL, return);

    if (!avatar->children)
	return;
    
    children = aal_list_first(avatar->children);
    
    /* Deleteing passed child from children list of specified avatar */
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
        avatar->children = NULL;
}

#ifndef ENABLE_COMPACT

/*
    Synchronizes passed avatar by using resursive pass though all childrens. This
    method will be used when memory pressure occurs. There is possible to pass
    as parameter of this function the root avatar pointer. In this case the whole
    tree will be flushed onto device, tree lies on.
*/
errno_t reiser4_avatar_sync(
    reiser4_avatar_t *avatar	/* avatar to be synchronized */
) {
    aal_list_t *children;
    
    aal_assert("umka-124", avatar != NULL, return 0);
    
    children = avatar->children ? aal_list_first(avatar->children) : NULL;
    
    /*
	Walking through the list of childrens and calling reiser4_avatar_sync
	function for each element.
    */
    if (children) {
	aal_list_t *walk;
	
	aal_list_foreach_forward(walk, children) {
	    if (reiser4_avatar_sync((reiser4_avatar_t *)walk->data))
		return -1;
	}
    }
    
/*#ifdef ENABLE_DEBUG
    {
	uint32_t used = aal_block_size(avatar->node->block) - 
	    reiser4_node_space(avatar->node);    
	
	uint32_t percents = (used * 100) / aal_block_size(avatar->node->block);
	
	aal_exception_info("Node %llu packing factor: %u%%", 
	    aal_block_number(avatar->node->block), percents);
    }
#endif*/
    
    /* Synchronizing avatar itself */
    if (reiser4_node_sync(avatar->node)) {
	aal_device_t *device = avatar->node->block->device;

	aal_exception_error("Can't synchronize node %llu to device. %s.", 
	    aal_block_number(avatar->node->block), device);

	return -1;
    }
    
    return 0;
}

errno_t reiser4_avatar_update_key(reiser4_avatar_t *avatar, 
    reiser4_pos_t *pos, reiser4_key_t *key)
{
    reiser4_pos_t parent_pos;
    
    aal_assert("umka-999", avatar != NULL, return -1);
    aal_assert("umka-1000", pos != NULL, return -1);
    aal_assert("umka-1001", key != NULL, return -1);
    
    aal_assert("umka-1002", 
	reiser4_node_count(avatar->node) > 0, return -1);
    
    if (reiser4_node_set_key(avatar->node, pos, key))
        return -1;
    
    if (pos->item == 0 && (pos->unit == ~0ul || pos->unit == 0)) {
	
	if (avatar->parent) {
	    if (reiser4_avatar_pos(avatar, &parent_pos))
		return -1;
	    
	    if (reiser4_avatar_update_key(avatar->parent, &parent_pos, key))
		return -1;
	}
    }
    
    return 0;
}

/* 
    Inserts item or unit into cached node. Keeps track of changes of the left
    delimiting key.
*/
errno_t reiser4_avatar_insert(
    reiser4_avatar_t *avatar,	    /* avatar item will be inserted in */
    reiser4_pos_t *pos,	    	    /* pos item will be inserted at */
    reiser4_item_hint_t *hint	    /* item hint to be inserted */
) {
    reiser4_pos_t parent_pos;
    
    aal_assert("umka-990", avatar != NULL, return -1);
    aal_assert("umka-991", pos != NULL, return -1);
    aal_assert("umka-992", hint != NULL, return -1);

    /* Saving the avatar in parent */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (avatar->parent) {
	    if (reiser4_avatar_pos(avatar, &parent_pos))
		return -1;
	}
    }
    
    /* Inserting item */
    if (reiser4_node_insert(avatar->node, pos, hint))
	return -1;

    /* Updating ldkey in parent avatar */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (avatar->parent) {
	    if (reiser4_avatar_update_key(avatar->parent, &parent_pos, &hint->key))
		return -1;
	}
    }

    return 0;
}

/* 
    Deletes item or unit from cached node. Keeps track of changes of the left
    delimiting key.
*/
errno_t reiser4_avatar_remove(
    reiser4_avatar_t *avatar,	    /* avatar item will be inserted in */
    reiser4_pos_t *pos		    /* pos item will be inserted at */
) {
    reiser4_key_t key;
    reiser4_pos_t parent_pos;

    aal_assert("umka-993", avatar != NULL, return -1);
    aal_assert("umka-994", pos != NULL, return -1);

    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (avatar->parent) {
	    if (reiser4_avatar_pos(avatar, &parent_pos))
		return -1;
	}
    }
    
    /* 
	Updating list of childrens of modified node in the case we modifying an 
	internal node.
    */
    if (avatar->children) {
	reiser4_avatar_t *child;
	
	reiser4_node_get_key(avatar->node, pos, &key);
	child = reiser4_avatar_find(avatar, &key);
        reiser4_avatar_detach(avatar, child);
    }

    /* Removing item or unit */
    if (reiser4_node_remove(avatar->node, pos))
	return -1;
    
    /* Updating left deleimiting key in all parent nodes */
    if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
	if (reiser4_node_count(avatar->node) > 0) {
	    if (avatar->parent) {
		reiser4_key_t lkey;

		reiser4_node_lkey(avatar->node, &lkey);
		if (reiser4_avatar_update_key(avatar->parent, &parent_pos, &lkey))
		    return -1;
	    }
	} else {
	    if (avatar->parent) {
		/* 
		    Removing cached node from the tree in the case it has not items 
		    anymore.
		*/
		if (reiser4_avatar_remove(avatar->parent, &parent_pos))
		    return -1;
	    }
	}
    }

    return 0;
}

/* Moves item or unit from src cached node to dst one */
errno_t reiser4_avatar_move(
    reiser4_avatar_t *dst_avatar,	    /* destination cached node */
    reiser4_pos_t *dst_pos,	    /* destination pos */
    reiser4_avatar_t *src_avatar,	    /* source cached node */
    reiser4_pos_t *src_pos	    /* source pos */
) {
    reiser4_key_t lkey;
    reiser4_pos_t dst_parent_pos;
    reiser4_pos_t src_parent_pos;
    
    aal_assert("umka-995", dst_avatar != NULL, return -1);
    aal_assert("umka-996", dst_pos != NULL, return -1);
    aal_assert("umka-997", src_avatar != NULL, return -1);
    aal_assert("umka-998", src_pos != NULL, return -1);
    
    /* Saving the position in teh parent avatar */
    if (dst_pos->item == 0 && (dst_pos->unit == ~0ul || dst_pos->unit == 0)) {
	if (dst_avatar->parent) {
	    if (reiser4_avatar_pos(dst_avatar, &dst_parent_pos))
		return -1;
	}
    }
    
    if (src_pos->item == 0 && (src_pos->unit == ~0ul || src_pos->unit == 0)) {
	if (src_avatar->parent) {
	    if (reiser4_avatar_pos(src_avatar, &src_parent_pos))
		return -1;
	}
    }
    
    if (src_avatar->children) {
        reiser4_key_t key;
        reiser4_avatar_t *child;

        reiser4_node_get_key(src_avatar->node, src_pos, &key);
	
        if ((child = reiser4_avatar_find(src_avatar, &key))) {
	    reiser4_avatar_detach(src_avatar, child);
	    reiser4_avatar_attach(dst_avatar, child);
	}
    }
    
    if (reiser4_node_count(src_avatar->node) == 1 && src_avatar->parent) {
	    
        if (reiser4_avatar_remove(src_avatar->parent, &src_parent_pos))
	   return -1;
    }
    
    /* Moving items */
    if (reiser4_node_move(dst_avatar->node, dst_pos, src_avatar->node, src_pos))
	return -1;
    
    /* Updating ldkey in parent node for dst node */
    if (dst_pos->item == 0 && (dst_pos->unit == ~0ul || dst_pos->unit == 0)) {
	    
	reiser4_node_lkey(dst_avatar->node, &lkey);
	if (dst_avatar->parent) {
	    if (reiser4_avatar_update_key(dst_avatar->parent, &dst_parent_pos, &lkey))
		return -1;
	}
    }

    /* Updating ldkey in parent node for src node */
    if (reiser4_node_count(src_avatar->node) > 0) {
	if (src_pos->item == 0 && (src_pos->unit == ~0ul || src_pos->unit == 0)) {
	    if (src_avatar->parent) {
		reiser4_node_lkey(src_avatar->node, &lkey);
		if (reiser4_avatar_update_key(src_avatar->parent, &src_parent_pos, &lkey))
		    return -1;
	    }
	}
	
    }
    
    return 0;
}

#endif


