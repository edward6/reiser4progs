/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/filter.c - Filter pass of file system recovery.
   
   The filter pass filters corrupted parts of a reiser4 tree out, repairs 
   all recoverable corruptions and builds a map of all used blocks, but 
   extents format + formatted nodes). Extents are left not checked as there
   is no enough information for their proper check. */

#include <repair/filter.h>

/* This is extension for repair_error_t. */
typedef enum repair_error_filter {
	/* Pointer to the wrong place. */
	RE_PTR         = (RE_LAST),

	/* Node is ok, but dkeys are wrong, remove from the tree and insert
	   back later item-by-item. */
	RE_DKEYS	= (RE_LAST << 1),

	/* Node was emptied. */
	RE_EMPTY	= (RE_LAST << 2)
} repair_error_filter_t;

typedef enum repair_mark {
	RM_BAD   = 0,
	RM_MARK  = 1,
	RM_CLEAR = 2
} repair_mark_t;

void cb_gauge_tree_percent(aal_gauge_t *gauge) {
	uint32_t pos[REISER4_TREE_MAX_HEIGHT][2];
	uint64_t amount, passed;
	reiser4_place_t *place;
	uint8_t i;
	
	if (!gauge || !gauge->data)
		return;
	
	place = &((reiser4_node_t *)gauge->data)->p;
	
	i = 0;
	while (place->node) {
		pos[i][0] = place->pos.item;
		pos[i][1] = reiser4_node_items(place->node);
		place = &place->node->p;
		i++;
	}
	
	passed = amount = 0;
	
	while (i) {
		i--;
		amount = amount ? amount * pos[i][1] : pos[i][1];
		
		if (passed) passed *= pos[i][1];
		passed += pos[i][0];
	}

	aal_gauge_set_value(gauge, amount ? passed * 100 / amount : 0);
}

static void repair_filter_node_handle(repair_filter_t *fd, blk_t blk, 
				      uint8_t level, repair_mark_t mark)
{
	if (mark == RM_MARK) {
		aux_bitmap_mark_region(fd->bm_used, blk, 1);
		fd->stat.good_nodes++;
	} else {
		aux_bitmap_clear_region(fd->bm_used, blk, 1);
		fd->stat.good_nodes--;
	}

	switch (level) {
	case LEAF_LEVEL:
		if (mark == RM_MARK) {
			if (fd->bm_leaf)
				aux_bitmap_mark_region(fd->bm_leaf, blk, 1);
			fd->stat.good_leaves++;
		} else {
			if (fd->bm_leaf)
				aux_bitmap_clear_region(fd->bm_leaf, blk, 1);
			fd->stat.good_leaves--;
			if (mark == RM_BAD)
				fd->stat.bad_leaves++;
		}

		break;
	case TWIG_LEVEL:
		if (mark == RM_MARK) {
			if (fd->bm_twig)
				aux_bitmap_mark_region(fd->bm_twig, blk, 1);
			fd->stat.good_twigs++;
		} else {
			if (fd->bm_twig)
				aux_bitmap_clear_region(fd->bm_twig, blk, 1);
			fd->stat.good_twigs--;
			if (mark == RM_BAD)
				fd->stat.bad_twigs++;
		}
		
		break;
	default:
		break;
	}
	
	if (fd->bm_met) {
		if (mark == RM_MARK)
			aux_bitmap_mark_region(fd->bm_met, blk, 1);
		else if (mark == RM_CLEAR)
			aux_bitmap_clear_region(fd->bm_met, blk, 1);
	}

	return;
}

static void repair_filter_read_node(repair_filter_t *fd, blk_t blk,
				    uint8_t level) 
{
	fd->stat.read_nodes++;
	repair_filter_node_handle(fd, blk, level, RM_MARK);
}


static void repair_filter_bad_node(repair_filter_t *fd, blk_t blk,
				   uint8_t level)
{
	fd->flags |= RE_FATAL;
	fd->stat.bad_nodes++;
	repair_filter_node_handle(fd, blk, level, RM_BAD);
}

static void repair_filter_empty_node(repair_filter_t *fd, blk_t blk,
				     uint8_t level)
{
	fd->flags |= RE_EMPTY;
	fd->repair->fatal++;
	repair_filter_node_handle(fd, blk, level, RM_BAD);
}

