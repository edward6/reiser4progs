/*
  joint.c -- the personalization of the reiser4 on-disk node. The libreiser4
  internal in-memory tree consists of reiser4_joint_t structures.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

/* Creates joint instance based on passed node */
reiser4_joint_t *reiser4_joint_create(
	reiser4_node_t *node)	/* the first component of joint */
{
	reiser4_joint_t *joint;

	aal_assert("umka-1268", node != NULL, return NULL);
    
	/* Allocating memory for instance of joint */
	if (!(joint = aal_calloc(sizeof(*joint), 0)))
		return NULL;

	joint->node = node;
	return joint;
}

errno_t reiser4_joint_lock(reiser4_joint_t *joint) {
	errno_t res = 0;
	
	aal_assert("umka-1515", joint != NULL, return -1);

	if (joint->parent)
		res = reiser4_joint_lock(joint->parent);
	
	joint->counter++;
	return res;
}

errno_t reiser4_joint_unlock(reiser4_joint_t *joint) {
	errno_t res = 0;
	
	aal_assert("umka-1516", joint != NULL, return -1);
	aal_assert("umka-1517", joint->counter > 0, return -1);

	if (joint->parent)
		res = reiser4_joint_unlock(joint->parent);
	
	joint->counter--;
	return res;
}

/* Makes duplicate of the passed @joint */
errno_t reiser4_joint_dup(
	reiser4_joint_t *joint,	/* joint to be duplicated */
	reiser4_joint_t *dup)	/* the clone will be saved */
{
	aal_assert("umka-1264", joint != NULL, return -1);
	aal_assert("umka-1265", dup != NULL, return -1);

	*dup = *joint;
	return 0;
}

