/*
  debugfs.c -- program for debugging reiser4 filesystem.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <aux/aux.h>
#include <misc/misc.h>
#include <reiser4/reiser4.h>

enum print_flags {
	PF_SUPER    = 1 << 0,
	PF_JOURNAL  = 1 << 1,
	PF_ALLOC    = 1 << 2,
	PF_OID	    = 1 << 3,
	PF_TREE	    = 1 << 4,
	PF_ITEMS    = 1 << 5,
	PF_BLOCK    = 1 << 6
};

typedef enum print_flags print_flags_t;

enum behav_flags {
	BF_FORCE    = 1 << 0,
	BF_QUIET    = 1 << 1,
	BF_TFRAG    = 1 << 2,
	BF_FFRAG    = 1 << 3,
	BF_NPACK    = 1 << 4,
	BF_DFRAG    = 1 << 5,
	BF_SEACH    = 1 << 6,
	BF_LS       = 1 << 7,
	BF_CAT      = 1 << 8
};

typedef enum behav_flags behav_flags_t;

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help            prints program usage.\n"
		"  -V, --version             prints current version.\n"
		"  -q, --quiet               forces creating filesystem without\n"
		"                            any questions.\n"
		"  -f, --force               makes debugfs to use whole disk, not\n"
		"                            block device or mounted partition.\n"
		"Browsing options:\n"
		"  -l, --ls FILE             browses passed file like standard\n"
		"                            ls program.\n"
		"  -c, --cat FILE            browses passed file like standard\n"
		"                            cat program.\n"
		"Print options:\n"
		"  -i, --print-items         forces debugfs.reiser4 to print\n"
		"                            items content if --print-tree was\n"
		"                            specified.\n"
		"  -t, --print-tree          prints the whole tree.\n"
		"  -j, --print-journal       prints journal.\n"
		"  -s, --print-super         prints the both super blocks.\n"
		"  -b, --print-block-alloc   prints block allocator data.\n"
		"  -o, --print-oid-alloc     prints oid allocator data.\n"
		"  -n, --print-block N       prints block by its number.\n"
		"Measurement options:\n"
		"  -N, --node-packing        measures avarage node packing.\n"
		"  -T, --tree-frag           measures tree fragmentation.\n"
		"  -F, --file-frag FILE      measures fragmentation of specified\n"
		"                            file.\n"
		"  -D, --data-frag           measures average files fragmentation.\n"
		"  -p, --show-each           show file fragmentation for each file\n"
		"                            if --data-frag is specified.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE     profile to be used.\n"
		"  -K, --known-profiles      prints known profiles.\n");
}

/* Initializes used by debugfs exception streams */
static void debugfs_init(void) {
	int ex;

	/* Initializing memory pressure hooks */
	progs_mpressure_init();
	
	/* Setting up exception streams*/
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

struct tree_print_hint {
	reiser4_tree_t *tree;
	print_flags_t flags;
};

/* Callback function used in traverse for opening the node */
static errno_t common_open_joint(
	reiser4_joint_t **joint,    /* joint to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	struct tree_print_hint *print_hint =
		(struct tree_print_hint *)data;

	*joint = reiser4_tree_load(print_hint->tree, blk);
	return -(*joint == NULL);
}

static errno_t debugfs_print_joint(
	reiser4_joint_t *joint,	    /* joint to be printed */
	void *data)		    /* traverse data */
{	
	aal_stream_t stream;

	struct tree_print_hint *print_hint =
		(struct tree_print_hint *)data;

	aal_stream_init(&stream);

	if (reiser4_node_print(joint->node, &stream, print_hint->flags & PF_ITEMS))
		goto error_free_stream;

	printf((char *)stream.data);

	aal_stream_fini(&stream);
	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_tree(reiser4_fs_t *fs, print_flags_t flags) {
	traverse_hint_t hint;
	struct tree_print_hint print_hint = {fs->tree, flags};
	
	hint.objects = 1 << NODEPTR_ITEM;
	hint.data = &print_hint;
	hint.cleanup = 1;
	
	reiser4_joint_traverse(fs->tree->root, &hint, common_open_joint, 
			       debugfs_print_joint, NULL, NULL, NULL);
    
	printf("\n");
    
	return 0;
}

errno_t debugfs_print_master(reiser4_fs_t *fs) {
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL, return -1);

	aal_stream_init(&stream);
		
	if (reiser4_master_print(fs->master, &stream))
		return -1;

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	{
		char uuid[37];
		uuid_unparse(reiser4_master_uuid(fs->master), uuid);
		aal_stream_format(&stream, "uuid:\t\t%s\n", uuid);
	}
#endif

	printf((char *)stream.data);
	printf("\n");
	
	return 0;
}

static errno_t debugfs_print_format(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->format->entity->plugin->format_ops.print) {
		aal_exception_info("Format print method is not implemented.");
		return 0;
	}
    
	aal_stream_init(&stream);
	
	printf("Format super block:\n");
	if (reiser4_format_print(fs->format, &stream)) {
		aal_exception_error("Can't print format specific super block.");
		goto error_free_stream;
	}
    
	printf((char *)stream.data);
	printf("\n");

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	aal_stream_t stream;
    
	if (!fs->oid->entity->plugin->oid_ops.print) {
		aal_exception_info("Oid allocator print method is not implemented.");
		return 0;
	}

	aal_stream_init(&stream);
    
	printf("Oid allocator:\n");
	if (fs->oid->entity->plugin->oid_ops.print(fs->oid->entity,
						   &stream, 0))
	{
		aal_exception_error("Can't print oid allocator.");
		goto error_free_stream;;
	}

	printf((char *)stream.data);
	printf("\n");

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, block allocator print "
			    "is not implemented yet!");
	return 0;
}
   
