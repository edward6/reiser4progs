/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debugfs.c -- program for debugging reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
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
		"  -a, --print-alloc               prints block allocator data.\n"
		"  -b, --print-block N             prints block by its number.\n"
		"  -n, --print-nodes FILE          prints all nodes file lies in.\n"
		"  -i, --print-file FILE           prints all items specified file\n"
		"                                  consists of.\n"
		"Metadata options:\n"
		"  -g, --pack-metadata             fetches filesystem metadata and writes it\n"
		"                                  to standard output.\n"
		"  -l, --unpack-metadata           uses metadata stream from stdandard input\n"
		"                                  to construct filesystem by it.\n"
		"Plugins options:\n"
		"  -P, --print-params              prints default params.\n"
		"  -p, --print-plugins             prints known plugins.\n"
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

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;

	uint32_t print_flags = 0;
	uint32_t behav_flags = 0;

	char override[4096];
	char *cat_filename = NULL;
	char *print_filename = NULL;
    
	reiser4_fs_t *fs;
	aal_device_t *device;
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
		{"print-alloc", no_argument, NULL, 'a'},
		{"print-oid", no_argument, NULL, 'd'},
		{"print-block", required_argument, NULL, 'b'},
		{"print-nodes", required_argument, NULL, 'n'},
		{"print-file", required_argument, NULL, 'i'},
		{"pack-metadata", no_argument, NULL, 'g'},
		{"unpack-metadata", no_argument, NULL, 'l'},
		{"print-params", no_argument, NULL, 'P'},
		{"print-plugins", no_argument, NULL, 'p'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};

	debugfs_init();
	memset(override, 0, sizeof(override));
	
	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVqftb:djc:n:i:o:Ppsagl",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'd':
			print_flags |= PF_OID;
			break;
		case 'a':
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
		case 'g':
			behav_flags |= BF_PACK_META;
			break;
		case 'l':
			behav_flags |= BF_UNPACK_META;
			break;
		case 'p':
			behav_flags |= BF_PLUGS;
			break;
		case 'P':
			behav_flags |= BF_PROF;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
			break;
		case '?':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		}
	}
    
	if (!(behav_flags & BF_QUIET))
		misc_print_banner(argv[0]);

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Overriding default params by passed values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';
		
		if (!(behav_flags & BF_QUIET)) {
			aal_exception_mess("Overriding default params "
					   "by \"%s\".", override);
		}
		
		if (misc_param_override(override))
			goto error_free_libreiser4;
	}
	
	if (behav_flags & BF_PROF)
		misc_param_print();

	if (behav_flags & BF_PLUGS)
		misc_plugins_print();

	if (optind >= argc) {
		debugfs_print_usage(argv[0]);
		goto error_free_libreiser4;
	}
		
	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(host_dev, "rw") && !(behav_flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted 'rw' at the moment. "
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

	if (behav_flags & BF_UNPACK_META) {
		aal_stream_t stream;
		
		aal_stream_init(&stream, stdin,
				&file_stream);
		
		if (!(fs = reiser4_fs_unpack(device, &stream))) {
			aal_exception_error("Can't unpack filesystem.");
			goto error_free_device;
		}

		aal_stream_fini(&stream);

		if (reiser4_fs_sync(fs)) {
			aal_exception_error("Can't save unpacked "
					    "filesystem.");
			goto error_free_fs;
		}
	} else {
		/* Open file system on the device */
		if (!(fs = reiser4_fs_open(device, FALSE))) {
			aal_exception_error("Can't open reiser4 on %s", host_dev);
			goto error_free_device;
		}
	}
	
	/* Opening the journal */
	if (!(fs->journal = reiser4_journal_open(fs, device))) {
		aal_exception_error("Can't open journal on %s", host_dev);
		goto error_free_fs;
	}
		
	/* In the case no print flags was specified, debugfs will print super
	   blocks by defaut. */
	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;

	/* Handling print options */
	if ((behav_flags & BF_CAT)) {
		if (debugfs_browse(fs, cat_filename))
			goto error_free_journal;
	}
	
	if (print_flags & PF_SUPER) {
		if (debugfs_print_master(fs))
			goto error_free_journal;
	
		if (debugfs_print_status(fs))
			goto error_free_journal;
		
		if (debugfs_print_format(fs))
			goto error_free_journal;
	}
    
	if (print_flags & PF_OID) {
		if (debugfs_print_oid(fs))
			goto error_free_journal;
	}
    
	if (print_flags & PF_ALLOC) {
		if (debugfs_print_alloc(fs))
			goto error_free_journal;
	}
    
	if (print_flags & PF_JOURNAL) {
		if (debugfs_print_journal(fs))
			goto error_free_journal;
	}
    
	if (print_flags & PF_TREE) {
		if (debugfs_print_tree(fs))
			goto error_free_journal;
	}

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr))
			goto error_free_journal;
	}
    
	if (print_flags & PF_NODES || print_flags & PF_ITEMS) {
		if (debugfs_print_file(fs, print_filename, print_flags))
			goto error_free_journal;
	}

	if (behav_flags & BF_PACK_META) {
		aal_stream_t stream;

		aal_stream_init(&stream, stdout,
				&file_stream);
		
		if (reiser4_fs_pack(fs, &stream)) {
			aal_exception_error("Can't pack filesystem.");
			goto error_free_journal;
		}

		aal_stream_fini(&stream);
	}
	
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