/* Freeing passed joint */
void reiser4_joint_close(
	reiser4_joint_t *joint)	/* joint to be freed */
{
	aal_assert("umka-793", joint != NULL, return);

	if (joint->flags & JF_DIRTY) {
		aal_exception_warn("Destroing dirty joint. Block %llu.",
				   joint->node->blk);
	}

	if (joint->counter) {
		aal_exception_warn("Destroing locked (%d) joint. Block %llu.",
				   joint->counter, joint->node->blk);
	}
	
	if (joint->children) {
		aal_list_t *walk;

		for (walk = joint->children; walk; ) {
			aal_list_t *temp = aal_list_next(walk);
			reiser4_joint_close((reiser4_joint_t *)walk->data);
			walk = temp;
		}

		aal_list_free(joint->children);
		joint->children = NULL;
	}
 
	if (joint->parent)
		reiser4_joint_detach(joint->parent, joint);
	
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

/* Returns left or right neighbor key for passed joint */
static errno_t reiser4_joint_neighbour_key(
	reiser4_joint_t *joint,	        /* joint for working with */
	direction_t direction,	        /* direction (left or right) */
	reiser4_key_t *key)		/* key pointer result should be stored */
{
	reiser4_pos_t pos;
	reiser4_coord_t coord;
    
	aal_assert("umka-770", joint != NULL, return -1);
	aal_assert("umka-771", key != NULL, return -1);
    
	if (reiser4_joint_pos(joint, &pos))
		return -1;
    
	/* Checking for position */
	if (direction == D_LEFT) {
	    
		if (pos.item == 0) 
			return -1;
	
	} else {
		reiser4_joint_t *parent = joint->parent;
	
		/* Checking and proceccing the special case called "shaft" */
		if (pos.item == reiser4_node_count(parent->node) - 1) {

			if (!parent->parent)
				return -1;
		
			return reiser4_joint_neighbour_key(parent->parent, 
							   direction, key);
		}
	}
    
	pos.item += (direction == D_RIGHT ? 1 : -1);

	if (reiser4_coord_open(&coord, joint->parent, CT_JOINT, &pos))
		return -1;
	
	return reiser4_item_key(&coord, key);
}

/* Returns position of passed joint in parent node */
errno_t reiser4_joint_pos(
	reiser4_joint_t *joint,	        /* joint position will be obtained for */
	reiser4_pos_t *pos)		/* pointer result will be stored in */
{
	reiser4_key_t lkey;
	reiser4_key_t parent_key;
    
	aal_assert("umka-869", joint != NULL, return -1);
	aal_assert("umka-1266", pos != NULL, return -1);
    
	if (!joint->parent)
		return -1;

	reiser4_node_lkey(joint->node, &lkey);
    
	if (reiser4_node_lookup(joint->parent->node, &lkey, pos) != 1)
		return -1;

	joint->pos = *pos;
    
	return 0;
}

/* 
   This function raises up both neighbours of the passed joint. This is used
   by shifting code in tree.c
*/
reiser4_joint_t *reiser4_joint_left(
	reiser4_joint_t *joint)	/* joint for working with */
{
	reiser4_key_t key;

	/* 
	   Initializing stop level for tree lookup function. Here tree lookup
	   function is used as instrument for reflecting the part of tree into
	   libreiser4 tree cache.  So, connecting to the stop level for lookup
	   we need to map the part of the tree from the root (tree height) to
	   the level of passed node, because we should make sure, that needed
	   neighbour will be mapped into cache and will be accesible by
	   joint->left or joint->right pointers.
	*/
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-776", joint != NULL, return NULL);

	/* Parent is not present. The root node. */
	if (!joint->parent)
		return NULL;
    
	/*
	  Attaching left neighbour into the tree. Here we take its key and
	  perform lookup. We use lookup because it attaches all nodes belong to
	  search path and settups them neighbours pointers.
	*/
	if (!joint->left) {
		if (reiser4_joint_neighbour_key(joint, D_LEFT, &key) == 0)
			reiser4_tree_lookup(joint->tree, &key, &level, NULL);
	}

	return joint->left;
}

reiser4_joint_t *reiser4_joint_right(reiser4_joint_t *joint) {
	reiser4_key_t key;

	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-1510", joint != NULL, return NULL);

	if (!joint->parent)
		return NULL;
    
	if (!joint->right) {
		if (reiser4_joint_neighbour_key(joint, D_RIGHT, &key) == 0)
			reiser4_tree_lookup(joint->tree, &key, &level, NULL);
	}
    
	return joint->right;
}

/* Helper function for registering in joint */
static int callback_comp_joint(
	const void *item1,		/* the first joint inetance for comparing */
	const void *item2,		/* the second one */
	void *data)		        /* user-specified data */
{
	reiser4_key_t lkey1, lkey2;

	reiser4_joint_t *joint1 = (reiser4_joint_t *)item1;
	reiser4_joint_t *joint2 = (reiser4_joint_t *)item2;
    
	reiser4_node_lkey(joint1->node, &lkey1);
	reiser4_node_lkey(joint2->node, &lkey2);
    
	return reiser4_key_compare(&lkey1, &lkey2);
}

/* Helper for comparing during finding in the children list */
static inline int callback_comp_key(
	const void *item,		/* joint find will operate on */
	const void *key,		/* key to be find */
	void *data)			/* user-specified data */
{
	reiser4_key_t lkey;
	reiser4_joint_t *joint;

	joint = (reiser4_joint_t *)item;
	reiser4_node_lkey(joint->node, &lkey);
    
	return reiser4_key_compare(&lkey, (reiser4_key_t *)key) == 0;
}

/* Finds children by its left delimiting key */
reiser4_joint_t *reiser4_joint_find(
	reiser4_joint_t *joint,	        /* joint to be greped */
	reiser4_key_t *key)		/* left delimiting key */
{
	aal_list_t *list;
	reiser4_joint_t *child;
    
	if (!joint->children)
		return NULL;
    
	/* Using aal_list find function */
	if (!(list = aal_list_find_custom(joint->children, (void *)key,
					   callback_comp_key, NULL)))
		return NULL;

	child = (reiser4_joint_t *)list->data;

	if (joint->tree && reiser4_lru_touch(&joint->tree->lru, child))
		aal_exception_warn("Can't update tree lru.");

	return child;
}

/*
  Connects children into sorted children list of specified node. Sets up both
  neighbours and parent pointer.
*/
errno_t reiser4_joint_attach(
	reiser4_joint_t *joint,	       /* joint child will be connected to */
	reiser4_joint_t *child)	       /* child joint for registering */
{
	reiser4_joint_t *left;
	reiser4_joint_t *right;
	reiser4_key_t key, lkey;
    
	aal_assert("umka-561", joint != NULL, return -1);
	aal_assert("umka-564", child != NULL, return -1);

	joint->children = aal_list_insert_sorted(joint->children, child,
						 callback_comp_joint, NULL);
    
	left = joint->children->prev ? 
		joint->children->prev->data : NULL;
    
	right = joint->children->next ? 
		joint->children->next->data : NULL;
    
	child->parent = joint;
    
	if (reiser4_joint_pos(child, &child->pos))
		return -1;
    
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

	joint->children = aal_list_first(joint->children);

	if (joint->tree && reiser4_lru_attach(&joint->tree->lru, child))
		aal_exception_warn("Can't attach node to tree lru.");
	
	return 0;
}

/*
  Remove specified childern from the node. Updates all neighbour pointers and
  parent pointer.
*/
void reiser4_joint_detach(
	reiser4_joint_t *joint,	/* joint child will be detached from */
	reiser4_joint_t *child)	/* pointer to child to be deleted */
{
	aal_assert("umka-562", joint != NULL, return);
	aal_assert("umka-563", child != NULL, return);

	if (!joint->children)
		return;
    
	if (child->left) {
		child->left->right = NULL;
		child->left = NULL;
	}
	
	if (child->right) {
		child->right->left = NULL;
		child->right = NULL;
	}
	
	child->tree = NULL;
	child->parent = NULL;
    
	/* Updating joint children list */
	joint->children = aal_list_remove(joint->children, child);

	if (joint->tree && reiser4_lru_detach(&joint->tree->lru, child))
		aal_exception_warn("Can't detach node from tree lru.");
}

#ifndef ENABLE_COMPACT

/*
  Synchronizes passed @joint by means of using resursive pass though all
  children. There is possible to pass as parameter of this function the root
  joint pointer. In this case the whole tree will be flushed onto device, tree
  lies on.
*/
errno_t reiser4_joint_sync(
	reiser4_joint_t *joint)	/* joint to be synchronized */
{
	aal_assert("umka-124", joint != NULL, return 0);
    
	/*
	  Walking through the list of children and calling reiser4_joint_sync
	  function for each element.
	*/
	if (joint->children) {
		aal_list_t *walk;
	
		aal_list_foreach_forward(walk, joint->children) {
			if (reiser4_joint_sync((reiser4_joint_t *)walk->data))
				return -1;
		}
	}

	/* Synchronizing passed @joint */
	if (joint->flags & JF_DIRTY) {
		
		if (reiser4_node_sync(joint->node)) {
			aal_device_t *device = joint->node->device;

			aal_exception_error("Can't synchronize node %llu to device. %s.", 
					    joint->node->blk, device->error);

			return -1;
		}

		joint->flags &= ~JF_DIRTY;
	}
    
	return 0;
}

errno_t reiser4_joint_update(reiser4_joint_t *joint, reiser4_pos_t *pos,
			     reiser4_key_t *key)
{
	reiser4_coord_t coord;
	reiser4_pos_t parent_pos;
    
	aal_assert("umka-999", joint != NULL, return -1);
	aal_assert("umka-1000", pos != NULL, return -1);
	aal_assert("umka-1001", key != NULL, return -1);
    
	aal_assert("umka-1002", 
		   reiser4_node_count(joint->node) > 0, return -1);

	if (reiser4_coord_open(&coord, joint, CT_JOINT, pos))
		return -1;

	if (reiser4_item_update(&coord, key))
		return -1;
    
	if (pos->item == 0 && (pos->unit == ~0ul || pos->unit == 0)) {
	
		if (joint->parent) {
			if (reiser4_joint_pos(joint, &parent_pos))
				return -1;
	    
			if (reiser4_joint_update(joint->parent, &parent_pos, key))
				return -1;
		}
	}
    
	joint->flags |= JF_DIRTY;
	
	return 0;
}

/* 
   Inserts item or unit into cached node. Keeps track of changes of the left
   delimiting key.
*/
errno_t reiser4_joint_insert(
	reiser4_joint_t *joint,	            /* joint item will be inserted in */
	reiser4_pos_t *pos,	    	    /* pos item will be inserted at */
	reiser4_item_hint_t *hint)	    /* item hint to be inserted */
{
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
		reiser4_joint_t *parent = joint->parent;
	
		if (parent) {
			if (reiser4_joint_update(parent, &parent_pos, &hint->key))
				return -1;
		}
	}

	joint->flags |= JF_DIRTY;
	
	return 0;
}