static errno_t debugfs_print_journal(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, journal print is not implemented yet!");
	return 0;
}

struct tree_frag_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	blk_t curr;
	count_t total, bad;
	uint16_t level;

	count_t internals;
};

static errno_t frag_open_joint(
	reiser4_joint_t **joint,    /* joint to be opened */
	blk_t blk,                  /* blk node lies in */
	void *data)		    /* traverse hint */
{	
	struct tree_frag_hint *frag_hint =
		(struct tree_frag_hint *)data;

	*joint = NULL;

	aal_assert("umka-1556", frag_hint->level > 0, return -1);
	
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	*joint = reiser4_tree_load(frag_hint->tree, blk);
	return -(*joint == NULL);
}

static errno_t callback_tree_frag(
	reiser4_joint_t *joint,	   /* joint to be estimated */
	void *data)	   /* user-specified data */
{
	reiser4_pos_t pos;
	reiser4_node_t *node = joint->node;
	
	struct tree_frag_hint *frag_hint =
		(struct tree_frag_hint *)data;

	if (frag_hint->level <= LEAF_LEVEL)
		return 0;

	frag_hint->internals++;
	
	aal_gauge_update(frag_hint->gauge, 0);
		
	pos.unit = ~0ul;
	
	for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++) {
		int64_t delta;
		reiser4_coord_t coord;
		reiser4_ptr_hint_t ptr;

		if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		if (reiser4_item_extent(&coord)) {
			for (pos.unit = 0; pos.unit < reiser4_item_count(&coord); pos.unit++) {
				if (plugin_call(continue, coord.entity.plugin->item_ops,
						fetch, &coord.entity, pos.unit, &ptr, 1))
					return -1;
				
				frag_hint->curr = ptr.ptr + ptr.width;
				frag_hint->total += ptr.width;
			}
			
			frag_hint->bad += reiser4_item_count(&coord);
		} else {
			if (plugin_call(continue, coord.entity.plugin->item_ops,
					fetch, &coord.entity, pos.unit, &ptr, 1))
				return -1;

			delta = frag_hint->curr - ptr.ptr;

			if (ptr.ptr == 0)
				continue;
		
			if (labs(delta) > 1)
				frag_hint->bad++;

			frag_hint->total++;

			frag_hint->curr = ptr.ptr;
		}
	}
	
	return 0;
}

