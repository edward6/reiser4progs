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
    PF_SUPER	= 1 << 0,
    PF_JOURNAL	= 1 << 1,
    PF_ALLOC	= 1 << 2,
    PF_OID	= 1 << 3,
    PF_TREE	= 1 << 4
};

typedef enum debugfs_print_flags debugfs_print_flags_t;

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
	"  -t | --print-tree              prints the whole tree (default).\n"
	"  -j | --print-journal           prints journal.\n"
	"  -s | --print-super-block       prints the both super blocks.\n"
	"  -b | --print-block-alloc       prints block allocator data.\n"
	"  -o | --print-oid-alloc         prints oid allocator data.\n"
	"Plugins options:\n"
	"  -e | --profile PROFILE         profile to be used.\n"
	"  -K | --known-profiles          prints known profiles.\n");
}

/* Initializes used by debugfs exception streams */
static void debugfs_init(void) {
    int i;

    /* Setting up exception streams*/
    for (i = 0; i < 5; i++)
	progs_exception_set_stream(i, stderr);
}

/* Callback function used in traverse for opening the node */
static errno_t debugfs_open_joint(
    reiser4_joint_t **joint,	/* joint to be opened */
    blk_t blk, void *data	/* blk to pe opened and user-specified data */
) {
    *joint = reiser4_tree_load((reiser4_tree_t *)data, blk);
    return -(*joint == NULL);
}

static errno_t debugfs_print_joint(
    reiser4_joint_t *joint,	/* joint to be printed */
    void *data			/* user-specified data */
) {
    char buff[255];
    reiser4_node_t *node = joint->node;
    uint8_t level = plugin_call(return -1, node->entity->plugin->node_ops,
	get_level, node->entity);

    printf("%s NODE (%llu) contains level=%u, items=%u, space=%u\n", 
	level > LEAF_LEVEL ? "TWIG" : "LEAF", aal_block_number(node->block),
	level, reiser4_node_count(node), reiser4_node_space(node));
    
    if (level > LEAF_LEVEL) {
	uint32_t i;
	char buff[255];
	
	for (i = 0; i < reiser4_node_count(node); i++) {
	    reiser4_key_t key;
	    reiser4_item_t item;
	    reiser4_pos_t pos = {i, ~0ul};

	    if (reiser4_item_open(&item, node, &pos)) {
		aal_exception_error("Can't open item %u in node %llu.", 
		    i, aal_block_number(node->block));
		return -1;
	    }

	    if (reiser4_item_nodeptr(&item))
		printf("(%u) NODEPTR: len=%u, ", i, reiser4_item_len(&item));
	    else
		printf("(%u) EXTENT: len=%u, ", i, reiser4_item_len(&item));
	    
	    if (reiser4_node_get_key(node, &pos, &key)) {
		aal_exception_error("Can't get key of item %u in node %llu.",
		    i, aal_block_number(node->block));
		return -1;
	    }
	    
	    aal_memset(buff, 0, sizeof(buff));
	    reiser4_key_print(&key, buff, sizeof(buff));
	    printf("KEY: %s, ", buff);

	    printf("PLUGIN: 0x%x (%s)\n", item.plugin->h.id, item.plugin->h.label);
	   
	    aal_memset(buff, 0, sizeof(buff));

	    if (reiser4_item_print(&item, buff, sizeof(buff)))
		return -1;

	    printf("[%s]\n", buff);
	}
    } else {
	uint32_t i;

	for (i = 0; i < reiser4_node_count(node); i++) {
	    reiser4_key_t key;
	    reiser4_item_t item;
	    reiser4_pos_t pos = {i, ~0ul};

	    if (reiser4_item_open(&item, node, &pos)) {
		aal_exception_error("Can't open item %u in node %llu.", 
		    i, aal_block_number(node->block));
		return -1;
	    }

	    if (reiser4_node_get_key(node, &pos, &key)) {
		aal_exception_error("Can't get key of item %u in node %llu.",
		    i, aal_block_number(node->block));
		return -1;
	    }
	    
	    printf("(%u) ", i);
		    
	    if (reiser4_item_statdata(&item)) {
		printf("STATDATA");
	    } else if (reiser4_item_direntry(&item)) {
		printf("DIRENTRY");
	    } else if (reiser4_item_tail(&item)) {
		printf("TAIL");
	    }
	    
	    printf(": len=%u, ", reiser4_item_len(&item));

	    aal_memset(buff, 0, sizeof(buff));
	    reiser4_key_print(&key, buff, sizeof(buff));
	    printf("KEY: %s, ", buff);
	    
	    printf("PLUGIN: 0x%x (%s)\n", item.plugin->h.id, item.plugin->h.label);
	}
    }
    
    return 0;
}

