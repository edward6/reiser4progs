/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/filter.c - Filter pass of file system recovery.
   
   The filter pass filters corrupted parts of a reiser4 tree out, repairs 
   all recoverable corruptions and builds a map of all used blocks, but 
   extents format + formatted nodes). Extents are left not checked as there
   is no enough information for their proper check. */

#include <repair/filter.h>

/* This is extention for repair_error_t. */
typedef enum repair_error_filter {
	
	/* Pointer to the wrong place. */
	RE_NPTR		= (RE_LAST),
	
	/* Node is ok, but dkeys are wrong, remove from the tree and insert
	   back later item-by-item. */
	RE_DKEYS	= (RE_LAST << 1)
} repair_error_filter_t;

/* Open callback for traverse. It opens a node at passed blk. It does 
   nothing if RE_NPTR is set and set this flag if node cannot 
   be opeened. Returns error if any. */
static reiser4_node_t *repair_filter_node_open(reiser4_tree_t *tree,
					       reiser4_place_t *place,
					       void *data)
{
	repair_filter_t *fd = (repair_filter_t *)data;
	reiser4_node_t *node = NULL;
	ptr_hint_t ptr;
	
	aal_assert("vpf-379", fd != NULL);
	aal_assert("vpf-433", fd->repair != NULL);
	aal_assert("vpf-842", fd->repair->fs != NULL);
	aal_assert("vpf-591", fd->repair->fs->format != NULL);
	aal_assert("vpf-1118", tree != NULL);
	aal_assert("vpf-1187", place != NULL);
	
	/* Fetching node ptr */
	plug_call(place->plug->o.item_ops, read, (place_t *)place,
		  &ptr, place->pos.unit, 1);
	
	if (ptr.start > fd->bm_used->total ||
	    ptr.width > fd->bm_used->total ||
	    ptr.start > fd->bm_used->total - ptr.width || 
	    !aux_bitmap_test_region(fd->bm_used, ptr.start, ptr.width, 0))
	{
		/* Bad pointer detected. Remove if possible. */
		aal_exception_error("Node (%llu), item (%u), unit (%u): Points "
				    "to invalid region [%llu..%llu] or some "
				    "blocks are used already. %s", 
				    node_blocknr(place->node), place->pos.item,
				    place->pos.unit, ptr.start,
				    ptr.start + ptr.width - 1,
				    fd->repair->mode == RM_BUILD ?
				    "Removed." : "The whole subtree is "
				    "skipped.");
			
		fd->stat.bad_ptrs += ptr.width;
		if (fd->repair->mode == RM_BUILD) {
			pos_t ppos;
			remove_hint_t hint;
			
			repair_place_get_lpos(place, ppos);

			hint.count = 1;
			
			if (reiser4_node_remove(place->node, &place->pos, &hint))
				return INVAL_PTR;
			
			place->pos = ppos;
		} else
			fd->repair->fatal++;
		
		return NULL;
	}
	
	if ((node = repair_node_open(fd->repair->fs, ptr.start)) == NULL) {
		fd->flags |= RE_NPTR;
		fd->repair->fatal++;
		return NULL;
	}
	
	if (reiser4_tree_connect(tree, place->node, node))
		goto error_close_node;
	
	if (fd->progress_handler && fd->level != LEAF_LEVEL) {
		fd->progress->state = PROGRESS_UPDATE;
		fd->progress->u.tree.i_total = reiser4_node_items(place->node);
		fd->progress->u.tree.u_total = reiser4_item_units(place);
		fd->progress->u.tree.item = place->pos.item;
		fd->progress->u.tree.unit = place->pos.unit;
		fd->progress_handler(fd->progress);
	}
	
	aux_bitmap_mark_region(fd->bm_used, ptr.start, ptr.width);
	
	return node;
	
 error_close_node:
	reiser4_node_close(node);
	return INVAL_PTR;
}

/* Before callback for traverse. It checks node level, node consistency, and 
   delimiting keys. If any check reveals a problem with the data consistency 
   it sets RE_NPTR flag. */