static errno_t callback_setup_frag(reiser4_coord_t *coord, void *data) {
	struct tree_frag_hint *frag_hint = (struct tree_frag_hint *)data;
    
	aal_assert("vpf-508", frag_hint != NULL, return -1);

	frag_hint->level--;

	return 0;
}

static errno_t callback_update_frag(reiser4_coord_t *coord, void *data) {
	struct tree_frag_hint *frag_hint = (struct tree_frag_hint *)data;
    
	aal_assert("vpf-509", frag_hint != NULL, return -1);

	frag_hint->level++;

	return 0;
}

static errno_t debugfs_tree_frag(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	reiser4_joint_t *root;
	
	struct tree_frag_hint frag_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Tree fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	root = fs->tree->root;

	frag_hint.internals = 0;
	frag_hint.bad = 0;
	frag_hint.total = 0;
	frag_hint.gauge = gauge;
	frag_hint.tree = fs->tree;
	frag_hint.curr = root->node->blk;
	frag_hint.level = plugin_call(return -1, 
	    fs->tree->root->node->entity->plugin->node_ops, get_level, 
	    fs->tree->root->node->entity);

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.data = (void *)&frag_hint;
	hint.objects = 1 << NODEPTR_ITEM;
	hint.cleanup = 1;

	aal_gauge_start(gauge);
	
	reiser4_joint_traverse(fs->tree->root, &hint, frag_open_joint,
			       callback_tree_frag, callback_setup_frag, 
			       callback_update_frag, NULL);

	aal_gauge_free(gauge);

	progs_wipe_line(stdout);
	
	printf("Tree fragmentation:\t%.5f\n", frag_hint.total > 0 ?
	       (double)frag_hint.bad / frag_hint.total : 0);

	printf("Internal nodes:\t\t%llu\n", frag_hint.internals);
	
	return 0;
};

struct node_pack_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	double formatted_used;
	double leaves_used;
	double internals_used;

	count_t nodes;
	count_t leaves;
	count_t internals;
	count_t formatted;
};

static errno_t callback_node_packing(
	reiser4_joint_t *joint,	    /* joint to be inspected */
	void *data)		    /* traverse data */
{
	uint8_t level;
	uint32_t formatted_used;
	uint32_t leaves_used;
	uint32_t internals_used;
	aal_device_t *device;
	reiser4_node_t *node = joint->node;
	
	struct node_pack_hint *pack_hint =
		(struct node_pack_hint *)data;

	level = plugin_call(return -1, node->entity->plugin->node_ops,
			    get_level, node->entity);

	if (pack_hint->formatted % 128 == 0)
		aal_gauge_update(pack_hint->gauge, 0);

	device = joint->node->device;
	formatted_used = aal_device_get_bs(device) - reiser4_node_space(joint->node);

	pack_hint->formatted_used = (formatted_used + (pack_hint->formatted_used * pack_hint->formatted)) /
		(pack_hint->formatted + 1);

	if (level > LEAF_LEVEL) {
		uint32_t count;
		item_entity_t *item;
		reiser4_coord_t coord;
		reiser4_pos_t pos = {~0ul, ~0ul};
		
		internals_used = aal_device_get_bs(device) -
			reiser4_node_space(joint->node);
		
		pack_hint->internals_used =
			(internals_used + (pack_hint->internals_used * pack_hint->internals)) /
			(pack_hint->internals + 1);

		for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++) {
			reiser4_coord_t coord;

			if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    pos.item, node->blk);
				return -1;
			}

			if (!reiser4_item_extent(&coord))
				continue;

			item = &coord.entity;
				
			count = plugin_call(return -1, item->plugin->item_ops,
					    count, item);

			for (pos.unit = 0; pos.unit < count; pos.unit++) {
				reiser4_ptr_hint_t ptr;
				
				if (plugin_call(return -1, item->plugin->item_ops, fetch, item, 
						pos.unit, &ptr, 1))
					return -1;

				pack_hint->nodes += ptr.width;
			}
		}
	} else {
		leaves_used = aal_device_get_bs(device) -
			reiser4_node_space(joint->node);

		pack_hint->leaves_used =
			(leaves_used + (pack_hint->leaves_used * pack_hint->leaves)) /
			(pack_hint->leaves + 1);
	}
	
	if (level > LEAF_LEVEL)
		pack_hint->internals++;
	else
		pack_hint->leaves++;
		
	pack_hint->formatted++;
	pack_hint->nodes++;
	
	return 0;
}