static errno_t debugfs_print_tree(reiser4_fs_t *fs) {
    reiser4_joint_traverse(fs->tree->root, (void *)fs->tree,
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
    
    printf("Format super block:\n");
    if (plugin_call(return -1, fs->format->entity->plugin->format_ops,
	print, fs->format->entity, buff, sizeof(buff), 0))
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
	aal_exception_info("Oid allocator print is not implemented or its info "
	    "is printed by super block print.");
	return 0;
    }
    
    fs->oid->entity->plugin->oid_ops.print(fs->oid->entity,
	buff, sizeof(buff), 0);

    printf(buff);
    printf("\n");

    return 0;
}
   
static errno_t debugfs_print_journal(reiser4_fs_t *fs) {
    aal_exception_error("Sorry, journal print is not implemented yet!");
    return 0;
}

int main(int argc, char *argv[]) {
    struct stat st;
    int c, force = 0, quiet = 0;
    debugfs_print_flags_t flags = 0;
    
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
	{"print-tree", no_argument, NULL, 't'},
	{"print-journal", no_argument, NULL, 'j'},
	{"print-super-block", no_argument, NULL, 's'},
	{"print-block-alloc", no_argument, NULL, 'b'},
	{"print-oid-alloc", no_argument, NULL, 'o'},
	{"known-profiles", no_argument, NULL, 'K'},
	{"quiet", no_argument, NULL, 'q'},
	{0, 0, 0, 0}
    };
    
    debugfs_init();
    
    progs_misc_print_banner(argv[0]);
    
    if (argc < 2) {
	debugfs_print_usage(argv[0]);
	return USER_ERROR;
    }
    
    /* Parsing parameters */    
    while ((c = getopt_long_only(argc, argv, "hVe:qfKstboj", long_options, 
	(int *)0)) != EOF) 
    {
	switch (c) {
	    case 'h': {
		debugfs_print_usage(argv[0]);
		return NO_ERROR;
	    }
	    case 'V': {
		progs_misc_print_banner(argv[0]);
		return NO_ERROR;
	    }
	    case 'e': {
		profile_label = optarg;
		break;
	    }
	    case 'o': {
		flags |= PF_OID;
		break;
	    }
	    case 'b': {
		flags |= PF_ALLOC;
		break;
	    }
	    case 's': {
		flags |= PF_SUPER;
		break;
	    }
	    case 'j': {
		flags |= PF_JOURNAL;
		break;
	    }
	    case 't': {
		flags |= PF_TREE;
		break;
	    }
	    case 'f': {
		force = 1;
		break;
	    }
	    case 'q': {
		quiet = 1;
		break;
	    }
	    case 'K': {
		progs_profile_list();
		return NO_ERROR;
	    }
	    case '?': {
	        debugfs_print_usage(argv[0]);
	        return USER_ERROR;
	    }
	}
    }
    
    if (flags == 0)
	flags |= PF_TREE;
    
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
    
    if (stat(host_dev, &st) == -1)
        goto error_free_libreiser4;
    
    /* 
        Checking is passed device is a block device. If so, we check also
        is it whole drive or just a partition. If the device is not a block
        device, then we emmit exception and propose user to use -f flag to 
        force.
    */
    if (!S_ISBLK(st.st_mode)) {
        if (!force) {
	    aal_exception_error("Device %s is not block device. "
		"Use -f to force over.", host_dev);
	    goto error_free_libreiser4;
	}
    } else {
        if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
	   (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) && !force)
	{
	    aal_exception_error("Device %s is an entire harddrive, not "
	        "just one partition.", host_dev);
	    goto error_free_libreiser4;
	}
    }
   
    /* Checking if passed partition is mounted */
    if (progs_misc_dev_mounted(host_dev, NULL) && !force) {
        aal_exception_error("Device %s is mounted at the moment. "
	   "Use -f to force over.", host_dev);
	goto error_free_libreiser4;
    }

    /* Opening device */
    if (!(device = aal_file_open(host_dev, DEFAULT_BLOCKSIZE, O_RDONLY))) {
        char *error = strerror(errno);
	
        aal_exception_error("Can't open device %s. %s.", host_dev, error);
        goto error_free_libreiser4;
    }
    
    if (!(fs = reiser4_fs_open(device, device, 0))) {
	aal_exception_error("Can't open reiser4 on %s", host_dev);
	goto error_free_libreiser4;
    }
    
    if (!aal_pow_of_two(flags) && !force) {
	if (aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_YESNO,
	    "Ambiguous print options has been detected. Continue?") == EXCEPTION_NO)
	    goto error_free_fs;
    }
    
    if (flags & PF_SUPER) {
	if (debugfs_print_master(fs))
	    goto error_free_fs;
	
	if (debugfs_print_format(fs))
	    goto error_free_fs;
    }
    
    if (flags & PF_ALLOC) {
	if (debugfs_print_alloc(fs))
	    goto error_free_fs;
    }
    
    if (flags & PF_OID) {
	if (debugfs_print_oid(fs))
	    goto error_free_fs;
    }
    
    if (flags & PF_JOURNAL) {
	if (debugfs_print_journal(fs))
	    goto error_free_fs;
    }
    
    if (flags & PF_TREE) {
	if (debugfs_print_tree(fs))
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

