/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mkfs.c -- program for creating reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#ifndef ENABLE_MINIMAL
#  include <time.h>
#  include <stdlib.h>
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

typedef enum mkfs_behav_flags {
	BF_FORCE      = 1 << 0,
	BF_YES        = 1 << 1,
	BF_LOST       = 1 << 2,
	BF_SHOW_PARM  = 1 << 3,
	BF_SHOW_PLUG  = 1 << 4,
	BF_DISCARD    = 1 << 5,
	BF_MIRRORS    = 1 << 6
} mkfs_behav_flags_t;

/* Prints mkfs options */
static void mkfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] "
		"FILE1 FILE2 ... [ size[K|M|G] ]\n", name);

	fprintf(stderr, 
		"Mkfs options:\n"
		"  -s, --lost-found              forces mkfs to create lost+found\n"
		"                                directory.\n"
		"  -a, --data-brick              forces mkfs to create data bricks.\n"
		"  -b, --block-size N            block size, 4096 by default, other\n"
		"                                are not supported at the moment.\n"
		"  -t, --stripe-size N           stripe size [K|M|G].\n"
		"  -n, --max-bricks N            current maximum allowed number of bricks\n"
		"  -r, --data-room-size N        data room size in file system blocks.\n"
		"                                in the logical volume.\n"
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
		"  -y, --yes                     assumes an answer 'yes' to all questions.\n"
		"  -f, --force                   makes mkfs to use whole disk, not\n"
		"                                block device or mounted partition.\n"
		"  -d, --discard                 tells mkfs to discard given device\n"
		"                                before creating the filesystem (for SSDs)\n"
		"  -m, --mirrors                 create mirrors on listed devices.\n");
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
	object_info_t info;
	
	aal_assert("vpf-1625", fs != NULL);
	aal_assert("vpf-1626", fs->tree != NULL);
	
	aal_memset(&info, 0, sizeof(info));

	info.tree = (tree_entity_t *)fs->tree;

	/* Preparing entry hint. */
	entry.name[0] = '\0';
	aal_memcpy(&entry.offset, &fs->tree->key, sizeof(entry.offset));

	reiser4_pset_root(&info);
	
	return reiser4_object_create(&entry, &info, NULL);
}

static int advise_stripe_size(fs_hint_t *hint, int is_default, int forced)
{
	return plugcall((reiser4_vol_plug_t *)reiser4_profile_plug(PROF_VOL),
			advise_stripe_size, &hint->stripe_size,
			hint->blksize, hint->blocks, is_default,
			forced);
}

static int advise_max_bricks(fs_hint_t *hint, int forced)
{
	return plugcall((reiser4_vol_plug_t *)reiser4_profile_plug(PROF_VOL),
			advise_max_bricks, &hint->max_bricks, forced);
}