static errno_t debugfs_node_packing(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	struct node_pack_hint pack_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Node packing",
				       progs_gauge_handler, NULL)))
		return -1;
	
	aal_memset(&pack_hint, 0, sizeof(pack_hint));

	pack_hint.tree = fs->tree;
	pack_hint.gauge = gauge;

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.data = (void *)&pack_hint;
	hint.objects = 1 << NODEPTR_ITEM;
	hint.cleanup = 1;

	aal_gauge_start(gauge);
	
	reiser4_joint_traverse(fs->tree->root, &hint, common_open_joint,
			       callback_node_packing, NULL, NULL, NULL);

	aal_gauge_free(gauge);

	progs_wipe_line(stdout);

	printf("Formatted packing:\t%.2f\n", pack_hint.formatted_used);
	printf("Leaves packing:\t\t%.2f\n", pack_hint.leaves_used);
	printf("Internals packing:\t%.2f\n\n", pack_hint.internals_used);

	printf("Total nodes:\t\t%llu\n", pack_hint.nodes);
	printf("Formatted nodes:\t%llu\n", pack_hint.formatted);
	printf("Leaf nodes:\t\t%llu\n", pack_hint.leaves);
	printf("Internal nodes:\t\t%llu\n", pack_hint.internals);
	
	return 0;
}

struct file_frag_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;
	behav_flags_t flags;

	blk_t curr;
	
	count_t fs_total, fs_bad;
	count_t fl_total, fl_bad;
	uint16_t level;
};

static errno_t callback_file_frag(
	object_entity_t *entity,   /* file to be inspected */
	blk_t blk,                 /* next file block */
	void *data)                /* user-specified data */
{
	int64_t delta;

	struct file_frag_hint *hint =
		(struct file_frag_hint *)data;

	if (hint->curr == 0) {
		hint->curr = blk;
		return 0;
	}
	
	delta = hint->curr - blk;

	if (labs(delta) > 1) {
		hint->fs_bad++;
		hint->fl_bad++;
	}
	
	hint->fs_total++;
	hint->fl_total++;
	hint->curr = blk;

	return 0;
}

static errno_t debugfs_file_frag(reiser4_fs_t *fs, char *filename) {
	aal_gauge_t *gauge;
	reiser4_file_t *file;
	struct file_frag_hint frag_hint;

	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "",
				       progs_gauge_handler, NULL)))
		goto error_free_file;
	
	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	frag_hint.tree = fs->tree;
	frag_hint.gauge = gauge;

	aal_gauge_rename(gauge, "Fragmentation for %s is", filename);
	aal_gauge_start(gauge);
	
	if (reiser4_file_layout(file, callback_file_frag, &frag_hint)) {
		aal_exception_error("Can't enumerate blocks occupied by %s",
				    filename);
		goto error_free_gauge;
	}
	
	aal_gauge_free(gauge);

	printf("%.5f\n", frag_hint.fl_total > 0 ?
	       (double)frag_hint.fl_bad / frag_hint.fl_total : 0);

	reiser4_file_close(file);
	
	return 0;

 error_free_gauge:
	aal_gauge_free(gauge);
 error_free_file:
	reiser4_file_close(file);
	return -1;
}

