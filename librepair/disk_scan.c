/* 
    librepair/disk_scan.c - Disk scan pass of reiser4 filesystem recovery. 
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING. 
*/

/* 
    The disk_scan pass scans the blocks which are specified in the bm_scan 
    bitmap, all formatted blocks marks in bm_map bitmap, all found leaves 
    in bm_leaf, all found twigs in bm_twig.

    After filter pass:
    - some extent units may points to formatted blocks;
    - if some formatted block is unused in on-disk block allocator, 
    correct allocator;
    - if some unformatted block is used in on-disk block allocator,
    zero extent pointer.
*/

#include <repair/librepair.h>
#include <repair/disk_scan.h>

/* The pass inself, goes through all the blocks marked in the scan bitmap, 
 * and if a block can contain some data to be recovered (formatted and contains 
 * not tree index data only) then fix all corruptions within the node and 
 * save it for further insertion. */
errno_t repair_disk_scan(repair_ds_t *ds) {
    repair_progress_t progress;
    reiser4_node_t *node;
    errno_t res = 0;
    uint8_t level;
    blk_t blk = 0;

    aal_assert("vpf-514", ds != NULL);
    aal_assert("vpf-705", ds->repair != NULL);
    aal_assert("vpf-844", ds->repair->fs != NULL);
    aal_assert("vpf-515", ds->bm_leaf != NULL);
    aal_assert("vpf-516", ds->bm_twig != NULL);
    aal_assert("vpf-820", ds->bm_scan != NULL);
    aal_assert("vpf-820", ds->bm_met != NULL);    

    
    if (ds->progress_handler) {
	aal_memset(&progress, 0, sizeof(repair_progress_t));
	progress.type = PROGRESS_INDICATOR;
	progress.state = PROGRESS_START;
	progress.total = aux_bitmap_marked(ds->bm_scan);
	progress.title = "DiskScan Pass: scanning the partition for lost "
	    "nodes:";
	ds->progress_handler(&progress);
	progress.state = PROGRESS_UPDATE;
	progress.text = "";
    }
    
    while ((blk = aux_bitmap_find_marked(ds->bm_scan, blk)) != INVAL_BLK) {
	if (ds->progress_handler) {
	    progress.done++;
	    ds->progress_handler(&progress);
	}
 
	node = repair_node_open(ds->repair->fs, blk);
	if (node == NULL) {
	    blk++;
	    continue;
	}
	
	aux_bitmap_mark(ds->bm_met, blk);
	
	level = reiser4_node_get_level(node);

	if (!repair_tree_data_level(level))
	    goto next;

	res = repair_node_check(node, ds->repair->mode);

	if (res < 0) {
	    reiser4_node_close(node);
	    goto error;
	}
	
	aal_assert("vpf-812", (res & ~REPAIR_FATAL) == 0);
	
	if (repair_error_exists(res) || reiser4_node_items(node) == 0)
	    goto next;
	
	if (level == TWIG_LEVEL)
	    aux_bitmap_mark(ds->bm_twig, blk);
	else
	    aux_bitmap_mark(ds->bm_leaf, blk);
	
    next:
	reiser4_node_close(node);
	blk++;
    }
    
error:
    
    if (ds->progress_handler) {
	progress.state = PROGRESS_END;
	ds->progress_handler(&progress);
    }
    
    return res;
}

