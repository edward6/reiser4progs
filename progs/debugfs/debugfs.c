/*
  debugfs.c -- program for debugging reiser4 filesystem.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "debugfs.h"

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces creating filesystem without\n"
		"                                  any questions.\n"
		"  -f, --force                     makes debugfs to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"Browsing options:\n"
		"  -l, --ls FILE                   browses passed file like standard\n"
		"                                  ls program.\n"
		"  -c, --cat FILE                  browses passed file like standard\n"
		"                                  cat program.\n"
		"Print options:\n"
		"  -t, --print-tree                prints the whole tree.\n"
		"  -j, --print-journal             prints journal.\n"
		"  -s, --print-super               prints the both super blocks.\n"
		"  -b, --print-block-alloc         prints block allocator data.\n"
		"  -d, --print-oid-alloc           prints oid allocator data.\n"
		"  -n, --print-block N             prints block by its number.\n"
		"  -i, --print-file FILE           prints the all file's metadata.\n"
		"  -w, --print-items               forces --print-file show only items\n"
		"                                  which are belong to specified file.\n"
		"Measurement options:\n"
		"  -S, --tree-stat                 measures some tree characteristics\n"
		"                                  (node packing, etc).\n"
		"  -T, --tree-frag                 measures tree fragmentation.\n"
		"  -F, --file-frag FILE            measures fragmentation of specified\n"
		"                                  file.\n"
		"  -D, --data-frag                 measures average files fragmentation.\n"
		"  -p, --show-each                 show file fragmentation for each file\n"
		"                                  if --data-frag is specified.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE           profile to be used.\n"
		"  -P, --known-plugins             prints known plugins.\n"
		"  -K, --known-profiles            prints known profiles.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes used by debugfs exception streams */
static void debugfs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

/*
  Prints passed @buff into stdout. The special print function is needed because
  we can't just put 4k buffer into stdout.
*/
errno_t debugfs_print_buff(void *buff, uint32_t size) {
	int len = size;
	void *ptr = buff;

	while (len > 0) {
		int written;

		if ((written = write(1, ptr, len)) <= 0) {
			
			if (errno == EINTR)
				continue;
			
			return -1;
		}
		
		ptr += written;
		len -= written;
	}

	return 0;
}

errno_t debugfs_print_stream(aal_stream_t *stream) {
	return debugfs_print_buff(stream->data, stream->size - 1);
}

/* Handler for connecting node into tree */
static errno_t debugfs_connect_handler(reiser4_tree_t *tree,
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

/* Hnalder for disconnecting node from the tree */
static errno_t debugfs_disconnect_handler(reiser4_tree_t *tree,
					  reiser4_place_t *place,
					  reiser4_node_t *node,
					  void *data)
{
	return 0;
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;
	uint32_t print_flags = 0;
	uint32_t behav_flags = 0;

	char override[4096];
	char *ls_filename = NULL;
	char *cat_filename = NULL;
	char *frag_filename = NULL;
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
		{"ls", required_argument, NULL, 'l'},
		{"cat", required_argument, NULL, 'c'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-block-alloc", no_argument, NULL, 'b'},
		{"print-oid-alloc", no_argument, NULL, 'd'},
		{"print-block", required_argument, NULL, 'n'},
		{"print-file", required_argument, NULL, 'i'},
		{"print-items", no_argument, NULL, 'w'},
		{"tree-stat", no_argument, NULL, 'S'},
		{"tree-frag", no_argument, NULL, 'T'},
		{"file-frag", required_argument, NULL, 'F'},
		{"data-frag", no_argument, NULL, 'D'},
		{"show-each", no_argument, NULL, 'p'},
		{"profile", required_argument, NULL, 'e'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"known-plugins", no_argument, NULL, 'P'},
		{"override", required_argument, NULL, 'o'},
		{"quiet", no_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	debugfs_init();

	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVe:qfKtbdjTDpSF:c:l:n:i:wo:P",
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
		case 'd':
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
		case 't':
			print_flags |= PF_TREE;
			break;
		case 'n': {
			print_flags |= PF_BLOCK;

			/* Parsing block number */
			if ((blocknr = progs_str2long(optarg, 10)) == INVAL_DIG) {
				aal_exception_error("Invalid block number (%s).", optarg);
				return USER_ERROR;
			}
			
			break;
		}
		case 'i':
			print_flags |= PF_FILE;
			print_filename = optarg;
			break;
		case 'w':
			print_flags |= PF_ITEMS;
			break;
		case 'S':
			behav_flags |= BF_TSTAT;
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
		case 'P':
			behav_flags |= BF_PLUGS;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
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

	if (behav_flags & BF_PLUGS) {
		progs_plugin_list();
		libreiser4_done();
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

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev,
				       BLOCKSIZE, O_RDONLY)))
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
    
	fs->tree->traps.connect = debugfs_connect_handler;
	fs->tree->traps.disconnect = debugfs_disconnect_handler;
	
	/*
	  Check if few print options are specified. If so, and --quiet flag was
	  not applyed we throw warning, because that is probably user error and
	  a lot of information will confuse him.
	*/
	if (!aal_pow_of_two(print_flags) && !(behav_flags & BF_QUIET) &&
	    !(print_flags & PF_ITEMS))
	{
		if (aal_exception_yesno("Few print options has been detected. "
					"Continue?") == EXCEPTION_NO)
			goto error_free_tree;
	}

	/*
	q  In the case no print flags was specified, debugfs will print super
	  blocks by defaut.
	*/
	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;

	/*
	  Check if specified options are compatible. For instance, --show-each
	  can be used only if --data-frag was specified.
	*/
	if (!(behav_flags & BF_DFRAG) && (behav_flags & BF_SEACH)) {
		aal_exception_warn("Option --show-each is only active if "
				   "--data-frag is specified.");
	}

	/* The same for --print-file option */
	if (!(print_flags & PF_FILE) && (print_flags & PF_ITEMS)) {
		aal_exception_warn("Option --print-items is only active if "
				   "--print-file is specified.");
	}

	/* Handling measurements options */
	if (behav_flags & BF_TFRAG || behav_flags & BF_DFRAG ||
	    behav_flags & BF_FFRAG || behav_flags & BF_TSTAT)
	{
		if (behav_flags & BF_QUIET ||
		    aal_exception_yesno("This operation may take a long time. "
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
	
			if (behav_flags & BF_TSTAT) {
				if (debugfs_tree_stat(fs))
					goto error_free_fs;
			}
		}
	}
	
	/* Handling print options */
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
		if (debugfs_print_tree(fs))
			goto error_free_fs;
	}

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr))
			goto error_free_fs;
	}
    
	if (print_flags & PF_FILE) {
		if (debugfs_print_file(fs, print_filename, print_flags))
			goto error_free_fs;
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
	libreiser4_done();
    
	return NO_ERROR;

 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_done();
 error:
	return OPER_ERROR;
}