static void repair_filter_bad_dk(repair_filter_t *fd, blk_t blk,
				 uint8_t level)
{
	fd->flags |= RE_DKEYS;
	repair_filter_node_handle(fd, blk, level, RM_CLEAR);
	fd->stat.bad_dk_nodes++;
	fd->repair->fatal++;

	switch (level) {
	case LEAF_LEVEL:
		fd->stat.bad_dk_leaves++;
		return;
	case TWIG_LEVEL:
		fd->stat.bad_dk_twigs++;
		return;
	default:
		return;
	}
	
}

static void repair_filter_fixed_node(repair_filter_t *fd, uint8_t level) {
	fd->stat.fixed_nodes++;
	
	switch (level) {
	case LEAF_LEVEL:
		fd->stat.fixed_leaves++;
		return;
	case TWIG_LEVEL:
		fd->stat.fixed_twigs++;
		return;
	default:
		return;
	}
}

static void repair_filter_bad_ptr(repair_filter_t *fd) {
	fd->flags |= RE_PTR;
	fd->repair->fatal++;
	fd->stat.bad_ptrs++;
}

static void repair_filter_bad_level(repair_filter_t *fd, 
				    blk_t blk, 
				    uint8_t level) 
{
	repair_filter_node_handle(fd, blk, level, RM_CLEAR);
	repair_filter_bad_ptr(fd);
}

/* Open callback for traverse. It opens a node at passed blk. It does 
   nothing if RE_PTR is set and set this flag if node cannot 
   be opeened. Returns error if any. */
static reiser4_node_t *repair_filter_node_open(reiser4_tree_t *tree,
					       reiser4_place_t *place,
					       void *data)
{
	repair_filter_t *fd = (repair_filter_t *)data;
	reiser4_node_t *node = NULL;
	int error = 0;
	blk_t blk;
	
	aal_assert("vpf-379", fd != NULL);
	aal_assert("vpf-433", fd->repair != NULL);
	aal_assert("vpf-842", fd->repair->fs != NULL);
	aal_assert("vpf-591", fd->repair->fs->format != NULL);
	aal_assert("vpf-1118", tree != NULL);
	aal_assert("vpf-1187", place != NULL);

	if ((blk = reiser4_item_down_link(place)) == INVAL_BLK)
		return INVAL_PTR;
	
	if (blk >= fd->bm_used->total) {
		fsck_mess("Node (%llu), item (%u), unit (%u): Points to the "
			  "invalid block (%llu).%s", place_blknr(place),
			  place->pos.item, place->pos.unit, blk, 
			  fd->repair->mode == RM_BUILD ? " Removed." :
			  " The whole subtree is skipped.");
		error = 1;
	}
	
	if (aux_bitmap_test_region(fd->bm_used, blk, 1, 1)) {
		/* Bad pointer detected. Remove if possible. */
		fsck_mess("Node (%llu), item (%u), unit (%u): Points to the "
			  "block (%llu) which is in the tree already.%s", 
			  place_blknr(place), place->pos.item, 
			  place->pos.unit, blk, fd->repair->mode == RM_BUILD ?
			  " Removed." : " The whole subtree is skipped.");
		error = 1;
	}
	
	if (error) goto error;
	
	if (!(node = repair_tree_load_node(fd->repair->fs->tree, place->node, 
					   blk, *fd->check_node))) 
	{
		fsck_mess("Node (%llu): failed to open the node pointed by the "
			  "node (%llu), item (%u), unit (%u) on the level (%u)."
			  " The whole subtree is skipped.", blk, 
			  place_blknr(place), place->pos.item, 
			  place->pos.unit, reiser4_node_get_level(place->node));
		goto error;
	}
	
	repair_filter_read_node(fd, blk, reiser4_node_get_level(node));
	return node;
	
 error:
	repair_filter_bad_ptr(fd);
	return NULL;
}

static errno_t cb_count_sd(reiser4_place_t *place, void *data) {
	repair_filter_t *fd = (repair_filter_t *)data;

	if (place->plug->id.group == STAT_ITEM)
		fd->stat.tmp++;

	return 0;
}

/* Before callback for traverse. It checks node level, node consistency, and 
   delimiting keys. If any check reveals a problem with the data consistency 
   it sets RE_FATAL flag. */