/* 
   Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key.
*/
errno_t reiser4_joint_remove(
	reiser4_joint_t *joint,	            /* joint item will be inserted in */
	reiser4_pos_t *pos)		    /* pos item will be inserted at */
{
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
		reiser4_coord_t coord;
		reiser4_joint_t *child;

		if (reiser4_coord_open(&coord, joint, CT_JOINT, pos))
			return -1;

		if (reiser4_item_key(&coord, &key))
			return -1;
		
		child = reiser4_joint_find(joint, &key);
		reiser4_joint_detach(joint, child);
	}

	/* Removing item or unit */
	if (reiser4_node_remove(joint->node, pos))
		return -1;
    
	/* Updating left deleimiting key in all parent nodes */
	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		reiser4_joint_t *parent = joint->parent;
	
		if (parent) {
			if (reiser4_node_count(joint->node) > 0) {
				reiser4_key_t lkey;

				reiser4_node_lkey(joint->node, &lkey);
				if (reiser4_joint_update(parent, &parent_pos, &lkey))
					return -1;
			} else {
				/* 
				   Removing cached node from the tree in the case it has not items 
				   anymore.
				*/
				if (reiser4_joint_remove(parent, &parent_pos))
					return -1;
			}
		}
	}

	joint->flags |= JF_DIRTY;
	
	return 0;
}

