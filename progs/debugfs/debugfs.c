/*
  debugfs.c -- program to debug reiser4 filesystem.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include <aux/aux.h>
#include <misc/misc.h>
#include <reiser4/reiser4.h>

enum debugfs_print_flags {
	PF_SUPER    = 1 << 0,
	PF_JOURNAL  = 1 << 1,
	PF_ALLOC    = 1 << 2,
	PF_OID	    = 1 << 3,
	PF_TREE	    = 1 << 4,
	PF_ITEMS    = 1 << 5
};

typedef enum debugfs_print_flags debugfs_print_flags_t;

enum debugfs_behav_flags {
	BF_FORCE    = 1 << 0,
	BF_QUIET    = 1 << 1,
	BF_TFRAG    = 1 << 2,
	BF_FFRAG    = 1 << 3
};

typedef enum debugfs_behav_flags debugfs_behav_flags_t;

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -? | -h | --help               prints program usage.\n"
		"  -V | --version                 prints current version.\n"
		"  -q | --quiet                   forces creating filesystem without\n"
		"                                 any questions.\n"
		"  -f | --force                   makes debugfs to use whole disk, not\n"
		"                                 block device or mounted partition.\n"
		"Print options:\n"
		"  -i | --print-items             forces debugfs.reiser4 to print items\n"
		"                                 content if --print-tree is specified\n"
		"  -t | --print-tree              prints the whole tree (default).\n"
		"  -j | --print-journal           prints journal.\n"
		"  -s | --print-super             prints the both super blocks.\n"
		"  -b | --print-block-alloc       prints block allocator data.\n"
		"  -o | --print-oid-alloc         prints oid allocator data.\n"
		"Measurement options:\n"
		"  -T | --total-fragmentation     measures total tree fragmentation\n"
		"  -F | --files-fragmentation     measures average file fragmentation\n"
		"Plugins options:\n"
		"  -e | --profile PROFILE         profile to be used.\n"
		"  -K | --known-profiles          prints known profiles.\n");
}

/* Initializes used by debugfs exception streams */
static void debugfs_init(void) {
	int ex;

	/* Setting up exception streams*/
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

struct print_tree_hint {
	reiser4_tree_t *tree;
	debugfs_print_flags_t flags;
};

/* Callback function used in traverse for opening the node */
static errno_t debugfs_open_joint(
	reiser4_joint_t **joint,    /* joint to be opened */
	blk_t blk, void *data)	    /* blk to pe opened and user-specified data */
{
	*joint = reiser4_tree_load(((struct print_tree_hint *)data)->tree, blk);
	return -(*joint == NULL);
}

static errno_t debugfs_print_joint(
	reiser4_joint_t *joint,	   /* joint to be printed */
	void *data)		   /* user-specified data */
{
	uint8_t level;
	char buff[8192];
	
	reiser4_node_t *node = joint->node;
	struct print_tree_hint *hint = (struct print_tree_hint *)data;

	level = plugin_call(return -1, node->entity->plugin->node_ops,
			    get_level, node->entity);

	printf("%s NODE (%llu) contains level=%u, items=%u, space=%u\n", 
	       level > LEAF_LEVEL ? "TWIG" : "LEAF", aal_block_number(node->block),
	       level, reiser4_node_count(node), reiser4_node_space(node));
    
	if (level > LEAF_LEVEL) {
		uint32_t i;
	
		for (i = 0; i < reiser4_node_count(node); i++) {
			reiser4_key_t key;
			reiser4_coord_t coord;
			reiser4_pos_t pos = {i, ~0ul};

			if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    i, aal_block_number(node->block));
				return -1;
			}

			if (reiser4_item_nodeptr(&coord))
				printf("(%u) NODEPTR: len=%u, ", i, reiser4_item_len(&coord));
			else
				printf("(%u) EXTENT: len=%u, ", i, reiser4_item_len(&coord));
	    
			if (reiser4_item_key(&coord, &key)) {
				aal_exception_error("Can't get key of item %u in node %llu.",
						    i, aal_block_number(node->block));
				return -1;
			}
	    
			aal_memset(buff, 0, sizeof(buff));
			reiser4_key_print(&key, buff, sizeof(buff));
			printf("KEY: %s, ", buff);

			printf("PLUGIN: 0x%x (%s)\n", coord.entity.plugin->h.sign.id,
			       coord.entity.plugin->h.label);

			aal_memset(buff, 0, sizeof(buff));

			if (reiser4_item_print(&coord, buff, sizeof(buff)))
				return -1;

			printf("%s\n", buff);
		}
	} else {
		uint32_t i;

		for (i = 0; i < reiser4_node_count(node); i++) {
			reiser4_key_t key;
			reiser4_coord_t coord;
			reiser4_pos_t pos = {i, ~0ul};

			if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    i, aal_block_number(node->block));
				return -1;
			}

			if (reiser4_item_key(&coord, &key)) {
				aal_exception_error("Can't get key of item %u in node %llu.",
						    i, aal_block_number(node->block));
				return -1;
			}
	    
			printf("(%u) ", i);
		    
			if (reiser4_item_statdata(&coord)) {
				printf("STATDATA");
			} else if (reiser4_item_direntry(&coord)) {
				printf("DIRENTRY");
			} else if (reiser4_item_tail(&coord)) {
				printf("TAIL");
			}
	    
			printf(": len=%u, ", reiser4_item_len(&coord));

			aal_memset(buff, 0, sizeof(buff));
			reiser4_key_print(&key, buff, sizeof(buff));
			printf("KEY: %s, ", buff);
	    
			printf("PLUGIN: 0x%x (%s)\n", coord.entity.plugin->h.sign.id,
			       coord.entity.plugin->h.label);

			if (hint->flags & PF_ITEMS) {
				aal_memset(buff, 0, sizeof(buff));
			
				if (reiser4_item_print(&coord, buff, sizeof(buff)))
					return -1;

				printf(buff);
			}
		}
	}
    
	return 0;
}