static errno_t repair_filter_node_check(reiser4_node_t *node, void *data) {
	repair_filter_t *fd = (repair_filter_t *)data;
	errno_t res = 0;
	uint32_t items;
	uint16_t level;
    
	aal_assert("vpf-252", data  != NULL);
	aal_assert("vpf-409", node != NULL);
    
	level = reiser4_node_get_level(node); 
    
	/* Initialize the level for the root node before traverse.
	   For the root node @fd->level is not set, set it to the
	   current node level. */
	if (!fd->level)
		fd->level = level;
	else
		fd->level--;

	/* Check the level. */
	if (fd->level != level) {
		fsck_mess("Level (%u) of the node (%llu) doesn't match the "
			  "expected one (%u). %s", level, node->block->nr, 
			  fd->level, fd->repair->mode == RM_BUILD ? 
			  "Removed." : "The whole subtree is skipped.");
		
		/* Should not be check for now as it may lie in unused space.
		   It is just a wrong pointer. Skip it. */
		
		repair_filter_bad_level(fd, node->block->nr, level);
		goto error;
	} 
	
	if ((res = repair_node_check_struct(node, cb_count_sd, 
					    fd->repair->mode, fd)) < 0)
		return res;
	
	if (!(res & RE_FATAL)) {
		(*fd->stat.files) += fd->stat.tmp;
		fd->stat.tmp = 0;

		res |= repair_node_check_level(node, fd->repair->mode);
		if (res < 0) return res;
	}
	
	if ((items = reiser4_node_items(node)) == 0) {
		repair_filter_empty_node(fd, node->block->nr, level);
		reiser4_node_mkclean(node);
		goto error;
	}
	
	repair_error_count(fd->repair, res);
	
	if (res & RE_FATAL) {
		repair_filter_bad_node(fd, node->block->nr, level);
		goto error;
	} else if (res == 0) {
		if (reiser4_node_isdirty(node))
			repair_filter_fixed_node(fd, level);
	}
	
	/* There are no fatal errors, check delimiting keys. */
	if ((res = repair_tree_dknode_check((reiser4_tree_t *)node->tree,
					    node, fd->repair->mode)) < 0)
	{
		return res;
	}
	
	if (res) {
		repair_filter_bad_dk(fd, node->block->nr, level);
		goto error;
	}
	
	/* Zero all flags for all items. */
	if (fd->repair->mode == RM_BUILD)
		repair_node_clear_flags(node);
	
	aal_gauge_set_data(fd->gauge, node);
	aal_gauge_touch(fd->gauge);
	
	return 0;
 error:
	fd->level++;
	return RE_FATAL;
}

/* Update callback for traverse. It rollback changes made in setup_traverse 
   callback and do some essential stuff after traversing through the child -
   level, if RE_PTR flag is set - deletes the child pointer and mark
   the pointed block as unused in bm_used bitmap. */
static errno_t repair_filter_update_traverse(reiser4_place_t *place, 
					     void *data) 
{
	repair_filter_t *fd = (repair_filter_t *)data;
	reiser4_node_t *node;
	reiser4_tree_t *tree;
	errno_t res;
	blk_t blk;
    
	aal_assert("vpf-257", fd != NULL);
	aal_assert("vpf-434", place != NULL);
	aal_assert("vpf-434", place->node != NULL);
	aal_assert("vpf-434", place->node->tree != NULL);

	tree = (reiser4_tree_t *)place->node->tree;

	if ((blk = reiser4_item_down_link(place)) == INVAL_BLK) {
		aal_error("Node (%llu), item (%u), unit(%u): Failed to "
			  "fetch the node pointer.", place_blknr(place),
			  place->pos.item, place->pos.unit);
		return -EIO;
	}

	if (!fd->flags)
		return 0;

	if ((fd->flags & RE_FATAL) || (fd->flags & RE_EMPTY)) {
		fsck_mess("Node (%llu): the node is %s. Pointed from "
			  "the node (%llu), item (%u), unit (%u). %s",
			  blk, fd->flags & RE_EMPTY ? "empty" :
			  fd->repair->mode == RM_BUILD ? "unrecoverable" : 
			  "broken", place_blknr(place), place->pos.item,
			  place->pos.unit, fd->repair->mode == RM_BUILD ? 
			  "Removed." : "The whole subtree is skipped.");
	} else if (fd->flags & RE_DKEYS) {
		fsck_mess("Node (%llu), item (%u), unit (%u): Points to "
			  "the node [%llu] with wrong delimiting keys. %s",
			  place_blknr(place), place->pos.item, 
			  place->pos.unit, blk, fd->repair->mode == RM_BUILD ?
			  "Removed, content will be inserted later item-by-"
			  "item." : "The whole subtree is skipped.");
	}
	
	/* In the case of an error the node should be closed as it should 
	   be disconnected from the parent -- it may happen that another 
	   parent has a pointer to it. */
	if ((node = reiser4_tree_lookup_node(tree, blk))) {
		if ((res = reiser4_tree_disconnect_node(tree, node)))
			return -EINVAL;
		
		/* If there is another pointer to this node, 
		   changes should be saved. */
		if ((res = reiser4_node_fini(node)))
			return res;
	}
	
	if (fd->repair->mode == RM_BUILD) {
		pos_t prev;
		trans_hint_t hint;

		fd->repair->fatal--;
		/* The node corruption was not fixed - delete the 
		   internal item. */
		repair_place_get_lpos(place, prev);

		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;
		
		if ((res = reiser4_tree_remove(tree, place, &hint)))
			return res;

		place->pos = prev;
	} 

	fd->flags = 0;

	return 0;
}

