/*
  resizefs.c -- program for resizing reiser4.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

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
	F_FORCE    = 1 << 0,
	F_QUIET    = 1 << 1,
	F_PLUGS    = 1 << 2,
	F_PROFS    = 1 << 3
};

typedef enum behav_flags behav_flags_t;

/* Prints resizefs options */
static void resizefs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE size[K|M|G]\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces creating filesystem without\n"
		"                                  any questions.\n"
		"  -f, --force                     makes reiserer to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE           profile to be used.\n"
		"  -P, --known-plugins             prints known plugins.\n"
		"  -K, --known-profiles            prints known profiles.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes exception streams used by resizefs */
static void resizefs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

/* Handler for connecting node into tree */
static errno_t resizefs_connect_handler(reiser4_tree_t *tree,
					reiser4_place_t *place,
					reiser4_node_t *node,
					void *data)
{
	/*
	  If tree's LRU is initializied and memory pressure is detected, calling
	  adjust lru code, which will remove unused nodes from the tree.
	*/
	if (tree->lru) {
		if (progs_mpressure_detect())
			return aal_lru_adjust(tree->lru);
	}
	
	return 0;
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;
	count_t fs_len;

	uint32_t flags = 0;
	char override[4096];

	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	char *frag_filename = NULL;
	char *profile_label = "smart40";
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"profile", required_argument, NULL, 'e'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"known-plugins", no_argument, NULL, 'P'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};

	resizefs_init();

	if (argc < 3) {
		resizefs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "Vhe:qfo:P",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			resizefs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			progs_print_banner(argv[0]);
			return NO_ERROR;
		case 'e':
			profile_label = optarg;
			break;
		case 'f':
			flags |= F_FORCE;
			break;
		case 'q':
			flags |= F_QUIET;
			break;
		case 'P':
			flags |= F_PLUGS;
			break;
		case 'o':
			aal_strncat(override, optarg,
				    aal_strlen(optarg));
			
			aal_strncat(override, ",", 1);
			break;
		case 'K':
			flags |= F_PROFS;
			break;
		case '?':
			resizefs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
	
	if (optind >= argc + 1) {
		resizefs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	if (!(flags & F_QUIET))
		progs_print_banner(argv[0]);

	if (flags & F_PROFS) {
		progs_profile_list();
		return NO_ERROR;
	}
	
	/* Initializing passed profile */
	if (!(profile = progs_profile_find(profile_label))) {
		aal_exception_error("Can't find profile by its "
				    "label %s.", profile_label);
		goto error;
	}
    
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	if (flags & F_PLUGS) {
		progs_plugin_list();
		libreiser4_fini();
		return 0;
	}
	
	/*
	  Overriding profile by passed by used values. This should be done after
	  libreiser4 is initialized.
	*/
	if (aal_strlen(override) > 0) {
		aal_exception_info("Overriding profile %s by \"%s\".",
				   profile->name, override);
		
		if (progs_profile_override(profile, override))
			goto error_free_libreiser4;
	}
	
	host_dev = argv[optind++];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* 
	   Checking if passed device is a block one. If so, we check also is
	   it whole drive or just a partition. If the device is not a block
	   device, then we emmit exception and propose user to use -f flag to
	   force.
	*/
	if (!S_ISBLK(st.st_mode)) {
		if (!(flags & F_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    (!(flags & F_FORCE)))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", host_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (progs_dev_mounted(host_dev, NULL) && !(flags & F_FORCE)) {
		aal_exception_error("Device %s is mounted at the moment. "
				    "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev,
				       REISER4_BLKSIZE, O_RDONLY)))
	{
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}

	/* Open file system on the device */
	if (!(fs = reiser4_fs_open(device, profile))) {
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}

	/* Initializing tree and tree's traps */
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_fs;
    
	fs_len = progs_size2long(argv[optind]);

	if (fs_len == INVAL_DIG) {
		aal_exception_error("Invalid new filesystem "
				    "size %s.", argv[optind]);
		goto error_free_tree;
	}
	
	fs_len /= reiser4_master_blocksize(fs->master);
	fs->tree->traps.connect = resizefs_connect_handler;

	if (reiser4_fs_resize(fs, fs_len)) {
		aal_exception_error("Can't resize reiser4 on %s.",
				    host_dev);
		goto error_free_tree;
	}
	
	/* Freeing tree */
	reiser4_tree_close(fs->tree);
    
	/* Deinitializing filesystem instance and device instance */
	reiser4_fs_close(fs);
	aal_device_close(device);
    
	/* 
	   Deinitializing libreiser4. At the moment only plugins are unloading 
	   durrign this.
	*/
	libreiser4_fini();
	return NO_ERROR;

 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}