/* Moves item or unit from src cached node to dst one */
errno_t reiser4_joint_move(
	reiser4_joint_t *dst_joint,     /* destination cached node */
	reiser4_pos_t *dst_pos,	        /* destination pos */
	reiser4_joint_t *src_joint,	/* source cached node */
	reiser4_pos_t *src_pos)	        /* source pos */
{
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
		reiser4_coord_t coord;
		reiser4_joint_t *child;

		if (reiser4_coord_open(&coord, src_joint, CT_JOINT, src_pos))
			return -1;
		
		if (reiser4_item_key(&coord, &key))
			return -1;
	
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
		reiser4_joint_t *parent = dst_joint->parent;
		reiser4_node_lkey(dst_joint->node, &lkey);
	
		if (parent) {
			if (reiser4_joint_update(parent, &dst_parent_pos, &lkey))
				return -1;
		}
	}

	/* Updating ldkey in parent node for src node */
	if (reiser4_node_count(src_joint->node) > 0) {
		if (src_pos->item == 0 && (src_pos->unit == ~0ul || src_pos->unit == 0)) {

			reiser4_joint_t *parent = src_joint->parent;
	    
			if (parent) {
				reiser4_node_lkey(src_joint->node, &lkey);
				if (reiser4_joint_update(parent, &src_parent_pos, &lkey))
					return -1;
			}
		}
	}
    
	src_joint->flags |= JF_DIRTY;
	dst_joint->flags |= JF_DIRTY;
	
	return 0;
}

