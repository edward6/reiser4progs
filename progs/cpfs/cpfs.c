/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cpfs.c -- program for copying reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <aux/aux.h>
#include <misc/misc.h>
#include <reiser4/reiser4.h>

enum cpfs_behav_flags {
	BF_FORCE   = 1 << 0,
	BF_QUIET   = 1 << 1,
	BF_PROF    = 1 << 2,
	BF_PLUGS   = 1 << 3
};

typedef enum cpfs_behav_flags cpfs_behav_flags_t;

/* Prints cpfs options */
static void cpfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] "
		"SRC DST\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces acting without any questions.\n"
		"  -f, --force                     makes cpfs to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"Plugins options:\n"
		"  -P, --print-params              prints default params.\n"
		"  -p, --known-plugins             prints known plugins.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes used by mkfs exception streams */
static void cpfs_init(void) {
	int ex;

	/* Setting up exception streams*/
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		misc_exception_set_stream(ex, stderr);
}

int main(int argc, char *argv[]) {
	int c;
	
	struct stat st;
	fs_hint_t hint;

	reiser4_fs_t *src_fs;
	reiser4_fs_t *dst_fs;

	char override[4096];
	char *src_dev, *dst_dev;

	aal_device_t *src_device;
	aal_device_t *dst_device;
    
	aal_gauge_t *gauge = NULL;
	cpfs_behav_flags_t flags = 0;
    
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"print-params", no_argument, NULL, 'P'},
		{"print-plugins", no_argument, NULL, 'p'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};
    
	cpfs_init();
	memset(override, 0, sizeof(override));

	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVqfPpo:", long_options, 
				(int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			cpfs_print_usage(argv[0]);
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
			cpfs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
    
	if (!(flags & BF_QUIET))
		misc_print_banner(argv[0]);

	/* Initializing libreiser4 (getting plugins, checking them on validness,
	   etc). */
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Overriding params by passed by used values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';

		if (!(flags & BF_QUIET)) {
			aal_exception_mess("Overriding default params "
					   "by \"%s\".", override);
		}

		if (misc_param_override(override))
			goto error_free_libreiser4;
	}

	if (flags & BF_PROF)
		misc_param_print();
	
	if (flags & BF_PLUGS)
		misc_plugins_print();

	if (optind >= argc)
		goto error_free_libreiser4;
	
	src_dev = argv[optind++];
	dst_dev = argv[optind++];
	
	if (!src_dev || !dst_dev) {
		cpfs_print_usage(argv[0]);
		goto error_free_libreiser4;
	}
	
	if (!(flags & BF_QUIET)) {
		if (!(gauge = aal_gauge_create(GAUGE_SILENT, NULL)))
			goto error_free_libreiser4;
	}
    
	/* Checking is passed device is a block device. If so, we check also is
	   it whole drive or just a partition. If the device is not a block
	   device, then we emmit exception and propose user to use -f flag to
	   force. */
	if (stat(src_dev, &st) == -1) {
		aal_exception_error("Device %s does not exist.",
				    src_dev);
		goto error_free_libreiser4;
	}
    
	if (!S_ISBLK(st.st_mode)) {
		if (!(flags & BF_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", src_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    !(flags & BF_FORCE))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", src_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(src_dev, "rw") && !(flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted for read/write "
				    "at the moment. Use -f to force over.",
				    src_dev);
		goto error_free_libreiser4;
	}

	/* The same checks for @dst_dev */
	if (stat(dst_dev, &st) == -1) {
		aal_exception_error("Can't stat %s.", dst_dev);
		goto error_free_libreiser4;
	}
    
	if (!S_ISBLK(st.st_mode)) {
		if (!(flags & BF_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", dst_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    !(flags & BF_FORCE))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", dst_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(dst_dev, NULL) && !(flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted at the moment. "
				    "Use -f to force over.", dst_dev);
		goto error_free_libreiser4;
	}

	if (!strcmp(dst_dev, src_dev)) {
		aal_exception_error("Destination device is the same "
				    "as source one.");
		goto error_free_libreiser4;
	}
	
	/* Opening @src_device */
	if (!(src_device = aal_device_open(&file_ops, src_dev, 
					   512, O_RDONLY))) 
	{
		aal_exception_error("Can't open %s. %s.", src_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
    
	/* Opening @dst_device */
	if (!(dst_device = aal_device_open(&file_ops, dst_dev, 
					   512, O_RDWR))) 
	{
		aal_exception_error("Can't open %s. %s.", dst_dev,
				    strerror(errno));
		goto error_free_src_device;
	}

	/* Checking for "quiet" mode */
	if (!(flags & BF_QUIET)) {
		if (aal_exception_yesno("All data on %s will be lost. "
					"Are you sure?", dst_dev) == EXCEPTION_NO)
		{
			goto error_free_dst_device;
		}
	}
    
	if (gauge) {
		aal_gauge_rename(gauge, "Copying %s to %s",
				 src_dev, dst_dev);

		aal_gauge_start(gauge);
	}

	/* Opening source fs */
	if (!(src_fs = reiser4_fs_open(src_device, TRUE))) {
		aal_exception_error("Cannot open src filesystem on %s.",
				    src_dev);
		goto error_free_dst_device;
	}

	src_fs->tree->mpc_func = misc_mpressure_detect;

	/* Creating destinatrion fs */
	aal_strncpy(hint.uuid, reiser4_master_get_uuid(src_fs->master),
		    sizeof(hint.uuid));
	
	aal_strncpy(hint.label, reiser4_master_get_label(src_fs->master),
		    sizeof(hint.label));
	
	hint.blocks = reiser4_format_get_len(src_fs->format);
	hint.blksize = reiser4_master_get_blksize(src_fs->master);
	
	/* Creating dst filesystem */
	if (!(dst_fs = reiser4_fs_create(dst_device, &hint))) {
		aal_exception_error("Can't create filesystem on %s.", 
				    dst_dev);
		goto error_free_src_fs;
	}

	dst_fs->tree->mpc_func = misc_mpressure_detect;
	
	/* Creating journal */
	if (!(dst_fs->journal = reiser4_journal_create(dst_fs, dst_device, NULL)))
		goto error_free_dst_fs;


	if (reiser4_fs_copy(src_fs, dst_fs)) {
		aal_exception_error("Can't copy %s to %s.", src_dev, dst_dev);
		goto error_free_dst_journal;
	}
	
	if (gauge) {
		aal_gauge_done(gauge);
		aal_gauge_free(gauge);
	}

	/* Freeing dst journal */
	reiser4_journal_close(dst_fs->journal);

	/* Closing dst fs */
	reiser4_fs_close(dst_fs);

	/* Synchronizing device. If device we are using is a file device
	   (libaal/file.c), then function fsync will be called. */
	if (aal_device_sync(dst_device)) {
		aal_exception_error("Can't synchronize device %s.", 
				    dst_dev);
		goto error_free_dst_device;
	}

	aal_device_close(dst_device);
    
	/* Finalizing libreiser4. At the moment only plugins are unloading
	   durring this. */
	libreiser4_fini();
	return NO_ERROR;
	
 error_free_dst_journal:
	reiser4_journal_close(dst_fs->journal);
 error_free_dst_fs:
	reiser4_fs_close(dst_fs);
 error_free_src_fs:
	reiser4_fs_close(src_fs);
 error_free_dst_device:
	aal_device_close(dst_device);
 error_free_src_device:
	aal_device_close(src_device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}


