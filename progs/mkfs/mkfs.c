/*
  mkfs.c -- program for creating reiser4 filesystem.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

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

#include <reiser4/reiser4.h>

#include <aux/aux.h>
#include <misc/misc.h>

enum mkfs_behav_flags {
	BF_FORCE   = 1 << 0,
	BF_QUIET   = 1 << 1,
	BF_PLUGS   = 1 << 2,
	BF_LOST    = 1 << 3
};

typedef enum mkfs_behav_flags mkfs_behav_flags_t;

/* Prints mkfs options */
static void mkfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] "
		"FILE1 FILE2 ... [ size[K|M|G] ]\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -V, --version                   prints current version.\n"
		"  -q, --quiet                     forces creating filesystem without\n"
		"                                  any questions.\n"
		"  -f, --force                     makes mkfs to use whole disk, not\n"
		"                                  block device or mounted partition.\n"
		"  -s, --lost-found                forces mkfs to create lost+found\n"
		"                                  directory.\n"
		"  -b, --block-size N              block size, 4096 by default, other\n"
		"                                  are not supported at the moment.\n"
		"  -i, --uuid UUID                 universally unique identifier.\n"
		"  -l, --label LABEL               volume label lets to mount\n"
		"                                  filesystem by its label.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE           profile to be used.\n"
		"  -P, --known-plugins             prints known plugins.\n"
		"  -K, --known-profiles            prints known profiles.\n"
	        "  -o, --override TYPE=PLUGIN      overrides the default plugin of the type\n"
	        "                                  \"TYPE\" by the plugin \"PLUGIN\".\n");
}

