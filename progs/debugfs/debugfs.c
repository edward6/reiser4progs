/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debugfs.c -- program for debugging reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "debugfs.h"

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces using filesystem without\n"
		"                                  any questions.\n"
		"  -f, --force                     makes debugfs to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"Browsing options:\n"
		"  -c, --cat FILE                  browses passed file like standard\n"
		"                                  cat and ls programs.\n"
		"Print options:\n"
		"  -s, --print-super               prints the both super blocks.\n"
		"  -t, --print-tree                prints the whole tree.\n"
		"  -j, --print-journal             prints journal.\n"
		"  -d, --print-oid                 prints oid allocator data.\n"
		"  -k, --print-alloc               prints block allocator data.\n"
		"  -b, --print-block N             prints block by its number.\n"
		"  -n, --print-nodes FILE          prints all nodes file lies in.\n"
		"  -i, --print-items FILE          prints all items file consists of.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE           profile to be used.\n"
		"  -P, --known-plugins             prints known plugins.\n"
		"  -K, --known-profiles            prints known profiles.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes exception streams used by debugfs */
static void debugfs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		misc_exception_set_stream(ex, stderr);
}

/* Prints passed @buff into stdout. The special print function is needed because
   we can't just put 4k buffer into stdout. */
errno_t debugfs_print_buff(void *buff, uint32_t size) {
	int len = size;
	void *ptr = buff;

	while (len > 0) {
		int written;

		if ((written = write(1, ptr, len)) <= 0) {
			
			if (errno == EINTR)
				continue;
			
			return -EIO;
		}
		
		ptr += written;
		len -= written;
	}

	return 0;
}

errno_t debugfs_print_stream(aal_stream_t *stream) {
	return debugfs_print_buff(stream->data, stream->size - 1);
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;

	uint32_t print_flags = 0;
	uint32_t behav_flags = 0;

	char override[4096];
	char *cat_filename = NULL;
	char *print_filename = NULL;
	char *profile_label = "smart40";
    
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	blk_t blocknr = 0;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"cat", required_argument, NULL, 'c'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-alloc", no_argument, NULL, 'k'},
		{"print-oid", no_argument, NULL, 'd'},
		{"print-block", required_argument, NULL, 'b'},
		{"print-nodes", required_argument, NULL, 'n'},
		{"print-items", required_argument, NULL, 'i'},
		{"profile", required_argument, NULL, 'e'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"known-plugins", no_argument, NULL, 'P'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};

	debugfs_init();

	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVe:qfKtb:djc:n:i:o:P",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'e':
			profile_label = optarg;
			break;
		case 'd':
			print_flags |= PF_OID;
			break;
		case 'k':
			print_flags |= PF_ALLOC;
			break;
		case 's':
			print_flags |= PF_SUPER;
			break;
		case 'j':
			print_flags |= PF_JOURNAL;
			break;
		case 't':
			print_flags |= PF_TREE;
			break;
		case 'b': {
			print_flags |= PF_BLOCK;

			/* Parsing block number */
			if ((blocknr = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_exception_error("Invalid block number (%s).", optarg);
				return USER_ERROR;
			}
			
			break;
		}
		case 'n':
			print_flags |= PF_NODES;
			print_filename = optarg;
			break;
		case 'i':
			print_flags |= PF_ITEMS;
			print_filename = optarg;
			break;
		case 'c':
			behav_flags |= BF_CAT;
			cat_filename = optarg;
			break;
		case 'f':
			behav_flags |= BF_FORCE;
			break;
		case 'q':
			behav_flags |= BF_QUIET;
			break;
		case 'P':
			behav_flags |= BF_PLUGS;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
			break;
		case 'K':
			behav_flags |= BF_PROFS;
			break;
		case '?':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
    
	if (optind >= argc) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	if (!(behav_flags & BF_QUIET))
		misc_print_banner(argv[0]);

	if (behav_flags & BF_PROFS) {
		misc_profile_list();
		return NO_ERROR;
	}
	
	/* Initializing passed profile */
	if (!(profile = misc_profile_find(profile_label))) {
		aal_exception_error("Can't find profile by its label %s.", 
				    profile_label);
		goto error;
	}
    
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	if (behav_flags & BF_PLUGS) {
		misc_plugin_list();
		libreiser4_fini();
		return 0;
	}
	
	/* Overriding profile by passed by used values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		aal_exception_info("Overriding profile %s by \"%s\".",
				   profile->name, override);
		
		if (misc_profile_override(profile, override))
			goto error_free_libreiser4;
	}
	
	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* Checking is passed device is a block device. If so, we check also is
	   it whole drive or just a partition. If the device is not a block
	   device, then we emmit exception and propose user to use -f flag to
	   force. */
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
	if (misc_dev_mounted(host_dev, NULL) && !(behav_flags & BF_FORCE)) {
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
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening the journal */
	if (!(fs->journal = reiser4_journal_open(fs, device))) {
		aal_exception_error("Can't open journal on %s", host_dev);
		goto error_free_fs;
	}
	
	/* Initializing tree and tree's traps */
	if (!(fs->tree = reiser4_tree_init(fs, NULL)))
		goto error_free_journal;
    
	/* In the case no print flags was specified, debugfs will print super
	   blocks by defaut. */
	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;

	/* Handling print options */
	if ((behav_flags & BF_CAT)) {
		if (debugfs_browse(fs, cat_filename))
			goto error_free_tree;
	}
	
	if (print_flags & PF_SUPER) {
		if (debugfs_print_master(fs))
			goto error_free_tree;
	
		if (debugfs_print_format(fs))
			goto error_free_tree;
	}
    
	if (print_flags & PF_OID) {
		if (debugfs_print_oid(fs))
			goto error_free_tree;
	}
    
	if (print_flags & PF_ALLOC) {
		if (debugfs_print_alloc(fs))
			goto error_free_tree;
	}
    
	if (print_flags & PF_JOURNAL) {
		if (debugfs_print_journal(fs))
			goto error_free_tree;
	}
    
	if (print_flags & PF_TREE) {
		if (debugfs_print_tree(fs))
			goto error_free_tree;
	}

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr))
			goto error_free_tree;
	}
    
	if (print_flags & PF_NODES || print_flags & PF_ITEMS) {
		if (debugfs_print_file(fs, print_filename, print_flags))
			goto error_free_tree;
	}
	
	/* Releasing the tree */
	reiser4_tree_fini(fs->tree);

	/* Closing the journal */
	reiser4_journal_close(fs->journal);
	
	/* Closing filesystem itself */
	reiser4_fs_close(fs);

	/* Closing device */
	aal_device_close(device);
    
	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   while doing this. */
	libreiser4_fini();
	return NO_ERROR;

 error_free_tree:
	reiser4_tree_fini(fs->tree);
 error_free_journal:
	reiser4_journal_close(fs->journal);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}