static errno_t debugfs_print_tree(reiser4_fs_t *fs, debugfs_print_flags_t flags) {
	struct print_tree_hint print_hint = {fs->tree, flags};
	traverse_hint_t traverse_hint = {TO_FORWARD, LEAF_LEVEL};
	
	reiser4_joint_traverse(fs->tree->root, &traverse_hint, (void *)&print_hint,
			       debugfs_open_joint, debugfs_print_joint, NULL, NULL, NULL, NULL);
    
	printf("\n");
    
	return 0;
}

errno_t debugfs_print_master(reiser4_fs_t *fs) {
	reiser4_master_t *master;
    
	aal_assert("umka-1299", fs != NULL, return -1);

	master = fs->master;
    
	printf("Master super block:\n");
	printf("block number:\t%llu\n", aal_block_number(master->block));
	printf("block size:\t%u\n", reiser4_master_blocksize(master));

	printf("magic:\t\t%s\n", reiser4_master_magic(master));
	printf("format:\t\t%x\n", reiser4_master_format(master));
	printf("label:\t\t%s\n", reiser4_master_label(master));

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	{
		char uuid[37];
		uuid_unparse(reiser4_master_uuid(master), uuid);
		printf("uuid:\t\t%s\n", uuid);
	}
#endif

	printf("\n");
	return 0;
}

static errno_t debugfs_print_format(reiser4_fs_t *fs) {
	char buff[4096];

	aal_memset(buff, 0, sizeof(buff));
   
	if (!fs->format->entity->plugin->format_ops.print) {
		aal_exception_info("Format print method is not implemented.");
		return 0;
	}
    
	printf("Format super block:\n");
	if (fs->format->entity->plugin->format_ops.print(fs->format->entity, 
							 buff, sizeof(buff), 0))
	{
		aal_exception_error("Can't print format specific super block.");
		return -1;
	}
    
	printf(buff);
	printf("\n");
    
	return 0;
}

static errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, block allocator print "
			    "is not implemented yet!");
	return 0;
}
   
static errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	char buff[255];
    
	if (!fs->oid->entity->plugin->oid_ops.print) {
		aal_exception_info("Oid allocator print method is not implemented.");
		return 0;
	}
    
	aal_memset(buff, 0, sizeof(buff));
    
	printf("Oid allocator:\n");
	if (fs->oid->entity->plugin->oid_ops.print(fs->oid->entity,
						   buff, sizeof(buff), 0))
	{
		aal_exception_error("Can't print oid allocator.");
		return -1;
	}

	printf(buff);
	printf("\n");

	return 0;
}

static errno_t debugfs_print_journal(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, journal print is not implemented yet!");
	return 0;
}

struct total_frag_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	blk_t curr;
	count_t total, bad;
};