static errno_t callback_data_frag(
	reiser4_joint_t *joint,  /* node to be inspected */
	void *data)   /* traverse hint */
{
	reiser4_pos_t pos;
	reiser4_node_t *node = joint->node;
	static int bogus = 0;

	struct file_frag_hint *frag_hint =
		(struct file_frag_hint *)data;

	if (frag_hint->level > LEAF_LEVEL)
		return 0;
	
	pos.unit = ~0ul;

	for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++) {
		reiser4_file_t *file;
		reiser4_coord_t coord;

		if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		if (!reiser4_item_statdata(&coord))
			continue;

		if (!(file = reiser4_file_begin(frag_hint->tree->fs, &coord)))
			continue;

		frag_hint->curr = 0;
		frag_hint->fl_bad = 0;
		frag_hint->fl_total = 0;

		if (bogus++ % 16 == 0)
			aal_gauge_update(frag_hint->gauge, 0);

		bogus %= 16;
	
		if (reiser4_file_layout(file, callback_file_frag, data)) {
			aal_exception_error("Can't enumerate blocks occupied by %s",
					    file->name);
			
			reiser4_file_close(file);
			continue;
		}

		if (frag_hint->flags & BF_SEACH) {
			double factor = frag_hint->fl_total > 0 ?
				(double)frag_hint->fl_bad / frag_hint->fl_total : 0;
			
			aal_exception_info("Fragmentation for %s: %.5f",
					   file->name, factor);
		}
		
		reiser4_file_close(file);
	}
	
	return 0;
}

static errno_t debugfs_data_frag(reiser4_fs_t *fs, behav_flags_t flags) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	struct file_frag_hint frag_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Data fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	aal_memset(&frag_hint, 0, sizeof(frag_hint));

	frag_hint.tree = fs->tree;
	frag_hint.gauge = gauge;
	frag_hint.flags = flags;

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.data = (void *)&frag_hint;
	hint.objects = 1 << NODEPTR_ITEM;
	hint.cleanup = 1;

	aal_gauge_start(gauge);
	
	reiser4_joint_traverse(fs->tree->root, &hint, common_open_joint,
			       callback_data_frag, callback_setup_frag, 
			       callback_update_frag, NULL);

	aal_gauge_free(gauge);

	printf("%.5f\n", frag_hint.fs_total > 0 ?
	       (double)frag_hint.fs_bad / frag_hint.fs_total : 0);
	
	return 0;
}

static errno_t debugfs_file_cat(reiser4_file_t *file) {
	char buff[4096];
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (!reiser4_file_read(file, buff, sizeof(buff)))
			break;

		write(1, buff, sizeof(buff));
	}

	return 0;
}

static errno_t debugfs_file_ls(reiser4_file_t *file) {
	reiser4_entry_hint_t entry;
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (reiser4_file_read(file, &entry, 1)) {
		printf("[%llx:%llx] %s\n", (entry.objid.locality >> 4), 
		       entry.objid.objectid, entry.name);
	}
	
	return 0;
}

static errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = 0;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	if (file->entity->plugin->h.sign.group == REGULAR_FILE)
		res = debugfs_file_cat(file);
	else if (file->entity->plugin->h.sign.group == DIRTORY_FILE) {
		res = debugfs_file_ls(file);
	} else {
		aal_exception_info("Sorry, browing special files and symlinks "
				   "is not implemented yet.");
	}
	
	reiser4_file_close(file);
	return res;
}