static errno_t repair_filter_node_check(reiser4_tree_t *tree,
					reiser4_node_t *node,
					void *data)
{
	repair_filter_t *fd = (repair_filter_t *)data;
	errno_t res = 0;
	uint16_t level;
    
	aal_assert("vpf-252", data  != NULL);
	aal_assert("vpf-409", node != NULL);
    
	fd->stat.read_nodes++;
	if (fd->progress_handler && fd->level != LEAF_LEVEL) {
		fd->progress->state = PROGRESS_START;
		fd->progress->u.tree.i_total = reiser4_node_items(node);
		fd->progress->u.tree.u_total = 0;
		fd->progress->u.tree.item = 0;
		fd->progress->u.tree.unit = 0;
		fd->progress_handler(fd->progress);
	}
    
	level = reiser4_node_get_level(node); 
    
	/* Initialize the level for the root node before traverse. */
	if (!fd->level)
		fd->level = level;
    
	/* Skip this check if level is not set (root node only). */
	if (fd->level - 1 != level) {
		aal_exception_error("Level of the node (%u) mismatches to the "
				    "expected one (%u).", level, fd->level);
		
		/* Should not be check for now as it may lie in unused space.
		   It is just a wrong pointer. Skip it. */
		fd->flags |= RE_NPTR;
		fd->repair->fatal++;
		return 1;
	} else 
		fd->level--;
	
	if ((res = repair_node_check_struct(node, fd->repair->mode)) < 0)
		return res;
	
	repair_error_check(res, fd->repair->mode);
	
	if (reiser4_node_items(node) == 0)
		res |= RE_FATAL;
	
	if (res & RE_FATAL) {
		fd->flags |= RE_FATAL;
		fd->repair->fatal++;
		return 1;
	} else if (res & RE_FIXABLE) {
		fd->repair->fixable++;
	} else {
		aal_assert("vpf-799", res == 0);
		
		if (reiser4_node_isdirty(node)) {
			fd->stat.fixed_nodes++;
			
			if (level == LEAF_LEVEL)
				fd->stat.fixed_leaves++;
			else if (level == TWIG_LEVEL)
				fd->stat.fixed_twigs++;
		}
	}
	
	/* There are no fatal errors, check delimiting keys. */
	if ((res = repair_tree_dknode_check(tree, node, fd->repair->mode)) < 0)
		return res;
	
	if (res) {
		fd->flags |= RE_DKEYS;
		fd->repair->fatal++;
		return res;
	}
	
	return 0;
}

#if 0
/* Setup callback for traverse. Prepares essential information for a child of 
   a node - level. */
