/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   measurefs.c -- program for measuring reiser4. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <misc/misc.h>
#include <reiser4/reiser4.h>

enum behav_flags {
	BF_FORCE    = 1 << 0,
	BF_QUIET    = 1 << 1,
	BF_TFRAG    = 1 << 2,
	BF_FFRAG    = 1 << 3,
	BF_TSTAT    = 1 << 4,
	BF_DFRAG    = 1 << 5,
	BF_SFILE    = 1 << 6,
	BF_PLUGS    = 1 << 7
};

typedef enum behav_flags behav_flags_t;

/* Prints measurefs options */
static void measurefs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces using filesystem without\n"
		"                                  any questions.\n"
		"  -f, --force                     makes debugfs to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"Measurement options:\n"
		"  -S, --tree-stat                 measures some tree characteristics\n"
		"                                  (node packing, etc).\n"
		"  -T, --tree-frag                 measures tree fragmentation.\n"
		"  -F, --file-frag FILE            measures fragmentation of specified\n"
		"                                  file or directory.\n"
		"  -D, --data-frag                 measures average files fragmentation.\n"
		"  -E, --show-file                 show file fragmentation for each file\n"
		"                                  durring calclulation if --data-frag is\n"
		"                                  specified.\n"
		"Plugins options:\n"
		"  -P, --known-plugins             prints known plugins.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes exception streams used by measurefs */
static void measurefs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		misc_exception_set_stream(ex, stderr);
}

struct tfrag_hint {
	aal_gauge_t *gauge;

	blk_t curr;
	uint16_t level;
	count_t total, bad;
};

typedef struct tfrag_hint tfrag_hint_t;

/* Open node callback for calculating the tree fragmentation */
static reiser4_node_t *tfrag_open_node(
	reiser4_tree_t *tree,	    /* tree being traveresed */
	reiser4_place_t *place,     /* place to read the blk number from */
	void *data)		    /* traverse hint */
{
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;
	reiser4_node_t *node;

	aal_assert("umka-1556", frag_hint->level > 0);
	
	/* As we do not need traverse leaf level at all, we going out here */
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	node = reiser4_tree_child(tree, place);
	
	return node == NULL ? INVAL_PTR : node;
}

static errno_t tfrag_process_item(
	void *entity,               /* item we traverse now */
	uint64_t start,             /* region start */
	uint64_t count,             /* region width */
	void *data)                 /* one of blk item points to */
{
	int64_t delta;
	tfrag_hint_t *hint;
	
	hint = (tfrag_hint_t *)data;
	
	if (start == 0)
		return 0;

	delta = hint->curr - start;
				
	if (labs(delta) > 1)
		hint->bad++;
				
	hint->total++;
	hint->curr = start + count - 1;

	return 0;
}

/* Traverse passed leaf @node and calculate fragmentation for it. The results
   are stored in frag_hint structure. This function is called from the tree
   traversal routine for each internal node. See bellow for details. */
static errno_t tfrag_process_node(
	reiser4_tree_t *tree,	   /* tree being traversed */
	reiser4_node_t *node,	   /* node to be estimated */
	void *data)	           /* user-specified data */
{
	pos_t pos;
	tfrag_hint_t *frag_hint;

	frag_hint = (tfrag_hint_t *)data;

	if (frag_hint->gauge)
		aal_gauge_update(frag_hint->gauge, 0);
		
	pos.unit = MAX_UINT32;

	/* Loop though the node items */
	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		reiser4_place_t place;

		/* Initializing item at @place */
		if (reiser4_place_open(&place, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->number);
			return -EINVAL;
		}

		/* Checking and calling item's layout method with function
		   tfrag_process_item() as a function for handling one block the
		   item points to. */
		if (!place.plug->o.item_ops->layout) {
			aal_exception_warn("Item %u in node %llu has not "
					   "\"layout\" method implemented. "
					   "The result will not be releable.",
					   pos.item, node->number);
			continue;
		}

		plug_call(place.plug->o.item_ops, layout, (place_t *)&place,
			  tfrag_process_item, data);
	}
	
	frag_hint->level--;
	return 0;
}