static int check_data_room_size(fs_hint_t *hint, int forced)
{
	return plugcall((reiser4_vol_plug_t *)reiser4_profile_plug(PROF_VOL),
			check_data_room_size, hint->data_room_size,
			hint->blocks, forced);
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
	uint32_t dev_cnt = 0;
	
#ifdef HAVE_UNAME
	struct utsname sysinfo;
#endif
	int default_stripe = 1;
	mkfs_behav_flags_t flags = 0;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"yes", no_argument, NULL, 'y'},
		{"block-size", required_argument, NULL, 'b'},
		{"stripe-size", required_argument, NULL, 't'},
		{"data-brick", no_argument, NULL, 'a'},
		{"data-room-size", required_argument, NULL, 'r'},
		{"label", required_argument, NULL, 'L'},
		{"uuid", required_argument, NULL, 'U'},
		{"lost-found", required_argument, NULL, 's'},
		{"print-profile", no_argument, NULL, 'p'},
		{"print-plugins", no_argument, NULL, 'l'},
		{"override", required_argument, NULL, 'o'},
		{"discard", no_argument, NULL, 'd'},
		{"mirrors", no_argument, NULL, 'm'},
		{"max-bricks", required_argument, NULL, 'n'},
		{0, 0, 0, 0}
	};
    
	mkfs_init();

	if (argc < 2) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}

	memset(&hint, 0, sizeof(hint));
	memset(override, 0, sizeof(override));

	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVyfb:t:U:L:n:r:spalo:dm?",
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
		case 'y':
			flags |= BF_YES;
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
		case 'd':
			flags |= BF_DISCARD;
			break;
		case 'm':
			flags |= BF_MIRRORS;
			break;
		case 'a':
			hint.is_data_brick = 1;
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
		case 't':
			/* Parsing stripe size */
			if ((hint.stripe_size = misc_size2long(optarg)) == INVAL_DIG) {
				aal_error("Invalid stripe size (%s).", optarg);
				return USER_ERROR;
			}
			default_stripe = 0;
			break;
		case 'n':
			/* Parsing max bricks */
			if ((hint.max_bricks = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid max bricks (%s).", optarg);
				return USER_ERROR;
			}
			break;
		case 'r':
			/* Parsing data room size */
			if ((hint.data_room_size = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid data room size (%s).", optarg);
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
			  if (uuid_parse(optarg,
					 (unsigned char *)hint.volume_uuid) < 0) {
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

	if (!(flags & BF_YES))
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

		if (!(flags & BF_YES)) {
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
	
	if (optind >= argc) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    

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
	
	if (!(flags & BF_YES)) {
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
		aal_error("Invalid blocksize (%u). It must not be greater than "
			  "%u.", hint.blksize, REISER4_MAX_BLKSIZE);
		goto error_free_libreiser4;
	}
	if (advise_max_bricks(&hint, flags & BF_FORCE) < 0)
		goto error_free_libreiser4;

#ifdef HAVE_UNAME
	/* Guessing system type */
	if (uname(&sysinfo) == -1) {
		aal_warn("Can't guess system type.");
		goto error_free_libreiser4;
	}

	if (!(flags & BF_YES)) {
		aal_mess("%s %s is detected.", sysinfo.sysname,
			 sysinfo.release);
	}
#endif
	/*
	 * Building list of devices the filesystem will be created on
	 */
	for (; optind < argc; optind++) {
		if (stat(argv[optind], &st) == -1) {
			aal_error("Can't stat %s. %s.", argv[optind],
				  strerror(errno));
			goto error_free_libreiser4;
		} else {
			devices = aal_list_append(devices, argv[optind]);
			dev_cnt ++;
		}
	}
	/*
	 * Set number of subvolumes and mirrors
	 */
	hint.num_subvols = 1;

	if ((flags & BF_MIRRORS) && (dev_cnt != 0))
		hint.num_replicas = dev_cnt - 1;
	else
		hint.num_replicas = 0;
	dev_cnt = 0;
	if (!(flags & BF_YES) && aal_list_len(devices)) {
		if (!(gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					       NULL, NULL, 0, NULL)))
			goto error_free_libreiser4;
	}
  	/*
	 * Loop through all the accumulated devices
	 */
	aal_list_foreach_forward(devices, walk) {
    
		host_dev = (char *)walk->data;
    
		if (stat(host_dev, &st) == -1) {
			aal_error("Can't stat %s. %s.", host_dev,
				  strerror(errno));
			goto error_free_libreiser4;
		}
		/*
		 * Check if passed device is a block device. If so, we check
		 * also is it whole drive or just a partition. If the device is
		 * not a block device, then we emmit exception and propose user
		 * to use -f flag to force
		 */
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
   		/*
		 * Check if passed partition is mounted
		 */
		if (misc_dev_mounted(host_dev) > 0 && !(flags & BF_FORCE)) {
			aal_error("Device %s is mounted at the moment. "
				  "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}
		/*
		 * Generate uuid if it was not specified and
		 * if libuuid is in use
		 */
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
		if (uuid_is_null((unsigned char *)hint.volume_uuid)) {
			uuid_generate((unsigned char *)hint.volume_uuid);
		}
		if (uuid_is_null((unsigned char *)hint.subvol_uuid)) {
			uuid_generate((unsigned char *)hint.subvol_uuid);
		}
		if (!(flags & BF_YES)) {
			char uuid[256];

			uuid_unparse((unsigned char *)hint.volume_uuid, uuid);
			aal_mess("Volume uuid %s will be used.", uuid);
		}
#else
		if (flags & BF_MIRRORS ||
		    hint.is_data_brick ||
		    ((reiser4_vol_plug_t *)(reiser4_profile_plug(PROF_VOL)))->
		    p.id.id == VOL_ASYM_ID) {
			aal_error("uuid is required to create logical volumes");
			goto error_free_libreiser4;
		}
#endif
		/*
		 * Initialize mkfs_id for fsck needs
		 */
		if (hint.mkfs_id == 0) {
			srandom(time(0) + dev_cnt);
			hint.mkfs_id = random();
		}
		if (flags & BF_MIRRORS)
			hint.mirror_id = dev_cnt;

		if (!(device = aal_device_open(&file_ops, host_dev, 
					       512, O_RDWR))) 
		{
			aal_error("Can't open %s. %s.",
				  host_dev, strerror(errno));
			goto error_free_libreiser4;
		}
    		/*
		 * Convert device length into fs blocksize blocks
		 */
		dev_len = reiser4_format_len(device, hint.blksize);

		if (flags & BF_MIRRORS &&
		    hint.blocks != 0 && hint.blocks != dev_len) {
			aal_error("Mirror can be created only on a device "
				  "of the same length as the original has.");
			goto error_free_device;
		}
		if (hint.blocks > dev_len) {
			aal_error("Filesystem wouldn't fit into device "
				  "%llu blocks long, %llu blocks required.",
				  dev_len, hint.blocks);
			goto error_free_device;
		}
		if (hint.blocks == 0)
			hint.blocks = dev_len;

		if (advise_stripe_size(&hint,
				       default_stripe, flags & BF_FORCE) < 0)
			goto error_free_device;

		if (check_data_room_size(&hint, flags & BF_FORCE) < 0)
			goto error_free_device;
		/*
		 * Check for non-intercative mode
		 */
		if (!(flags & BF_YES)) {
			if (aal_yesno("Reiser4 is going to be created on %s.",
				      host_dev) == EXCEPTION_OPT_NO)
			{
				goto error_free_device;
			}
		}
		if (flags & BF_DISCARD) {
			if (gauge) {
				aal_gauge_rename(gauge, "Discarding %s ... ",
						 host_dev);
				aal_gauge_touch(gauge);
			}

			if ((aal_device_discard(device, 0, dev_len) != 0)) {
				aal_error("Failed to discard %s (%s).",
					  device->name, device->error);
				/* discard is optional, don't fail the mkfs */
			}

			if (gauge) {
				aal_gauge_done(gauge);
			}
		}

		if (gauge) {
			aal_gauge_rename(gauge, "Creating reiser4 on %s ... ",
					 host_dev);
			aal_gauge_touch(gauge);
		}
		/*
		 * Create a filesystem on the device
		 */
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
			goto error_free_fs;
		}

		/* Linking root to itself */
		if (reiser4_object_link(fs->root, fs->root, NULL)) {
			aal_error("Can't link root directory "
				  "to itself.");
			goto error_free_fs;
		}
	
		if (reiser4_pset_tree(fs->tree, 1)) {
			aal_error("Can't initialize the fs-global "
				  "object plugin set.");
			goto error_free_fs;
		}

		/* Set data room size calculated by the rest of free space */
		reiser4_set_data_room(fs, &hint);

		/* Backup the fs metadata. */
		if (!(fs->backup = reiser4_backup_create(fs))) {
			aal_error("Can't create the fs metadata backup.");
			goto error_free_fs;
		}

		/* Creating lost+found directory */
		if ((flags & BF_LOST) && !hint.is_data_brick) {
			reiser4_object_t *object;
	    
			if (!(object = reiser4_dir_create(fs->root,
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

		if (!(flags & BF_MIRRORS)) {
			/*
			 * The following parameters are independent,
			 * so we zero them in order to force mkfs to
			 * generate different ones for next device
			 * form built device list
			 */
			aal_memset(hint.volume_uuid, 0, sizeof(hint.volume_uuid));
			aal_memset(hint.label, 0, sizeof(hint.label));
			hint.mkfs_id = 0;
			hint.blocks = 0;
		}
		aal_memset(hint.subvol_uuid, 0, sizeof(hint.subvol_uuid));
		/*
		 * Free the root directory & fs
		 */
		reiser4_object_close(fs->root);
		reiser4_fs_close(fs);
		/*
		 * Synchronizing device.
		 * If device we are using is a file device (libaal/file.c),
		 * then function fsync will be called
		 */
		if (aal_device_sync(device)) {
			aal_error("Can't synchronize device %s.", 
				  device->name);
			goto error_free_device;
		}

		aal_device_close(device);
		dev_cnt ++;
	}
    
	/* Freeing the all used objects */
	if (gauge)
		aal_gauge_free(gauge);
	
	aal_list_free(devices, NULL, NULL);

	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   during this. */
	libreiser4_fini();
    
	return NO_ERROR;

 error_free_root:
	reiser4_object_close(fs->root);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}