static errno_t repair_filter_setup_traverse(reiser4_place_t *place, void *data) {
	repair_filter_t *fd = (repair_filter_t *)data;
	ptr_hint_t ptr;

	aal_assert("vpf-255", data != NULL);
	aal_assert("vpf-531", place != NULL);
	aal_assert("vpf-703", reiser4_item_branch(place));
    
	if (fd->progress_handler && fd->level != LEAF_LEVEL) {
		fd->progress->state = PROGRESS_UPDATE;
		fd->progress->u.tree.i_total = reiser4_node_items(place->node);
		fd->progress->u.tree.u_total = reiser4_item_units(place);
		fd->progress->u.tree.item = place->pos.item;
		fd->progress->u.tree.unit = place->pos.unit;
		fd->progress_handler(fd->progress);
	}
	
	if (plug_call(place->item.plug->o.item_ops, read, &place->item, &ptr,
		      place->pos.unit, 1) != 1)
	{
		aal_exception_fatal("Node (%llu), item (%u), unit(%u): Failed to "
				    "fetch the node pointer.", place->node, 
				    place->pos.item, place->pos.unit);
		
		return -EINVAL;
	}
    
	/* FIXME: as a result layout of nodeptr items is checked automatically, 
	   what is not very well as read does not mean that we get pointed block. */
	
	/* The validness of this node pointer must be checked at node_check time. */
	if (ptr.start < fd->bm_used->total && ptr.width < fd->bm_used->total &&
	    ptr.start < fd->bm_used->total - ptr.width && 
	    aux_bitmap_test_region(fd->bm_used, ptr.start, ptr.width, 0))
	{
		aux_bitmap_mark_region(fd->bm_used, ptr.start, ptr.width);
	} else {
		/* Bad pointer detected. Remove if possible. */
		aal_exception_error("Node (%llu), item (%u), unit (%u): Points to "
				    "invalid region [%llu..%llu] or some blocks are "
				    "used already. %s", node_blocknr(place->node), 
				    place->pos.item, place->pos.unit, ptr.start,
				    ptr.start + ptr.width - 1, 
				    fd->repair->mode == RM_BUILD ? 
					"Removed." : "The whole subtree is skipped.");
			
		fd->stat.bad_ptrs += ptr.width;
		if (fd->repair->mode == RM_BUILD) {
			pos_t ppos;
			
			repair_place_get_lpos(place, ppos);
			
			if (reiser4_node_remove(place->node, &place->pos, 1)) {
				aal_exception_error("Node (%llu), pos (%u, %u): "
						    "Remove failed.", node_blocknr(place->node),
						    place->pos.item, place->pos.unit);
				return -EINVAL;
			}
			
			place->pos = ppos;
		} else
			fd->repair->fatal++;
		
		return 1;
	}
	
	fd->level--;
	
	return 0;
}
#endif

/* Update callback for traverse. It rollback changes made in setup_traverse 
   callback and do some essential stuff after traversing through the child -
   level, if RE_NPTR flag is set - deletes the child pointer and mark
   the pointed block as unused in bm_used bitmap. */