/* After callback for traverse. Does needed stuff after traversing through all 
   children - if no child left, set RE_PTR flag to force deletion of the 
   pointer to this block in update_traverse callback. */
static errno_t repair_filter_after_traverse(reiser4_node_t *node, void *data) {
	repair_filter_t *fd = (repair_filter_t *)data;
	
	aal_assert("vpf-393", node != NULL);
	aal_assert("vpf-256", fd != NULL);    
	
	if (reiser4_node_items(node) == 0) {
		repair_filter_empty_node(fd, node->block->nr, 
					 reiser4_node_get_level(node));
		reiser4_node_mkclean(node);
	}

	fd->level++;
	return 0;
}

/* Does some update stuff after traverse through the internal tree - 
   deletes the pointer to the root block from the specific super block 
   if RE_PTR flag is set, mark that block used in bm_used bitmap 
   otherwise. */
static void repair_filter_update(repair_filter_t *fd) {
	repair_filter_stat_t *stat;
	reiser4_format_t *format;
	aal_stream_t stream;
	char *time_str;
	uint8_t height;
	
	aal_assert("vpf-421", fd != NULL);
	
	stat = &fd->stat;
	format = fd->repair->fs->format;
	
	if (fd->flags) {
		reiser4_tree_t *tree;
		reiser4_node_t *node;
		
		if (!(fd->flags & RE_PTR)) {
			fsck_mess("Root node (%llu): the node is %s. %s",
				  reiser4_format_get_root(format), 
				  fd->flags & RE_EMPTY ? "empty" :
				  fd->repair->mode == RM_BUILD ? 
				  "unrecoverable" : "broken",
				  fd->repair->mode == RM_BUILD ? "Zeroed." :
				  "The whole subtree is skipped.");
		} else {
			/* Wrong pointer. */
			aal_warn("Reiser4 storage tree does not exist. "
				 "Filter pass skipped.");
		}
	
		tree = fd->repair->fs->tree;
		node = fd->repair->fs->tree->root;
		
		if (node) {
			reiser4_tree_disconnect_node(tree, node);

			/* If there is another pointer to this node, 
			   changes should be saved. */
			reiser4_node_fini(node);
			tree->root = NULL;
		}

		if (fd->repair->mode == RM_BUILD) {
			reiser4_format_set_root(format, INVAL_BLK);
			fd->repair->fatal--;
		}
	}

	/* Check the tree height. */
	height = reiser4_format_get_height(fd->repair->fs->format);
	fd->level--;
	if (height != fd->level) {
		fsck_mess("The tree height %u found in the format is wrong. "
			 "%s %u.", height, fd->repair->mode == RM_CHECK ? 
			 "Should be" : "Fixed to", fd->level);

		if (fd->repair->mode == RM_CHECK) {
			fd->repair->fixable++;
		} else {
			reiser4_format_set_height(fd->repair->fs->format, 
						  fd->level);
		}
	}
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tRead nodes %llu\n", 
			  stat->read_nodes);
	aal_stream_format(&stream, "\tNodes left in the tree %llu\n",
			  stat->good_nodes);

	aal_stream_format(&stream, "\t\tLeaves of them %llu, Twigs of "
			  "them %llu\n", stat->good_leaves, 
			  stat->good_twigs);

	if (stat->fixed_nodes) {
		aal_stream_format(&stream, "\tCorrected nodes %llu\n",
				  stat->fixed_nodes);
		aal_stream_format(&stream, "\t\tLeaves of them %llu, "
				  "Twigs of them %llu\n", 
				  stat->fixed_leaves,
				  stat->fixed_twigs);
	}

	if (fd->stat.bad_nodes) {
		aal_stream_format(&stream, "\t%s of them %llu\n", 
				  fd->repair->mode == RM_BUILD ? 
				  "Emptied" : "Broken", 
				  fd->stat.bad_nodes);

		aal_stream_format(&stream, "\t\tLeaves of them %llu, "
				  "Twigs of them %llu\n", 
				  fd->stat.bad_leaves,
				  fd->stat.bad_twigs);
	}

	if (fd->stat.bad_dk_nodes) {
		aal_stream_format(&stream, "\tNodes with wrong "
				  "delimiting keys %llu\n",
				  fd->stat.bad_dk_nodes);

		aal_stream_format(&stream, "\t\tLeaves of them %llu, "
				  "Twigs of them %llu\n", 
				  fd->stat.bad_dk_leaves, 
				  fd->stat.bad_dk_twigs);
	}

	if (fd->stat.bad_ptrs) {
		aal_stream_format(&stream, "\t%s node pointers %llu\n",
				  fd->repair->mode == RM_BUILD ?
				  "Zeroed" : "Invalid", 
				  fd->stat.bad_ptrs);
	}

	time_str = ctime(&fd->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&fd->stat.time);
	time_str = ctime(&fd->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	aal_mess(stream.entity);
	aal_stream_fini(&stream);
}

