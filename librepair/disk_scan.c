/*  Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    librepair/disk_scan.c - Disk scan pass of reiser4 filesystem recovery. 
    
    The disk_scan pass scans the blocks which are specified in the bm_scan 
    bitmap, all formatted blocks marks in bm_map bitmap, all found leaves 
    in bm_leaf, all found twigs in bm_twig.

    After filter pass:
    - some extent units may points to formatted blocks;
    - if some formatted block is unused in on-disk block allocator, correct 
    allocator;
    - if some unformatted block is used in on-disk block allocator, zero 
    extent pointer. */

#include <repair/disk_scan.h>

static void repair_disk_scan_update(repair_ds_t *ds) {
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-882", ds != NULL);
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tRead nodes %llu\n", ds->stat.read_nodes);
	aal_stream_format(&stream, "\tGood nodes %llu\n", ds->stat.good_nodes);
	
	aal_stream_format(&stream, "\t\tLeaves of them %llu, Twigs of them "
			  "%llu\n", ds->stat.good_leaves, ds->stat.good_twigs);
	
	if (ds->stat.fixed_nodes) {
		aal_stream_format(&stream, "\tCorrected nodes %llu\n", 
				  ds->stat.fixed_nodes);
		aal_stream_format(&stream, "\t\tLeaves of them %llu, Twigs of "
				  "them %llu\n", ds->stat.fixed_leaves, 
				  ds->stat.fixed_twigs);
	}
	
	time_str = ctime(&ds->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&ds->stat.time);
	time_str = ctime(&ds->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	aal_mess(stream.entity);
	aal_stream_fini(&stream);
}

static errno_t cb_count_sd(reiser4_place_t *place, void *data) {
	repair_ds_t *ds = (repair_ds_t *)data;

	if (place->plug->id.group == STAT_ITEM)
		ds->stat.tmp++;

	return 0;
}


/* The pass inself, goes through all the blocks marked in the scan bitmap, and
   if a block can contain some data to be recovered (formatted and contains not
   tree index data only) then fix all corruptions within the node and save it
   for further insertion. */
errno_t repair_disk_scan(repair_ds_t *ds) {
	reiser4_node_t *node;
	aal_gauge_t *gauge;
	errno_t res = 0;
	uint64_t total;
	uint8_t level;
	blk_t blk = 0;
	
	aal_assert("vpf-514", ds != NULL);
	aal_assert("vpf-705", ds->repair != NULL);
	aal_assert("vpf-844", ds->repair->fs != NULL);
	aal_assert("vpf-515", ds->bm_leaf != NULL);
	aal_assert("vpf-516", ds->bm_twig != NULL);
	aal_assert("vpf-820", ds->bm_scan != NULL);
	aal_assert("vpf-820", ds->bm_met != NULL);    
	
	aal_mess("LOOKING FOR UNCONNECTED NODES");
	gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS], 
				 NULL, NULL, 500, NULL);
	aal_gauge_touch(gauge);
	time(&ds->stat.time);

	total = reiser4_bitmap_marked(ds->bm_scan);
	
	while ((blk = reiser4_bitmap_find_marked(ds->bm_scan, blk)) 
	       != INVAL_BLK) 
	{
		ds->stat.read_nodes++;
		aal_gauge_set_value(gauge, ds->stat.read_nodes * 100 / total);
		aal_gauge_touch(gauge);
		
		if (!(node = repair_node_open(ds->repair->fs->tree, blk, 
					      ds->mkidok ? ds->mkid : 0)))
		{
			blk++;
			continue;
		}
		
		reiser4_bitmap_mark(ds->bm_met, blk);
		
		level = reiser4_node_get_level(node);
		
		if (!repair_tree_data_level(level))
			goto next;
		
		if ((res = repair_node_check_struct(node, cb_count_sd, 
						    ds->repair->mode, ds)) < 0)
		{
			reiser4_node_close(node);
			goto error;
		}
		
		if (!(res & RE_FATAL)) {
			(*ds->stat.files) += ds->stat.tmp;
			ds->stat.tmp = 0;
			
			res |= repair_node_check_level(node, ds->repair->mode);
			
			if (res < 0) {
				reiser4_node_close(node);
				goto error;
			}
		}
		
		aal_assert("vpf-812", (res & ~RE_FATAL) == 0);
		
		if (res || reiser4_node_items(node) == 0)
			goto next;
		
		ds->stat.good_nodes++;
		if (level == TWIG_LEVEL) {
			reiser4_bitmap_mark(ds->bm_twig, blk);
			ds->stat.good_twigs++;
			
			if (reiser4_node_isdirty(node))
				ds->stat.fixed_twigs++;
		} else {
			reiser4_bitmap_mark(ds->bm_leaf, blk);
			ds->stat.good_leaves++;
			
			if (reiser4_node_isdirty(node))
				ds->stat.fixed_leaves++;
		}
		
		/* Zero all flags for all items. */
		repair_node_clear_flags(node);

		/* If mkfsid is a new one, set it to the node. */
		if (!ds->mkidok && ds->mkid != reiser4_node_get_mstamp(node))
			reiser4_node_set_mstamp(node, ds->mkid);

	next:
		reiser4_node_fini(node);
		blk++;
	}
 error:
	aal_gauge_done(gauge);
	aal_gauge_free(gauge);
	repair_disk_scan_update(ds);
	return res;
}