static errno_t repair_filter_update_traverse(reiser4_tree_t *tree, 
					     reiser4_place_t *place, 
					     void *data) 
{
	repair_filter_t *fd = (repair_filter_t *)data;
	ptr_hint_t ptr;
	uint8_t level;
    
	aal_assert("vpf-257", fd != NULL);
	aal_assert("vpf-434", place != NULL);
    
	if (plug_call(place->plug->o.item_ops, read, (place_t *)place, 
		      &ptr, place->pos.unit, 1) != 1)
	{
		aal_exception_fatal("Node (%llu), item (%u), unit(%u): Failed "
				    "to fetch the node pointer.",node_blocknr(place->node),
				    place->pos.item, place->pos.unit);
		return -EINVAL;
	}
	
	if (fd->flags) {
		/* Clear pointed block in the formatted bitmap. */
		aux_bitmap_clear_region(fd->bm_used, ptr.start, ptr.width);
		
		if (fd->flags & RE_NPTR) {
			aal_exception_error("Node (%llu), item (%u), unit (%u): Points "
					    "to the invalid node [%llu]. %s", 
					    node_blocknr(place->node), place->pos.item, 
					    place->pos.unit, ptr.start, 
					    fd->repair->mode == RM_BUILD ? 
					    "Removed." : "The whole subtree is skipped.");
			
			fd->stat.bad_ptrs += ptr.width;
		} else if (fd->flags & RE_FATAL) {
			aal_exception_error("Node (%llu), item (%u), unit (%u): Points "
					    "to the %s node [%llu]. %s", 
					    node_blocknr(place->node), place->pos.item, 
					    place->pos.unit, 
					    fd->repair->mode == RM_BUILD ? 
					    "emptied" : "unrecoverable", ptr.start, 
					    fd->repair->mode == RM_BUILD ? 
					    "Removed." : "The whole subtree is skipped.");
			
			level = reiser4_node_get_level(place->node);
			
			/* Extents cannot point to this node. */
			aux_bitmap_mark_region(fd->bm_met, ptr.start, ptr.width);
			fd->stat.bad_nodes += ptr.width;
			if (level == LEAF_LEVEL)
				fd->stat.bad_leaves += ptr.width;
			else if (level == TWIG_LEVEL)
				fd->stat.bad_twigs += ptr.width;
		} else if (fd->flags & RE_DKEYS) {
			aal_exception_error("Node (%llu), item (%u), unit (%u): Points "
					    "to the node [%llu] with wrong delimiting "
					    "keys. %s", node_blocknr(place->node), 
					    place->pos.item, place->pos.unit, 
					    ptr.start, 
					    fd->repair->mode == RM_BUILD ? 
					    "Removed." : "The whole subtree is skipped.");
			
			level = reiser4_node_get_level(place->node);
			
			fd->stat.bad_dk_nodes += ptr.width;
			/* Insert it later. FIXME: This is hardcoded, should be 
			   changed. */
			if (level == LEAF_LEVEL) {
				aux_bitmap_mark_region(fd->bm_leaf, ptr.start, 
						       ptr.width);
				fd->stat.bad_dk_leaves += ptr.width;
			} else if (level == TWIG_LEVEL) {
				aux_bitmap_mark_region(fd->bm_twig, ptr.start, 
						       ptr.width);
				fd->stat.bad_dk_twigs += ptr.width;
			} else
				aux_bitmap_mark_region(fd->bm_met, ptr.start, 
						       ptr.width);
		} else {
			aal_assert("vpf-827: Not expected case.", FALSE);
		}
		
		if (fd->repair->mode == RM_BUILD) {
			pos_t prev;
			remove_hint_t hint;
			
			fd->repair->fatal--;
			/* The node corruption was not fixed - delete the 
			   internal item. */
			repair_place_get_lpos(place, prev);

			hint.count = 1;

			if (reiser4_node_remove(place->node, &place->pos, &hint))
				return -EINVAL;
			
			place->pos = prev;
		} 
		
		fd->flags = 0;
	} else {
		/* FIXME-VITALY: hardcoded level, should be changed. */
		fd->stat.good_nodes += ptr.width;
		if (reiser4_node_get_level(place->node) == TWIG_LEVEL + 1) {
			aux_bitmap_mark_region(fd->bm_twig, ptr.start, ptr.width);
			fd->stat.good_twigs += ptr.width;
		} else if (reiser4_node_get_level(place->node) == TWIG_LEVEL) {
			aux_bitmap_mark_region(fd->bm_leaf, ptr.start, ptr.width);
			fd->stat.good_leaves += ptr.width;
		}
	}
    
	fd->level++;

	return 0;
}

/* After callback for traverse. Does needed stuff after traversing through all 
   children - if no child left, set RE_NPTR flag to force deletion of the 
   pointer to this block in update_traverse callback. */
static errno_t repair_filter_after_traverse(reiser4_tree_t *tree, 
					    reiser4_node_t *node, 
					    void *data) 
{
	repair_filter_t *fd = (repair_filter_t *)data;
	
	aal_assert("vpf-393", node != NULL);
	aal_assert("vpf-256", fd != NULL);    
	
	if (reiser4_node_items(node) == 0) {
		fd->flags |= RE_FATAL;
		reiser4_node_mkclean(node);
	}
	
	if (fd->progress_handler && fd->level != LEAF_LEVEL) {
		fd->progress->state = PROGRESS_END;
		fd->progress_handler(fd->progress);
	}
	
	return 0;
}

/* Setup data (common and specific) before traverse through the tree. */
static void repair_filter_setup(repair_filter_t *fd) {
	reiser4_format_t *format = fd->repair->fs->format;
	blk_t root;
        
	root = reiser4_format_get_root(format);
	
	/* Check the root pointer to be valid block. */
	if (root < reiser4_format_start(format) || 
	    root > reiser4_format_get_len(format))
	{
		/* Wrong pointer. */
		fd->flags |= RE_NPTR;
		fd->repair->fatal++;
	} else if (aux_bitmap_test(fd->bm_used, 
				   reiser4_format_get_root(format)))
	{
		/* This block is from format area. */
		fd->flags |= RE_NPTR;
		fd->repair->fatal++;
	} else	{
		/* We meet the block for the first time. */
		aux_bitmap_mark(fd->bm_used, 
				reiser4_format_get_root(format));
	}
	
	aal_memset(fd->progress, 0, sizeof(*fd->progress));
	fd->progress->type = GAUGE_TREE;
	fd->progress->title = "***** Tree Traverse Pass: scanning the reiser4 "
		"internal tree.";
	fd->progress->text = "";
	time(&fd->stat.time);
}