static int traverse_continue(rpid_t objects, rpid_t type) {
	return (objects & (1 << type));
}

/* This function traverse passed node. */
errno_t reiser4_joint_traverse(
	reiser4_joint_t *joint,		     /* block which should be traversed */
	traverse_hint_t *hint,		     /* hint for traverse and for callback methods */
	reiser4_open_func_t open_func,	     /* callback for node opening */
	reiser4_edge_func_t before_func,     /* callback to be called at the beginning */
	reiser4_setup_func_t setup_func,     /* callback to be called before a child  */
	reiser4_setup_func_t update_func,    /* callback to be called after a child */
	reiser4_edge_func_t after_func)      /* callback to be called at the end */
{
	errno_t result = 0;
	reiser4_coord_t coord;
	object_entity_t *entity;
	reiser4_pos_t *pos = &coord.pos;
	
	reiser4_joint_t *child = NULL;
    
	aal_assert("vpf-418", hint != NULL, return -1);
	aal_assert("vpf-390", joint!= NULL, return -1);
	aal_assert("vpf-391", joint->node != NULL, return -1);

	entity = joint->node->entity;
	
	joint->counter++;

	if ((before_func && (result = before_func(joint, hint->data))))
		goto error;

	for (pos->item = 0; pos->item < reiser4_node_count(joint->node); pos->item++) {
		pos->unit = ~0ul; 

		/*
		  If there is a suspicion in a corruption, it must be checked in
		  before_func. All items must be opened here.
		*/
		if (reiser4_coord_open(&coord, joint, CT_JOINT, pos)) {
			blk_t blk = joint->node->blk;
			aal_exception_error("Can't open item by coord. Node %llu, item %u.",
					    blk, pos->item);
			goto error_after_func;
		}
		
		if (!traverse_continue(hint->objects, reiser4_item_type(&coord)))
			continue;
	    
		for (pos->unit = 0; pos->unit < reiser4_item_count(&coord); pos->unit++) {
			reiser4_ptr_hint_t ptr;
		
			if (plugin_call(continue, coord.entity.plugin->item_ops,
					fetch, &coord.entity, pos->unit, &ptr, 1))
				goto error_after_func;
		
			if (ptr.ptr != FAKE_BLK && ptr.ptr != 0) {
				child = NULL;
					
				if (setup_func && (result = setup_func(&coord, hint->data)))
					goto error_after_func;

				if (!open_func)
					goto update;

				if (!(child = reiser4_joint_find(joint, &coord.entity.key))) {
						
					if ((result = open_func(&child, ptr.ptr, hint->data)))
						goto error_update_func;

					if (!child)
						goto update;

//					child->data = (void *)1;
					
					if (reiser4_joint_attach(joint, child))
						goto error_free_child;
				}

				if ((result = reiser4_joint_traverse(child, hint, 
								     open_func,
								     before_func, 
								     setup_func,
								     update_func,
								     after_func)) < 0)
					goto error_free_child;

				if (hint->cleanup && !child->children && !child->counter)
					reiser4_joint_close(child);
					
			update:
				if (update_func && (result = update_func(&coord, hint->data)))
					goto error_after_func;
			}
				
			/* We want to get out of the internal loop or the item was removed. */
			if (pos->unit == ~0ul)
				break;				
		}
	}
	
	if (after_func && (result = after_func(joint, hint->data)))
		goto error;

	joint->counter--;
	return result;

 error_free_child:
	
	if (hint->cleanup && !child->children && !child->counter)
		reiser4_joint_close(child);

 error_update_func:
	
	if (update_func)
		result = update_func(&coord, hint->data);
    
 error_after_func:
	if (after_func)
		result = after_func(joint, hint->data);
    
 error:
	joint->counter--;
	return result;
}

#endif
