/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mkfs.c -- program for creating reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
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

#ifdef HAVE_UNAME
#  include <sys/utsname.h>
#endif

#include <reiser4/libreiser4.h>

#include <aux/aux.h>
#include <misc/misc.h>

enum mkfs_behav_flags {
	BF_FORCE      = 1 << 0,
	BF_QUIET      = 1 << 1,
	BF_LOST       = 1 << 2,
	BF_SHOW_PARM  = 1 << 3,
	BF_SHOW_PLUG  = 1 << 4
};

typedef enum mkfs_behav_flags mkfs_behav_flags_t;

/* Prints mkfs options */
static void mkfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] "
		"FILE1 FILE2 ... [ size[K|M|G] ]\n", name);
    
	fprintf(stderr, 
		"Mkfs options:\n"
		"  -s, --lost-found              forces mkfs to create lost+found\n"
		"                                directory.\n"
		"  -b, --block-size N            block size, 4096 by default, other\n"
		"                                are not supported at the moment.\n"
		"  -U, --uuid UUID               universally unique identifier.\n"
		"  -L, --label LABEL             volume label lets to mount\n"
		"                                filesystem by its label.\n"
		"Plugins options:\n"
		"  -p, --print-profile           prints the plugin profile.\n"
		"  -l, --print-plugins           prints all known plugins.\n"
	        "  -o, --override TYPE=PLUGIN    overrides the default plugin of the type\n"
	        "                                \"TYPE\" by the plugin \"PLUGIN\" in the\n"
		"                                profile.\n"
		"Common options:\n"
		"  -?, -h, --help                prints program usage.\n"
		"  -V, --version                 prints current version.\n"
		"  -q, --quiet                   forces creating filesystem without\n"
		"                                any questions.\n"
		"  -f, --force                   makes mkfs to use whole disk, not\n"
		"                                block device or mounted partition.\n");
}

/* Initializes used by mkfs exception streams */
static void mkfs_init(void) {
	int ex;

	/* Setting up exception streams*/
	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
}

static reiser4_object_t *reiser4_root_create(reiser4_fs_t *fs) {
	entry_hint_t entry;
	object_hint_t hint;
	
	aal_assert("vpf-1625", fs != NULL);
	aal_assert("vpf-1626", fs->tree != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));

	/* Preparing object hint. INVAL_PTR means that this kind of plugin 
	   is not ready yet but needs to be stored, */
	hint.mode = 0;
	reiser4_opset_profile(hint.info.opset.plug);
	hint.info.opset.plug[OPSET_OBJ] = hint.info.opset.plug[OPSET_MKDIR];

	/* Preparing entry hint. */
	entry.name[0] = '\0';
	reiser4_key_assign(&entry.offset, &fs->tree->key);

	return reiser4_object_create(fs->tree, &entry, &hint);
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	
	fs_hint_t hint;
	reiser4_fs_t *fs;
	char override[4096];
	aal_device_t *device;

	aal_list_t *walk = NULL;
	aal_gauge_t *gauge = NULL;
	aal_list_t *devices = NULL;
    
	char *host_dev;
	count_t dev_len = 0;
	
#ifdef HAVE_UNAME
	struct utsname sysinfo;
#endif
	
	mkfs_behav_flags_t flags = 0;
    
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"block-size", required_argument, NULL, 'b'},
		{"label", required_argument, NULL, 'L'},
		{"uuid", required_argument, NULL, 'U'},
		{"lost-found", required_argument, NULL, 's'},
		{"print-profile", no_argument, NULL, 'p'},
		{"print-plugins", no_argument, NULL, 'l'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};
    
	mkfs_init();

	if (argc < 2) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}

	hint.blocks = 0;
	hint.blksize = 0;
	
	memset(override, 0, sizeof(override));
	memset(hint.uuid, 0, sizeof(hint.uuid));
	memset(hint.label, 0, sizeof(hint.label));

	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVqfb:U:L:splo:?",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
		case '?':
			mkfs_print_usage(argv[0]);
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
		case 'l':
			flags |= BF_SHOW_PLUG;
			break;
		case 'p':
			flags |= BF_SHOW_PARM;
			break;
		case 's':
			flags |= BF_LOST;
			break;
		case 'o':
			aal_strncat(override, optarg,
				    aal_strlen(optarg));
			
			aal_strncat(override, ",", 1);
			break;
		case 'b':
			/* Parsing blocksize */
			if ((hint.blksize = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid blocksize (%s).", optarg);
				return USER_ERROR;
			}
			
			if (!aal_pow2(hint.blksize)) {
				aal_error("Invalid blocksize (%u). It must power "
					  "of two.", hint.blksize);
				return USER_ERROR;	
			}
			break;
		case 'U':
			/* Parsing passed by user uuid */
			if (aal_strlen(optarg) != 36) {
				aal_error("Invalid uuid was specified (%s).",
					   optarg);
				return USER_ERROR;
			}
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
			{
				if (uuid_parse(optarg, hint.uuid) < 0) {
					aal_error("Invalid uuid was "
						  "specified (%s).",
						  optarg);
					return USER_ERROR;
				}
			}
#endif		
			break;
		case 'L':
			aal_strncpy(hint.label, optarg,
				    sizeof(hint.label));
			break;
		}
	}

	if (optind >= argc + 1) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	if (!(flags & BF_QUIET))
		misc_print_banner(argv[0]);

	/* Initializing libreiser4 (getting plugins, checking them on validness,
	   etc). */
	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Overriding default params by passed values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';

		if (!(flags & BF_QUIET)) {
			aal_mess("Overriding the plugin profile by \"%s\".",
				 override);
		}

		if (misc_profile_override(override))
			goto error_free_libreiser4;
	}

	if (flags & BF_SHOW_PARM)
		misc_profile_print();
	
	if (flags & BF_SHOW_PLUG)
		misc_plugins_print();

	if (optind >= argc)
		goto error_free_libreiser4;
		
