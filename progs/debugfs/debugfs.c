/*
    debugfs.c -- program to debug reiser4 filesystem.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#ifdef HAVE_LIBUUID
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

#include <reiser4/reiser4.h>

#include <aux/aux.h>
#include <misc/misc.h>

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

static errno_t debugfs_print_node(reiser4_node_t *node) {
    uint8_t level = plugin_call(return -1, node->entity->plugin->node_ops,
	get_level, node->entity);

    printf("%s NODE (%llu) contains level=%u, nr_items=%u, free_space=%u\n", 
	level > REISER4_LEAF_LEVEL ? "INTERNAL" : "LEAF", aal_block_number(node->block),
	level, reiser4_node_count(node), reiser4_node_space(node));
    
    if (level > REISER4_LEAF_LEVEL) {
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

	    if (reiser4_item_internal(&item))
		printf("(%u) PTR: len=%u, ", i, reiser4_item_len(&item));
	    else
		printf("(%u) EXT: len=%u, ", i, reiser4_item_len(&item));
	    
	    if (reiser4_node_get_key(node, &pos, &key)) {
		aal_exception_error("Can't get key of item %u in node %llu.",
		    i, aal_block_number(node->block));
		return -1;
	    }
	    
	    printf("KEY: 0x%llx 0x%x 0x%llx 0x%llx, ",
		reiser4_key_get_locality(&key), reiser4_key_get_type(&key),
		reiser4_key_get_objectid(&key), reiser4_key_get_offset(&key));

	    printf("PLUGIN: 0x%x (%s)\n", item.plugin->h.id, item.plugin->h.label);
	   
	    if (reiser4_item_internal(&item)) {
		printf("[ %llu ]\n", reiser4_item_get_iptr(&item));
	    } else {
		char buff[255];
		
		aal_memset(buff, 0, sizeof(buff));
		
		plugin_call(return -1, item.plugin->item_ops, print,
		    &item, buff, sizeof(buff), 0);

		printf("[ %s ]\n", buff);
	    }
	}
	
	for (i = 0; i < reiser4_node_count(node); i++) {
	    blk_t blk;
	    aal_block_t *block;
	    reiser4_item_t item;
	    reiser4_node_t *inode;
	    reiser4_pos_t pos = {i, ~0ul};

	    if (reiser4_item_open(&item, node, &pos)) {
		aal_exception_error("Can't open item %u in node %llu.", 
		    i, aal_block_number(node->block));
		return -1;
	    }

	    if (!reiser4_item_internal(&item))
		continue;

	    blk = reiser4_item_get_iptr(&item);
	    if (!(block = aal_block_open(node->block->device, blk))) {
		aal_exception_error("Can't read block %llu. %s.", 
		    blk, node->block->device->error);
		return -1;
	    }
	    
	    if (!(inode = reiser4_node_open(block))) {
		aal_exception_error("Can't open node %llu. %s.", 
		    blk, node->block->device->error);
		return -1;
	    }

	    if (debugfs_print_node(inode))
		return -1;
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
		printf("SD");
	    } else if (reiser4_item_direntry(&item)) {
		printf("DR");
	    } else if (reiser4_item_tail(&item)) {
		printf("TL");
	    }
	    
	    printf(": len=%u, ", reiser4_item_len(&item));

	    printf("KEY: 0x%llx 0x%x 0x%llx 0x%llx, ", 
		reiser4_key_get_locality(&key), reiser4_key_get_type(&key),
		reiser4_key_get_objectid(&key), reiser4_key_get_offset(&key));
	    
	    printf("PLUGIN: 0x%x (%s)\n", item.plugin->h.id, item.plugin->h.label);

	}
    }
    
    return 0;
}

static errno_t debugfs_print_fs(reiser4_fs_t *fs) {
    return debugfs_print_node(fs->tree->cache->node);
}

int main(int argc, char *argv[]) {
    struct stat st;
    int c, force = 0, quiet = 0;
    
    char *host_dev;
    char *profile_label = "default40";
    
    reiser4_fs_t *fs;
    aal_device_t *device;
    reiser4_profile_t *profile;

    static struct option long_options[] = {
	{"version", no_argument, NULL, 'V'},
	{"help", no_argument, NULL, 'h'},
	{"profile", required_argument, NULL, 'e'},
	{"force", no_argument, NULL, 'f'},
	{"known-profiles", no_argument, NULL, 'K'},
	{"quiet", no_argument, NULL, 'q'},
	{0, 0, 0, 0}
    };
    
    if (argc < 2) {
	debugfs_print_usage(argv[0]);
	return USER_ERROR;
    }

    debugfs_init();
    
    /* Parsing parameters */    
    while ((c = getopt_long_only(argc, argv, "hVe:qfK", long_options, 
	(int *)0)) != EOF) 
    {
	switch (c) {
	    case 'h': {
		debugfs_print_usage(argv[0]);
		return NO_ERROR;
	    }
	    case 'V': {
		printf(BANNER(argv[0]));
		return NO_ERROR;
	    }
	    case 'e': {
		profile_label = optarg;
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
    
    printf(BANNER(argv[0]));

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
    if (!(device = aal_file_open(host_dev, REISER4_DEFAULT_BLOCKSIZE, O_RDONLY))) {
        char *error = strerror(errno);
	
        aal_exception_error("Can't open device %s. %s.", host_dev, error);
        goto error_free_libreiser4;
    }
    
    if (!(fs = reiser4_fs_open(device, device, 0))) {
	aal_exception_error("Can't open reiser4 on %s", host_dev);
	goto error_free_libreiser4;
    }
    
    if (debugfs_print_fs(fs))
	goto error_free_fs;
    
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