static errno_t tfrag_update_node(reiser4_tree_t *tree, 
				 reiser4_place_t *place, 
				 void *data) 
{
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

/* Entry point for calculating tree fragmentation. It zeroes out all counters in
   structure which wiil be passed to actual routines and calls tree_traverse
   function with couple of callbacks for handling all traverse cases (open node,
   traverse node, etc). Actual statistics collecting is performed in the passed
   callbacks and subcallbacks (for item traversing). */
errno_t measurefs_tree_frag(reiser4_fs_t *fs, uint32_t flags) {
	errno_t res;
	tfrag_hint_t frag_hint;

	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	if (!(flags & BF_QUIET)) {
		/* Initializing gauge, because it is a long process and user
		   should be informed what the stage of the process is going at
		   the moment. */
		if (!(frag_hint.gauge = aal_gauge_create(GAUGE_INDICATOR,
							 NULL)))
		{
			aal_exception_fatal("Out of memory!");
			return -ENOMEM;
		}

		aal_gauge_rename(frag_hint.gauge, "Tree fragmentation");
	}
	
	/* Preparing serve structure, statistics will be stored in  */
	frag_hint.curr = reiser4_tree_root(fs->tree);
	frag_hint.level = reiser4_tree_height(fs->tree);
	
	if (frag_hint.gauge)
		aal_gauge_start(frag_hint.gauge);

	/* Calling tree traversal */
	if ((res = reiser4_tree_traverse(fs->tree, tfrag_open_node,
					 tfrag_process_node, tfrag_update_node,
					 NULL, &frag_hint)))
		return res;

	if (frag_hint.gauge)
		aal_gauge_free(frag_hint.gauge);
	else
		printf("Tree fragmentation: ");

	/* Printing the result */
	printf("%.6f\n", frag_hint.total > 0 ?
	       (double)frag_hint.bad / frag_hint.total : 0);
	
	return 0;
};

struct tstat_hint {
	aal_gauge_t *gauge;

	double leaves_used;
	double formatted_used;
	double internals_used;

	count_t nodes;
	count_t leaves;
	count_t twigs;
	count_t internals;
	count_t formatted;
};

typedef struct tstat_hint tstat_hint_t;

/* Process one block belong to the item (extent or nodeptr) */
static errno_t stat_process_item(
	void *entity,		    /* item we traverse now */
	uint64_t start,             /* region start */
	uint64_t count,             /* region count */
	void *data)                 /* one of blk item points to */
{
	tstat_hint_t *stat_hint = (tstat_hint_t *)data;
	stat_hint->nodes += count;

	return 0;
}

/* Processing one formatted node */
static errno_t stat_process_node(
	reiser4_tree_t *tree,	    /* tree being traversed */
	reiser4_node_t *node,	    /* node to be inspected */
	void *data)		    /* traverse data */
{
	uint8_t level;
	aal_device_t *device;
	uint32_t leaves_used;
	uint32_t formatted_used;
	uint32_t internals_used;

	tstat_hint_t *stat_hint = (tstat_hint_t *)data;

	level = reiser4_node_get_level(node);

	if (stat_hint->gauge && stat_hint->formatted % 128 == 0)
		aal_gauge_update(stat_hint->gauge, 0);

	device = node->device;
	
	formatted_used = aal_device_get_bs(device) -
		reiser4_node_space(node);

	stat_hint->formatted_used = formatted_used +
		(stat_hint->formatted_used * stat_hint->formatted);

	stat_hint->formatted_used /= (stat_hint->formatted + 1);

	/* If we are on the level higher taht leaf level, we traverse extents on
	   it. Otherwise we just update stat structure. */
	if (level > LEAF_LEVEL) {
		pos_t pos = {MAX_UINT32, MAX_UINT32};
		
		internals_used = aal_device_get_bs(device) -
			reiser4_node_space(node);
		
		stat_hint->internals_used = internals_used +
			(stat_hint->internals_used * stat_hint->internals);

		stat_hint->internals_used /= (stat_hint->internals + 1);

		for (pos.item = 0; pos.item < reiser4_node_items(node);
		     pos.item++)
		{
			errno_t res;
			reiser4_place_t place;
			
			if ((res = reiser4_place_open(&place, node, &pos))) {
				aal_exception_error("Can't open item %u in node "
						    "%llu.", pos.item, node->number);
				return res;
			}
			
			if (!place.plug->o.item_ops->layout)
				continue;

			plug_call(place.plug->o.item_ops, layout,
				  (place_t *)&place, stat_process_item, data);
		}
	} else {
		leaves_used = aal_device_get_bs(device) -
			reiser4_node_space(node);

		stat_hint->leaves_used = leaves_used +
			(stat_hint->leaves_used * stat_hint->leaves);
		
		stat_hint->leaves_used /= (stat_hint->leaves + 1);
	}
	
	stat_hint->leaves += (level == LEAF_LEVEL);
	stat_hint->twigs += (level == TWIG_LEVEL);
	stat_hint->internals += (level > LEAF_LEVEL);

	stat_hint->nodes++;
	stat_hint->formatted++;

	return 0;
}

/* Entry point function for calculating tree statistics */
errno_t measurefs_tree_stat(reiser4_fs_t *fs, uint32_t flags) {
	errno_t res;
	tstat_hint_t stat_hint;

	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	
	if (!(flags & BF_QUIET)) {
		if (!(stat_hint.gauge = aal_gauge_create(GAUGE_INDICATOR,
							 NULL)))
		{
			aal_exception_fatal("Out of memory!");
			return -ENOMEM;
		}

		aal_gauge_rename(stat_hint.gauge, "Tree statistics");
	}

	if (stat_hint.gauge)
		aal_gauge_start(stat_hint.gauge);
	
	if ((res = reiser4_tree_traverse(fs->tree, NULL, stat_process_node, 
					 NULL, NULL, &stat_hint)))
		return res;

	if (stat_hint.gauge) {
		aal_gauge_free(stat_hint.gauge);
		misc_wipe_line(stdout);
	}

	/* Printing results */
	printf("Formatted packing:%*.2f\n", 10, stat_hint.formatted_used);
	printf("Leaves packing:%*.2f\n", 13, stat_hint.leaves_used);
	printf("Internals packing:%*.2f\n\n", 10, stat_hint.internals_used);

	printf("Total nodes:%*llu\n", 16, stat_hint.nodes);
	printf("Formatted nodes:%*llu\n", 12, stat_hint.formatted);
	printf("Leaf nodes:%*llu\n", 17, stat_hint.leaves);
	printf("Twig nodes:%*llu\n", 17, stat_hint.twigs);
	printf("Internal nodes:%*llu\n", 13, stat_hint.internals);
	
	return 0;
}

struct ffrag_hint {
	aal_gauge_t *gauge;

	count_t bad;
	count_t total;

	blk_t last;
	count_t files;
	
	double current;
	uint32_t flags;
	uint16_t level;
};

typedef struct ffrag_hint ffrag_hint_t;

/* Callback function for processing one block belong to the fiel we are
   traversing. */
static errno_t ffrag_process_blk(
	object_entity_t *entity,   /* file to be inspected */
	blk_t blk,                 /* next file block */
	void *data)                /* user-specified data */
{
	int64_t delta;
	ffrag_hint_t *frag_hint;

	frag_hint = (ffrag_hint_t *)data;
	
	/* Check if we are went here first time */
	if (frag_hint->last > 0) {
		delta = frag_hint->last - blk;

		if (labs(delta) > 1)
			frag_hint->bad++;

		frag_hint->total++;
	}

	frag_hint->last = blk;
	return 0;
}

/* Calculates the passed file fragmentation */
errno_t measurefs_file_frag(reiser4_fs_t *fs,
			    char *filename,
			    uint32_t gauge)
{
	errno_t res = 0;
	ffrag_hint_t frag_hint;
	reiser4_object_t *object;

	/* Opens object by its name */
	if (!(object = reiser4_object_open(fs->tree, filename, FALSE)))
		return -EINVAL;

	/* Initializing serve structures */
	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	/* Calling file layout function, which calls ffrag_process_blk()
	   fucntion for each block belong to the file @filename. */
	if ((res = reiser4_object_layout(object, ffrag_process_blk,
					 &frag_hint)))
	{
		aal_exception_error("Can't enumerate data blocks "
				    "occupied by %s", filename);
		goto error_free_object;
	}
	
	reiser4_object_close(object);

	/* Printing results */
	printf("Fragmentation for %s is %.6f\n", filename,
	       frag_hint.total > 0 ? (double)frag_hint.bad /
	       frag_hint.total : 0);
	
	return 0;

 error_free_object:
	reiser4_object_close(object);
	return res;
}

/* Processes leaf node in order to find all the stat data items which denote
   corresponding files and calculate file fragmentation for each of them. */
static errno_t dfrag_process_node(
	reiser4_tree_t *tree,	    /* tree being traversed */
	reiser4_node_t *node,       /* node to be inspected */
	void *data)                 /* traverse hint */
{
	pos_t pos;
	static int bogus = 0;
	ffrag_hint_t *frag_hint;

	pos.unit = MAX_UINT32;
	frag_hint = (ffrag_hint_t *)data;

	frag_hint->level--;
	
	if (frag_hint->level > LEAF_LEVEL)
		return 0;
	
	/* The loop though the all items in current node */
	for (pos.item = 0; pos.item < reiser4_node_items(node);
	     pos.item++)
	{
		errno_t res;
		reiser4_place_t place;
		reiser4_object_t *object;

		/* Initialiing the item at @place */
		if ((res = reiser4_place_init(&place, node, &pos))) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->number);
			return res;
		}

		/* If the item is not a stat data item, we getting to the next
		   circle of the loop, because we are intersted only in the stat
		   data items, which can be used for opening files. */
		if (!reiser4_object_begin(&place))
			continue;

		/* Opening object by its stat data item denoded by @place */
		if (!(object = reiser4_object_realize(tree, &place)))
			continue;

		/* Initializing per-file counters */
		frag_hint->bad = 0;
		frag_hint->last = 0;
		frag_hint->total = 0;

		if (frag_hint->gauge && bogus++ % 16 == 0)
			aal_gauge_update(frag_hint->gauge, 0);

		bogus %= 16;

		/* Calling calculating the file fragmentation by emans of using
		   the function we have seen abowe. */
		if (reiser4_object_layout(object, ffrag_process_blk, data)) {
			aal_exception_error("Can't enumerate data blocks "
					    "occupied by %s", object->name);
			goto error_close_object;
		}

		if (frag_hint->total > 0) {
			frag_hint->current += (double)frag_hint->bad /
				frag_hint->total;
		}

		frag_hint->files++;
			
		/* We was instructed show file fragmentation for each file, not
		   only the average one, we will do it now. */
		if (frag_hint->flags & BF_SFILE) {
			double file_factor = frag_hint->total > 0 ?
				(double)frag_hint->bad / frag_hint->total : 0;
			
			double curr_factor = frag_hint->files > 0 ?
				(double)frag_hint->current / frag_hint->files : 0;
			
			aal_exception_info("Fragmentation for %s: %.6f [ %.6f ]",
					   object->name, file_factor, curr_factor);
		}

	error_close_object:
		reiser4_object_close(object);
	}
	
	return 0;
}