#ifdef HAVE_SYSCONF
	/* Guessing block size by getting page size */
	if (!hint.blksize) {
		hint.blksize = sysconf(_SC_PAGESIZE);
	} else {
		if (!(flags & BF_FORCE)) {
			if (hint.blksize != (uint32_t)sysconf(_SC_PAGESIZE)) {
				aal_warn("Block size (%u) and page size "
					 "(%ld) mismatch is detected. "
					 "Reiser4 does not support block "
					 "sizes different than page size "
					 "yet. Use -f to force over.",
					 hint.blksize, sysconf(_SC_PAGESIZE));
				goto error_free_libreiser4;
			}
		}
	}
	
	if (!(flags & BF_QUIET)) {
		aal_mess("Block size %u will be used.", hint.blksize);
	}
#else
	if (!hint.blksize) {
		aal_warn("Can't guess page size. Default "
			 "block size (4096) will be used, "
			 "or use -b option instead.");
		hint.blksize = 4096;
	}
#endif
	
	if (hint.blksize > REISER4_MAX_BLKSIZE) {
		aal_error("Invalid blocksize (%u). It must not be greater then "
			  "%u.", hint.blksize, REISER4_MAX_BLKSIZE);
		return USER_ERROR;	
	}
	
#ifdef HAVE_UNAME
	/* Guessing system type */
	if (uname(&sysinfo) == -1) {
		aal_warn("Can't guess system type.");
		goto error_free_libreiser4;
	}

	if (!(flags & BF_FORCE)) {
		if (aal_strncmp(sysinfo.release, "2.5", 3) &&
		    aal_strncmp(sysinfo.release, "2.6", 3))
		{
			aal_warn("%s %s is detected. Reiser4 does not "
				 "support such a platform. Use -f to "
				 "force over.", sysinfo.sysname,
				 sysinfo.release);
			goto error_free_libreiser4;
		}

	}

	if (!(flags & BF_QUIET)) {
		aal_mess("%s %s is detected.", sysinfo.sysname,
			 sysinfo.release);
	}