static errno_t debugfs_print_block(reiser4_fs_t *fs, blk_t blk,
				   print_flags_t flags)
{
	errno_t res = 0;
	reiser4_joint_t *joint;
	struct traverse_hint hint;
	struct tree_print_hint print_hint;

	if (!reiser4_alloc_test(fs->alloc, blk)) {
		aal_exception_info("Block %llu is not belong to "
				   "filesystem.", blk);
		return 0;
	}
		
	switch (reiser4_format_belongs(fs->format, blk)) {
	case RB_SKIPPED:
		aal_exception_info("Block %llu belongs to skipped area.", blk);
		return 0;
	case RB_FORMAT:
		aal_exception_info("Sorry, printing format area blocks is not "
				   "implemented yet!");
		return 0;
	case RB_JOURNAL:
		aal_exception_info("Sorry, printing journal area blocks is not "
				   "implemented yet!");
		return 0;
	case RB_ALLOC:
		aal_exception_info("Sorry, printing block allocator blocks is not "
				   "implemented yet!");
		return 0;
	default:
		break;
	}
	
	aal_exception_disable();
	
	if (!(joint = reiser4_tree_load(fs->tree, blk))) {
		aal_exception_enable();
		aal_exception_info("Node %llu is not a formated node.", blk);
		return 0;
	}

	aal_exception_enable();
	
	print_hint.tree = fs->tree;
	print_hint.flags = flags;

	hint.data = &print_hint;
		
	res = debugfs_print_joint(joint, &hint);
	reiser4_joint_close(joint);
	
	return res;
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	print_flags_t print_flags = 0;
	behav_flags_t behav_flags = 0;
    
	char *host_dev;
	char *ls_filename = NULL;
	char *cat_filename = NULL;
	char *frag_filename = NULL;
	char *profile_label = "smart40";
    
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	blk_t blocknr = 0;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"profile", required_argument, NULL, 'e'},
		{"force", no_argument, NULL, 'f'},
		{"ls", required_argument, NULL, 'l'},
		{"cat", required_argument, NULL, 'c'},
		{"print-items", no_argument, NULL, 'i'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-block-alloc", no_argument, NULL, 'b'},
		{"print-oid-alloc", no_argument, NULL, 'o'},
		{"print-block", required_argument, NULL, 'n'},
		{"node-packing", no_argument, NULL, 'N'},
		{"tree-frag", no_argument, NULL, 'T'},
		{"file-frag", required_argument, NULL, 'F'},
		{"data-frag", no_argument, NULL, 'D'},
		{"show-each", no_argument, NULL, 'p'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"quiet", no_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	debugfs_init();

	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVe:qfKstbiojTDpNF:c:l:n:",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			progs_print_banner(argv[0]);
			return NO_ERROR;
		case 'e':
			profile_label = optarg;
			break;
		case 'o':
			print_flags |= PF_OID;
			break;
		case 'b':
			print_flags |= PF_ALLOC;
			break;
		case 's':
			print_flags |= PF_SUPER;
			break;
		case 'j':
			print_flags |= PF_JOURNAL;
			break;
		case 'i':
			print_flags |= PF_ITEMS;
			break;
		case 't':
			print_flags |= PF_TREE;
			break;
		case 'n': {
			int error;
			
			print_flags |= PF_BLOCK;
			
			if (!(blocknr = aux_strtol(optarg, &error)) && error) {
				aal_exception_error("Invalid block number (%s).", optarg);
				return USER_ERROR;
			}
			
			break;
		}
		case 'N':
			behav_flags |= BF_NPACK;
			break;
		case 'T':
			behav_flags |= BF_TFRAG;
			break;
		case 'D':
			behav_flags |= BF_DFRAG;
			break;
		case 'p':
			behav_flags |= BF_SEACH;
			break;
		case 'c':
			behav_flags |= BF_CAT;
			cat_filename = optarg;
			break;
		case 'l':
			behav_flags |= BF_LS;
			ls_filename = optarg;
			break;
		case 'F':
			behav_flags |= BF_FFRAG;
			frag_filename = optarg;
			break;
		case 'f':
			behav_flags |= BF_FORCE;
			break;
		case 'q':
			behav_flags |= BF_QUIET;
			break;
		case 'K':
			progs_print_banner(argv[0]);
			progs_profile_list();
			return NO_ERROR;
		case '?':
			debugfs_print_usage(argv[0]);
			return USER_ERROR;
		}
	}
    
	if (optind >= argc) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	progs_print_banner(argv[0]);
    
	/* Initializing passed profile */
	if (!(profile = progs_profile_find(profile_label))) {
		aal_exception_error("Can't find profile by its label %s.", 
				    profile_label);
		goto error;
	}
    
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* 
	   Checking is passed device is a block device. If so, we check also is
	   it whole drive or just a partition. If the device is not a block
	   device, then we emmit exception and propose user to use -f flag to
	   force.
	*/
	if (!S_ISBLK(st.st_mode)) {
		if (!(behav_flags & BF_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    (!(behav_flags & BF_FORCE)))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", host_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (progs_dev_mounted(host_dev, NULL) && !(behav_flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted at the moment. "
				    "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device */
	if (!(device = aal_file_open(host_dev, DEFAULT_BLOCKSIZE, O_RDONLY))) {
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, device, 0))) {
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}
    
	if (!aal_pow_of_two(print_flags) && !(behav_flags & BF_QUIET)) {
		if (!(print_flags & PF_ITEMS)) {
			if (aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_YESNO,
						"Ambiguous print options has been detected. "
						"Continue?") == EXCEPTION_NO)
				goto error_free_fs;
		}
	}

	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;
		
	if (!(print_flags & PF_TREE) && !(print_flags & PF_BLOCK) &&
	    (print_flags & PF_ITEMS))
	{
		aal_exception_warn("Option --print-items is only active if "
				   "--print-tree is specified.");
	}

	if (!(behav_flags & BF_DFRAG) && (behav_flags & BF_SEACH)) {
		aal_exception_warn("Option --show-each is only active if "
				   "--data-frag is specified.");
	}

	if (behav_flags & BF_TFRAG || behav_flags & BF_DFRAG ||
	    behav_flags & BF_FFRAG || behav_flags & BF_NPACK)
	{
		if (behav_flags & BF_QUIET ||
		    aal_exception_yesno("This operation may take long time. "
					"Continue?") == EXCEPTION_YES)
		{
			if (behav_flags & BF_TFRAG) {
				if (debugfs_tree_frag(fs))
					goto error_free_fs;
			}

			if (behav_flags & BF_DFRAG) {
				if (debugfs_data_frag(fs, behav_flags))
					goto error_free_fs;
			}

			if (behav_flags & BF_FFRAG) {
				if (debugfs_file_frag(fs, frag_filename))
					goto error_free_fs;
			}
	
			if (behav_flags & BF_NPACK) {
				if (debugfs_node_packing(fs))
					goto error_free_fs;
			}
		}
	}
	
	if ((behav_flags & BF_LS)) {
		if (debugfs_browse(fs, ls_filename))
			goto error_free_fs;
	}
	
	if ((behav_flags & BF_CAT)) {
		if (debugfs_browse(fs, cat_filename))
			goto error_free_fs;
	}
	
	if (print_flags & PF_SUPER) {
		if (debugfs_print_master(fs))
			goto error_free_fs;
	
		if (debugfs_print_format(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_OID) {
		if (debugfs_print_oid(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_ALLOC) {
		if (debugfs_print_alloc(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_JOURNAL) {
		if (debugfs_print_journal(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_TREE) {
		if (debugfs_print_tree(fs, print_flags))
			goto error_free_fs;
	}

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr, print_flags))
			goto error_free_fs;
	}
    
	/* Deinitializing filesystem instance and device instance */
	reiser4_fs_close(fs);
	aal_file_close(device);
    
	/* 
	   Deinitializing libreiser4. At the moment only plugins are unloading 
	   durrign this.
	*/
	libreiser4_done();
    
	return NO_ERROR;

 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_file_close(device);
 error_free_libreiser4:
	libreiser4_done();
 error:
	return OPER_ERROR;
}