/* Does some update stuff after traverse through the internal tree - 
   deletes the pointer to the root block from the specific super block 
   if RE_NPTR flag is set, mark that block used in bm_used bitmap 
   otherwise. */
static void repair_filter_update(repair_filter_t *fd, 
				 reiser4_node_t *root) 
{
	repair_filter_stat_t *stat;
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-421", fd != NULL);
	aal_assert("vpf-863", root != NULL);
	
	stat = &fd->stat;
	
	if (fd->flags & (RE_NPTR | RE_FATAL)) {
		aux_bitmap_clear(fd->bm_used, node_blocknr(root));

		fd->flags = 0;
		if (fd->flags & RE_NPTR)
			stat->bad_ptrs++;
		else
			stat->bad_nodes++;

		if (fd->repair->mode == RM_BUILD) {
			reiser4_format_set_root(fd->repair->fs->format, 
						INVAL_BLK);
			/* FIXME: sync it to disk. */
		}
	} else {
		aal_assert("vpf-862", fd->flags == 0);
		
		/* FIXME-VITALY: hardcoded level, should be changed. */
		if (reiser4_node_get_level(root) == TWIG_LEVEL) {
			aux_bitmap_mark(fd->bm_twig, node_blocknr(root));
			stat->good_twigs++;
		} else if (reiser4_node_get_level(root) == LEAF_LEVEL) {
			aux_bitmap_mark(fd->bm_leaf, node_blocknr(root));
			stat->good_leaves++;
		}
		
		stat->good_nodes++;
	}

	if (!fd->progress_handler)
		return;

	aal_stream_init(&stream);
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

	fd->progress->state = PROGRESS_STAT;
	fd->progress->text = (char *)stream.data;
	fd->progress_handler(fd->progress);

	aal_stream_fini(&stream);
}

/* The pass itself - goes through the existent tree trying to filter all 
   corrupted parts off, and fixing what can be fixed. Account all kind of 
   nodes in corresponding bitmaps. */
errno_t repair_filter(repair_filter_t *fd) {
	repair_progress_t progress;
	reiser4_fs_t *fs;
	errno_t res = 0;

	aal_assert("vpf-536", fd != NULL);
	aal_assert("vpf-814", fd->repair != NULL);
	aal_assert("vpf-843", fd->repair->fs != NULL);
	aal_assert("vpf-816", fd->repair->fs->tree != NULL);
	aal_assert("vpf-815", fd->bm_used != NULL);
	aal_assert("vpf-814", fd->bm_leaf != NULL);
	aal_assert("vpf-814", fd->bm_twig != NULL);
	aal_assert("vpf-814", fd->bm_met != NULL);
    
	fs = fd->repair->fs;
    
	if (reiser4_tree_fresh(fs->tree)) {
		aal_exception_warn("Reiser4 storage tree does not exist. "
				   "Filter pass skipped.");
		return 0;
	}
	
	fd->progress = &progress;
	repair_filter_setup(fd);
	
	fs->tree->root = repair_node_open(fd->repair->fs, 
					  reiser4_format_get_root(fs->format));
	
	if (fs->tree->root != NULL) {
		/* Cut the corrupted, unrecoverable parts of the tree off. */
		res = reiser4_tree_down(fs->tree, fs->tree->root, 
					repair_filter_node_open,
					repair_filter_node_check,
					repair_filter_update_traverse,  
					repair_filter_after_traverse, fs);
	
		if (res < 0)
			return res;
	} else
		fd->flags |= RE_NPTR;
	
	repair_filter_update(fd, fs->tree->root);
	
	reiser4_tree_collapse(fs->tree);
	
	return res;
}