#endif

	/* Building list of devices the filesystem will be created on */
	for (; optind < argc; optind++) {
		if (stat(argv[optind], &st) == -1) {

			if (misc_size2long(argv[optind]) != INVAL_DIG &&
			    hint.blocks != 0)
			{
				aal_error("Filesystem length is already "
					  "set to %llu.", hint.blocks);
				continue;
			}
			
			/* Checking device name for validness */
			hint.blocks = misc_size2long(argv[optind]);
			
			if (hint.blocks != INVAL_DIG) {
				/* Converting into fs blocksize blocks */
				hint.blocks /= (hint.blksize / 1024);
				/* Just to know that blocks was given. 0 
				   means nothing was specified by user. */
				if (!hint.blocks) hint.blocks = 1;
			} else {
				aal_error("%s is not a valid size nor an "
					  "existent file.", argv[optind]);
				goto error_free_libreiser4;
			}
		} else {
			devices = aal_list_append(devices, argv[optind]);
		}
	}

	if (!(flags & BF_QUIET) && aal_list_len(devices)) {
		if (!(gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					       NULL, NULL, 0, NULL)))
			goto error_free_libreiser4;
	}
    
	/* The loop through all devices */
	aal_list_foreach_forward(devices, walk) {
    
		host_dev = (char *)walk->data;
    
		if (stat(host_dev, &st) == -1) {
			aal_error("Can't stat %s. %s.", host_dev,
				  strerror(errno));
			goto error_free_libreiser4;
		}
    
		/* Checking is passed device is a block device. If so, we check
		   also is it whole drive or just a partition. If the device is
		   not a block device, then we emmit exception and propose user
		   to use -f flag to force. */
		if (!S_ISBLK(st.st_mode)) {
			if (!(flags & BF_FORCE)) {
				aal_error("Device %s is not block "
					  "device. Use -f to force "
					  "over.", host_dev);
				goto error_free_libreiser4;
			}
		} else {
			if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) &&
			      MINOR(st.st_rdev) % 64 == 0) ||
			     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) &&
			      MINOR(st.st_rdev) % 16 == 0)) &&
			    !(flags & BF_FORCE))
			{
				aal_error("Device %s is an entire "
					  "harddrive, not just one "
					  "partition.", host_dev);
				goto error_free_libreiser4;
			}
		}
   
		/* Checking if passed partition is mounted */
		if (misc_dev_mounted(host_dev) > 0 && !(flags & BF_FORCE)) {
			aal_error("Device %s is mounted at the moment. "
				  "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}

		/* Generating uuid if it was not specified and if libuuid is in use */
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
		if (uuid_is_null(hint.uuid)) {
			uuid_generate(hint.uuid);
		}

		if (!(flags & BF_QUIET)) {
			char uuid[256];
				
			uuid_unparse(hint.uuid, uuid);
			aal_mess("Uuid %s will be used.", uuid);
		}
#endif
		/* Opening device */
		if (!(device = aal_device_open(&file_ops, host_dev, 
					       512, O_RDWR))) 
		{
			aal_error("Can't open %s. %s.",
				  host_dev, strerror(errno));
			goto error_free_libreiser4;
		}
    
		/* Converting device length into fs blocksize blocks */
		dev_len = aal_device_len(device) /
			(hint.blksize / device->blksize);
    
		if (!hint.blocks)
			hint.blocks = dev_len;
	
		if (hint.blocks > dev_len) {
			aal_error("Filesystem wouldn't fit into device "
				  "%llu blocks long, %llu blocks required.",
				  dev_len, hint.blocks);
			goto error_free_device;
		}

		/* Checking for "quiet" mode */
		if (!(flags & BF_QUIET)) {
			if (aal_yesno("Reiser4 is going to be created on %s.",
				      host_dev) == EXCEPTION_OPT_NO)
			{
				goto error_free_device;
			}
		}
    
		if (gauge) {
			aal_gauge_rename(gauge, "Creating reiser4 on %s ... ",
					 host_dev);
			aal_gauge_touch(gauge);
		}

		/* Creating filesystem */
		if (!(fs = reiser4_fs_create(device, &hint))) {
			aal_error("Can't create filesystem on %s.", 
				  device->name);
			goto error_free_device;
		}

		/* Creating journal */
		if (!(fs->journal = reiser4_journal_create(fs, device)))
			goto error_free_fs;

		/* Creating root directory */
		if (!(fs->root = reiser4_root_create(fs))) {
			aal_error("Can't create filesystem "
				  "root directory.");
			goto error_free_journal;
		}

		/* Linking root to itself */
		if (reiser4_object_link(fs->root, fs->root, NULL)) {
			aal_error("Can't link root directory "
				  "to itself.");
			goto error_free_journal;
		}
	
		if (reiser4_opset_init(fs->tree, 1)) {
			aal_error("Can't initialize the fs-global "
				  "object plugin set.");
			goto error_free_journal;
		}

		/* Creating lost+found directory */
		if (flags & BF_LOST) {
			reiser4_object_t *object;
	    
			if (!(object = reiser4_dir_create(fs, fs->root,
							  "lost+found")))
			{
				aal_error("Can't create \"/lost+found\" "
					  "directory.");
				goto error_free_root;
			}
	    
			reiser4_object_close(object);
		}
	
		if (gauge)
			aal_gauge_done(gauge);

		/* Zeroing uuid in order to force mkfs to generate it on its own
		   for next device form built device list. */
		aal_memset(hint.uuid, 0, sizeof(hint.uuid));

		/* Zeroing out label, because all filesystems cannot have the
		   same label. */
		aal_memset(hint.label, 0, sizeof(hint.label));

		/* Zeroing fs_len in order to force mkfs on next turn to calc
		   its size from actual device length. */
		hint.blocks = 0;
	
		/* Freeing the root directory */
		reiser4_object_close(fs->root);

		/* Freeing journal */
		reiser4_journal_close(fs->journal);

		/* Freeing the filesystem instance and device instance */
		reiser4_fs_close(fs);

		/* Synchronizing device. If device we are using is a file device
		   (libaal/file.c), then function fsync will be called. */
		if (aal_device_sync(device)) {
			aal_error("Can't synchronize device %s.", 
				  device->name);
			goto error_free_device;
		}

		aal_device_close(device);
	}
    
	/* Freeing the all used objects */
	if (gauge)
		aal_gauge_free(gauge);
	
	aal_list_free(devices);

	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   during this. */
	libreiser4_fini();
    
	return NO_ERROR;

 error_free_root:
	reiser4_object_close(fs->root);
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