static errno_t repair_filter_traverse(repair_filter_t *fd) {
	reiser4_format_t *format;
	reiser4_tree_t *tree;
	errno_t res;
	blk_t root;
	
	aal_assert("vpf-1314", fd != NULL);
	
	format = fd->repair->fs->format;
	tree = fd->repair->fs->tree;
	root = reiser4_format_get_root(format);
	
	/* Check the root pointer to be valid block. */
	if (root < reiser4_format_start(format) || 
	    root > reiser4_format_get_len(format))
	{
		goto error;
	} else if (aux_bitmap_test(fd->bm_used, root)) {
		/* This block is from format area. */
		goto error;
	}
	
	/* try to open the root node. */
	if (!(tree->root = repair_tree_load_node(fd->repair->fs->tree, 
						 NULL, root, 0)))
	{
		fsck_mess("Node (%llu): failed to open the root node. "
			  "The whole filter pass is skipped.", root);
		
		goto error;
	}
	
	repair_filter_read_node(fd, root, reiser4_node_get_level(tree->root));
	
	/* If SB's mkfs id exists and matches the root node's one, 
	   check the mkfs id of all nodes. */
	*fd->check_node = (reiser4_format_get_stamp(format) && 
			   (reiser4_format_get_stamp(format) ==
			    reiser4_node_get_mstamp(tree->root)));
	
	aal_gauge_touch(fd->gauge);

	/* Cut the corrupted, unrecoverable parts of the tree off. */
	res = reiser4_tree_trav_node(tree, tree->root,
				     repair_filter_node_open,
				     repair_filter_node_check,
				     repair_filter_update_traverse,  
				     repair_filter_after_traverse, fd);

	aal_gauge_done(fd->gauge);
	
	return res < 0 ? res : 0;
 error:
	repair_filter_bad_ptr(fd);

	return 0;
}

/* The pass itself - goes through the existent tree trying to filter all 
   corrupted parts off, and fixing what can be fixed. Account all kind of 
   nodes in corresponding bitmaps. */
errno_t repair_filter(repair_filter_t *fd) {
	errno_t res = 0;

	aal_assert("vpf-536", fd != NULL);
	aal_assert("vpf-814", fd->repair != NULL);
	aal_assert("vpf-843", fd->repair->fs != NULL);
	aal_assert("vpf-816", fd->repair->fs->tree != NULL);
	aal_assert("vpf-815", fd->bm_used != NULL);

	aal_mess("CHECKING STORAGE TREE");
	fd->gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS], 
				     cb_gauge_tree_percent, NULL, 500, NULL);
	time(&fd->stat.time);
	
	res = repair_filter_traverse(fd);
	
	aal_gauge_free(fd->gauge);
	repair_filter_update(fd);
	
	if (fd->repair->mode != RM_CHECK)
		reiser4_fs_sync(fd->repair->fs);
	
	return res;
}