static errno_t dfrag_update_node(reiser4_tree_t *tree, 
				 reiser4_place_t *place,
				 void *data)
{
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

/* Entry point function for data fragmentation */
errno_t measurefs_data_frag(reiser4_fs_t *fs,
			    uint32_t flags)
{
	errno_t res;
	ffrag_hint_t frag_hint;

	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	if (!(flags & BF_QUIET)) {
		if (!(frag_hint.gauge = aal_gauge_create(GAUGE_INDICATOR,
							 NULL)))
		{
			aal_exception_fatal("Out of memory!");
			return -ENOMEM;
		}

		aal_gauge_rename(frag_hint.gauge, "Data fragmentation");
	}

	frag_hint.flags = flags;
	frag_hint.level = reiser4_tree_height(fs->tree);
	
	if (frag_hint.gauge)
		aal_gauge_start(frag_hint.gauge);
	
	if ((res = reiser4_tree_traverse(fs->tree, NULL, dfrag_process_node, 
					 dfrag_update_node, NULL, &frag_hint)))
		return res;

	if (frag_hint.gauge)
		aal_gauge_free(frag_hint.gauge);

	if (frag_hint.flags & BF_SFILE || !frag_hint.gauge)
		printf("Data fragmentation is: ");
	
	printf("%.6f\n", frag_hint.files > 0 ?
	       (double)frag_hint.current / frag_hint.files : 0);
	
	return 0;
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;

	uint32_t flags = 0;
	char override[4096];

	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;
	char *frag_filename = NULL;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"tree-stat", no_argument, NULL, 'S'},
		{"tree-frag", no_argument, NULL, 'T'},
		{"file-frag", required_argument, NULL, 'F'},
		{"data-frag", no_argument, NULL, 'D'},
		{"show-file", no_argument, NULL, 'E'},
		{"known-plugins", no_argument, NULL, 'P'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};

	measurefs_init();

	if (argc < 2) {
		measurefs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVqfKTDESF:o:",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			measurefs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'S':
			flags |= BF_TSTAT;
			break;
		case 'T':
			flags |= BF_TFRAG;
			break;
		case 'D':
			flags |= BF_DFRAG;
			break;
		case 'E':
			flags |= BF_SFILE;
			break;
		case 'F':
			flags |= BF_FFRAG;
			frag_filename = optarg;
			break;
		case 'f':
			flags |= BF_FORCE;
			break;
		case 'q':
			flags |= BF_QUIET;
			break;
		case 'P':
			flags |= BF_PLUGS;
			break;
		case 'o':
			aal_strncat(override, optarg,
				    aal_strlen(optarg));
			
			aal_strncat(override, ",", 1);
			break;
		case '?':
			measurefs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
    
	if (optind >= argc) {
		measurefs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	if (!(flags & BF_QUIET))
		misc_print_banner(argv[0]);

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Initializing passed profile */
	profile = misc_profile_default();
    
	/* Overriding profile by passed by used values. This should be done
	   after libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';
		
		if (!(flags & BF_QUIET)) {
			aal_exception_info("Overriding default profile "
					   "by \"%s\".", override);
		}
		
		if (misc_profile_override(override))
			goto error_free_libreiser4;
	}
	
	if (flags & BF_PLUGS) {
		misc_profile_print();
		libreiser4_fini();
		return 0;
	}
	
	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* Checking if passed device is a block one. If so, we check also is it
	   whole drive or just a partition. If the device is not a block device,
	   then we emmit exception and propose user to use -f flag to force. */
	if (!S_ISBLK(st.st_mode)) {
		if (!(flags & BF_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    (!(flags & BF_FORCE)))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", host_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(host_dev, NULL) && !(flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted at the moment. "
				    "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev,
				       REISER4_SECSIZE, O_RDONLY)))
	{
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}

	/* Open file system on the device */
	if (!(fs = reiser4_fs_open(device, profile))) {
		aal_exception_error("Can't open reiser4 on %s",
				    host_dev);
		goto error_free_libreiser4;
	}

	/* Initializing tree and tree's traps */
	if (!(fs->tree = reiser4_tree_init(fs, misc_mpressure_detect)))
		goto error_free_fs;
    
	/* Check if specified options are compatible. For instance, --show-each
	   can be used only if --data-frag was specified. */
	if (!(flags & BF_DFRAG) && (flags & BF_SFILE)) {
		aal_exception_warn("Option --show-file is only active if "
				   "--data-frag is specified.");
	}

	if (!(flags & BF_TFRAG || flags & BF_DFRAG ||
	      flags & BF_FFRAG || flags & BF_TSTAT))
	{
		flags |= BF_TSTAT;
	}

	/* Handling measurements options */
	if (flags & BF_TFRAG) {
		if (measurefs_tree_frag(fs, flags))
			goto error_free_tree;
	}

	if (flags & BF_DFRAG) {
		if (measurefs_data_frag(fs, flags))
			goto error_free_tree;
	}

	if (flags & BF_FFRAG) {
		if (measurefs_file_frag(fs, frag_filename,
					flags))
			goto error_free_tree;
	}
	
	if (flags & BF_TSTAT) {
		if (measurefs_tree_stat(fs, flags))
			goto error_free_tree;
	}

	/* Freeing tree */
	reiser4_tree_fini(fs->tree);
    
	/* Deinitializing filesystem instance and device instance */
	reiser4_fs_close(fs);
	aal_device_close(device);
    
	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   durring this. */
	libreiser4_fini();
	return NO_ERROR;

 error_free_tree:
	reiser4_tree_fini(fs->tree);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}