static errno_t debugfs_calc_joint(
	reiser4_joint_t *joint,	   /* joint to be estimated */
	void *data)		   /* user-specified data */
{
	uint32_t i, level;
	reiser4_node_t *node = joint->node;
	struct total_frag_hint *hint = (struct total_frag_hint *)data;

	level = plugin_call(return -1, node->entity->plugin->node_ops,
			    get_level, node->entity);

	if (level <= LEAF_LEVEL)
		return 0;
	
	if (hint->curr == 0)
		hint->curr = aal_block_number(node->block);

	for (i = 0; i < reiser4_node_count(node); i++) {
		int64_t delta;
		reiser4_coord_t coord;
		reiser4_ptr_hint_t ptr;
		reiser4_pos_t pos = {i, ~0ul};

		aal_gauge_touch(hint->gauge);
		
		if (reiser4_coord_open(&coord, node, CT_NODE, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, aal_block_number(node->block));
			return -1;
		}

		if (plugin_call(continue, coord.entity.plugin->item_ops,
				fetch, &coord.entity, 0, &ptr, 1))
			return -1;

		if (reiser4_item_nodeptr(&coord)) {
			delta = hint->curr - ptr.ptr;

			if (labs(delta) > 1) {
				hint->bad += labs(delta);
				hint->total += labs(delta);
			} else
				hint->total++;
			
			hint->curr = ptr.ptr;
		} else {
			if (ptr.ptr == 0)
				continue;
			
			delta = hint->curr - ptr.ptr;
			
			if (labs(delta) > 1) {
				hint->bad += labs(delta);
				hint->total += labs(delta);
			} else
				hint->total++;

			hint->curr = ptr.ptr + ptr.width;
		}
	}
	
	return 0;
}

static errno_t debugfs_total_fragmentation(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	struct total_frag_hint total_frag_hint;
	traverse_hint_t traverse_hint = {TO_FORWARD, LEAF_LEVEL};

	aal_memset(&total_frag_hint, 0, sizeof(total_frag_hint));
	
	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Estimating fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	total_frag_hint.tree = fs->tree;
	total_frag_hint.gauge = gauge;

	aal_gauge_start(gauge);
	
	reiser4_joint_traverse(fs->tree->root, &traverse_hint, (void *)&total_frag_hint,
			       debugfs_open_joint, debugfs_calc_joint, NULL, NULL, NULL, NULL);

	aal_gauge_free(gauge);

	printf("%.2f\n", total_frag_hint.total > 0 ?
	       (double)total_frag_hint.bad / total_frag_hint.total : 0);
	
	return 0;
};

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	debugfs_print_flags_t print_flags = 0;
	debugfs_behav_flags_t behav_flags = 0;
    
	char *host_dev;
	char *profile_label = "smart40";
    
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"profile", required_argument, NULL, 'e'},
		{"force", no_argument, NULL, 'f'},
		{"print-items", no_argument, NULL, 'i'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-block-alloc", no_argument, NULL, 'b'},
		{"print-oid-alloc", no_argument, NULL, 'o'},
		{"total-fragmentation", no_argument, NULL, 'T'},
		{"files-fragmentation", no_argument, NULL, 'F'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"quiet", no_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	debugfs_init();

	progs_print_banner(argv[0]);
    
	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long_only(argc, argv, "hVe:qfKstbojiTF",
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
		case 'T':
			behav_flags |= BF_TFRAG;
			break;
		case 'F':
			behav_flags |= BF_FFRAG;
			
			aal_exception_info("Sorry, not implemented yet!");
			return NO_ERROR;
		case 'f':
			behav_flags |= BF_FORCE;
			break;
		case 'q':
			behav_flags |= BF_QUIET;
			break;
		case 'K':
			progs_profile_list();
			return NO_ERROR;
		case '?':
			debugfs_print_usage(argv[0]);
			return USER_ERROR;
		}
	}
    
	if (print_flags == 0)
		print_flags |= PF_SUPER;
    
	if (optind >= argc) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
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

	if (!(print_flags & PF_TREE) && (print_flags & PF_ITEMS)) {
		aal_exception_warn("Option --print-items is only active if "
				   "--print-tree is specified.");
	}

	if ((behav_flags & BF_TFRAG)) {
		if (debugfs_total_fragmentation(fs))
			goto error_free_fs;
		print_flags = 0;
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