/* Initializes used by mkfs exception streams */
static void mkfs_init(void) {
	int ex;

	/* Setting up exception streams*/
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

/* Crates directory */
static reiser4_object_t *mkfs_create_dir(reiser4_fs_t *fs,
		                       const char *name,
				       reiser4_object_t *parent,
				       reiser4_profile_t *profile)
{
	pid_t hash;
	pid_t statdata;
	pid_t direntry;
	pid_t directory;

	reiser4_object_t *object;
	reiser4_object_hint_t hint;

	directory = reiser4_profile_value(profile, "directory");
	
	/* Preparing object hint */
	hint.plugin = libreiser4_factory_ifind(OBJECT_PLUGIN_TYPE, 
					       directory);

	if (!hint.plugin) {
		aal_exception_error("Can't find dir plugin by its id 0x%x.", 
				    directory);
		return NULL;
	}
    
	hint.statdata = reiser4_profile_value(profile, "statdata");
	hint.body.dir.hash = reiser4_profile_value(profile, "hash");
	hint.body.dir.direntry = reiser4_profile_value(profile, "direntry");

	/* Creating directory by passed parameters */
	if (!(object = reiser4_object_create(fs, parent, &hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, name))
			return NULL;
	}

	return object;
}

int main(int argc, char *argv[]) {
	int c;
	
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	aal_gauge_t *gauge;
	aal_list_t *walk = NULL;
	aal_list_t *devices = NULL;
    
	struct stat st;
	char override[4096];
	char uuid[17], label[17];
	mkfs_behav_flags_t flags = 0;
	count_t fs_len = 0, dev_len = 0;
	uint16_t blocksize = REISER4_BLKSIZE;

	char *host_dev, *profile_label = "smart40";
    
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"block-size", required_argument, NULL, 'b'},
		{"label", required_argument, NULL, 'l'},
		{"uuid", required_argument, NULL, 'i'},
		{"lost-found", required_argument, NULL, 's'},
		{"profile", required_argument, NULL, 'e'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"known-plugins", no_argument, NULL, 'P'},
		{"override", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
	};
    
	mkfs_init();

	if (argc < 2) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}

	memset(uuid, 0, sizeof(uuid));
	memset(label, 0, sizeof(label));
	memset(override, 0, sizeof(override));

	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVe:qfKb:i:l:sPo:", long_options, 
				(int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			mkfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			progs_print_banner(argv[0]);
			return NO_ERROR;
		case 'e':
			profile_label = optarg;
			break;
		case 'f':
			flags |= BF_FORCE;
			break;
		case 'q':
			flags |= BF_QUIET;
			break;
		case 'P':
			flags |= BF_PLUGS;
			break;
		case 's':
			flags |= BF_LOST;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
			break;
		case 'K':
			progs_print_banner(argv[0]);
			progs_profile_list();
			return NO_ERROR;
		case 'b':
			/* Parsing blocksize */
			if ((blocksize = (uint16_t)progs_str2long(optarg, 10)) == INVAL_DIG) {
				aal_exception_error("Invalid blocksize (%s).", optarg);
				return USER_ERROR;
			}
			
			if (!aal_pow_of_two(blocksize)) {
				aal_exception_error("Invalid block size %u. "
						    "It must power of two.",
						    (uint16_t)blocksize);
				return USER_ERROR;	
			}
			break;
		case 'i':
			/* Parsing passed by user uuid */
			if (aal_strlen(optarg) != 36) {
				aal_exception_error("Invalid uuid was "
						    "specified (%s).", optarg);
				return USER_ERROR;
			}
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
			{
				if (uuid_parse(optarg, uuid) < 0) {
					aal_exception_error("Invalid uuid was "
							    "specified (%s).", optarg);
					return USER_ERROR;
				}
			}
#endif		
			break;
		case 'l':
			aal_strncpy(label, optarg, sizeof(label) - 1);
			break;
		case '?':
			mkfs_print_usage(argv[0]);
			return USER_ERROR;
		}
	}
    
	if (optind >= argc + 1) {
		mkfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	progs_print_banner(argv[0]);
    
	/* Initializing passed profile */
	if (!(profile = progs_profile_find(profile_label))) {
		aal_exception_error("Can't find profile by its label %s.", 
				    profile_label);
		goto error;
	}

	/*
	  Initializing libreiser4 (getting plugins, checking them on validness,
	  etc).
	*/
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	if (flags & BF_PLUGS) {
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
	
	/* Building list of devices the filesystem will be created on */
	for (; optind < argc; optind++) {
		if (stat(argv[optind], &st) == -1) {

			/* Checking device name for validness */
			fs_len = (progs_size2long(argv[optind]));
			
			if (fs_len != INVAL_DIG) {
				if (fs_len < blocksize) {
					aal_exception_error("%s is not a valid "
							    "filesystem size.",
							    argv[optind]);
					goto error_free_libreiser4;
				}
				
				fs_len /= blocksize;
			} else {
				aal_exception_error("%s is not a valid size nor an "
						    "existent file.", argv[optind]);
				goto error_free_libreiser4;
			}
		} else
			devices = aal_list_append(devices, argv[optind]);
	}

	/*
	  All devices cannot have the same uuid and label, so here we clean it
	  out.
	*/
	if (aal_list_length(devices) > 1) {
		aal_memset(uuid, 0, sizeof(uuid));
		aal_memset(label, 0, sizeof(label));
	}
    
	if (!(gauge = aal_gauge_create(progs_gauge_silent_handler, "", NULL)))
		goto error_free_libreiser4;
    
	/* The loop through all devices */
	aal_list_foreach_forward(devices, walk) {
    
		host_dev = (char *)walk->data;
    
		if (stat(host_dev, &st) == -1)
			goto error_free_libreiser4;
    
		/* 
		   Checking is passed device is a block device. If so, we check
		   also is it whole drive or just a partition. If the device is
		   not a block device, then we emmit exception and propose user
		   to use -f flag to force.
		*/
		if (!S_ISBLK(st.st_mode)) {
			if (!(flags & BF_FORCE)) {
				aal_exception_error("Device %s is not block device. "
						    "Use -f to force over.", host_dev);
				goto error_free_libreiser4;
			}
		} else {
			if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
			     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
			    !(flags & BF_FORCE))
			{
				aal_exception_error("Device %s is an entire harddrive, not "
						    "just one partition.", host_dev);
				goto error_free_libreiser4;
			}
		}
   
		/* Checking if passed partition is mounted */
		if (progs_dev_mounted(host_dev, NULL) && !(flags & BF_FORCE)) {
			aal_exception_error("Device %s is mounted at the moment. "
					    "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}

		/* Generating uuid if it was not specified and if libuuid is in use */
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
		if (aal_strlen(uuid) == 0)
			uuid_generate(uuid);
#endif
		/* Opening device */
		if (!(device = aal_device_open(&file_ops, host_dev, blocksize, O_RDWR))) {
			char *error = strerror(errno);
	
			aal_exception_error("Can't open %s. %s.", host_dev, error);
			goto error_free_libreiser4;
		}
    
		/* Preparing filesystem length */
		dev_len = aal_device_len(device);
    
		if (!fs_len)
			fs_len = dev_len;
	
		if (fs_len > dev_len) {
			aal_exception_error("Filesystem wouldn't fit into device "
					    "%llu blocks long, %llu blocks required.",
					    dev_len, fs_len);
			goto error_free_device;
		}

		/* Checking for "quiet" mode */
		if (!(flags & BF_QUIET)) {
			if (aal_exception_yesno("Reiser4 with %s profile "
						"is going to be created "
						"on %s.", profile_label,
						host_dev) == EXCEPTION_NO)
				goto error_free_device;
		}
    
		aal_gauge_rename(gauge, "Creating reiser4 with %s on %s",
				 profile->name, host_dev);

		aal_gauge_start(gauge);

		/* Creating filesystem */
		if (!(fs = reiser4_fs_create(device, uuid, label,
					     profile, fs_len))) 
		{
			aal_exception_error("Can't create filesystem on %s.", 
					    aal_device_name(device));
			goto error_free_device;
		}

		/* Creating journal */
		if (!(fs->journal = reiser4_journal_create(fs, device, NULL)))
			goto error_free_fs;

		/* Creating tree */
		if (!(fs->tree = reiser4_tree_init(fs)))
			goto error_free_journal;
    
		/* Creating root directory */
		if (!(fs->root = mkfs_create_dir(fs, NULL, NULL, profile))) {
			aal_exception_error("Can't create filesystem root "
					    "directory.");
			goto error_free_tree;
		}
	
		/* Creating lost+found directory */
		if (flags & BF_LOST) {
			reiser4_object_t *object;
	    
			if (!(object = mkfs_create_dir(fs, "lost+found",
						       fs->root, profile)))
			{
				aal_exception_error("Can't create lost+found "
						    "directory.");
				goto error_free_root;
			}
	    
			reiser4_object_close(object);
		}
	
		aal_gauge_done(gauge);
	
		aal_gauge_rename(gauge, "Synchronizing %s", host_dev);
		aal_gauge_start(gauge);
	
		/* 
		   Synchronizing device. If device we are using is a file_device
		   (libaal/file.c), then function fsync will be called.
		*/
		if (aal_device_sync(device)) {
			aal_exception_error("Can't synchronize device %s.", 
					    aal_device_name(device));
			goto error_free_root;
		}

		/* 
		   Zeroing uuid in order to force mkfs to generate it on its own
		   for next device form built device list.
		*/
		aal_memset(uuid, 0, sizeof(uuid));

		/* 
		   Zeroing fs_len in order to force mkfs on next turn to calc
		   its size from actual device length.
		*/
		fs_len = 0;
	
		aal_gauge_done(gauge);

		/* Freeing the root directory */
		reiser4_object_close(fs->root);

		/* Freeing tree */
		reiser4_tree_close(fs->tree);
		
		/* Freeing journal */
		reiser4_journal_close(fs->journal);

		/* Freeing the filesystem instance and device instance */
		reiser4_fs_close(fs);
		aal_device_close(device);
	}
    
	/* Freeing the all used objects */
	aal_gauge_free(gauge);
	aal_list_free(devices);

	/* 
	   Deinitializing libreiser4. At the moment only plugins are unloading
	   durrign this.
	*/
	libreiser4_fini();
    
	return NO_ERROR;

 error_free_root:
	reiser4_object_close(fs->root);
 error_free_tree:
	reiser4_tree_close(fs->tree);
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

