/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   resizefs.c -- program for resizing reiser4. */

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
	BF_PROF     = 1 << 2,
	BF_PLUGS    = 1 << 3
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
		"  -P, --print-profile             prints default profile.\n"
		"  -p, --print-plugins             prints known plugins.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes exception streams used by resizefs */
static void resizefs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		misc_exception_set_stream(ex, stderr);
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
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"print-profile", no_argument, NULL, 'P'},
		{"print-plugins", no_argument, NULL, 'p'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};

	resizefs_init();
	memset(override, 0, sizeof(override));

	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "Vhqfo:Pp",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			resizefs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'f':
			flags |= BF_FORCE;
			break;
		case 'q':
			flags |= BF_QUIET;
			break;
		case 'P':
			flags |= BF_PROF;
			break;
		case 'p':
			flags |= BF_PLUGS;
			break;
		case 'o':
			aal_strncat(override, optarg,
				    aal_strlen(optarg));
			
			aal_strncat(override, ",", 1);
			break;
		case '?':
			resizefs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
	
	if (!(flags & BF_QUIET))
		misc_print_banner(argv[0]);

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

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
	
	if (flags & BF_PROF)
		misc_profile_print();

	if (flags & BF_PLUGS)
		misc_plugins_print();

	if (optind >= argc)
		goto error_free_libreiser4;
		
	host_dev = argv[optind++];
    
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
				       512, O_RDWR)))
	{
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}

	/* Open file system on the device */
	if (!(fs = reiser4_fs_open(device))) {
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}

	/* Initializing tree and tree's traps */
	if (!(fs->tree = reiser4_tree_init(fs, NULL)))
		goto error_free_fs;
    
	fs_len = misc_size2long(argv[optind]);

	if (fs_len == INVAL_DIG) {
		aal_exception_error("Invalid new filesystem "
				    "size %s.", argv[optind]);
		goto error_free_tree;
	}
	
	fs_len /= (reiser4_master_blksize(fs->master) / 1024);

	if (reiser4_fs_resize(fs, fs_len)) {
		aal_exception_error("Can't resize reiser4 on %s.",
				    host_dev);
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
