/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   fsck.c -- reiser4 filesystem checking and recovering program. */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <fsck.h>

static void fsck_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);

	fprintf(stderr, 
		"Fsck options:\n"
		"  --check                       checks the consistency (default)\n"
		"  --fix                         fixes minor corruptions\n"
		"  --build-sb                    rebuilds the super block\n"
		"  --build-fs                    rebuilds the filesystem\n"
		"\n"
		"  -L, --logfile file            complains into the file\n"
		"  -n, --no-log                  makes fsck to not complain\n"
		"  -a, --auto                    automatically checks the consistency\n"
		"                                without any questions.\n"
		"  -v, --verbose                 makes fsck to be verbose\n"
		"  -r                            ignored\n"
		"Plugins options:\n"
		"  -p, --print-profile           prints the plugin profile.\n"
		"  -l, --print-plugins           prints all known plugins.\n"
		"  -o, --override TYPE=PLUGIN    overrides the default plugin of the type\n"
	        "                                \"TYPE\" by the plugin \"PLUGIN\" in the\n"
		"                                profile.\n"
		"Common options:\n"
		"  -?, -h, --help                prints program usage.\n"
		"  -V, --version                 prints current version.\n"
		"  -q, --quiet                   supresses progress information.\n"
		"  -f, --force                   makes fsck to use whole disk, not block\n"
		"                                device or mounted partition.\n"
		"  -c, --cache N                 number of nodes in tree buffer cache\n");
}

#define WARNING \
"*******************************************************************\n"\
"This is an EXPERIMENTAL version of fsck.reiser4. Read README first.\n"\
"*******************************************************************\n"

static errno_t fsck_ask_confirmation(fsck_parse_t *data, char *host_name) {
	fprintf(stderr, WARNING "\n");
	aal_mess("Fscking the %s block device.", host_name);
	
	switch (data->sb_mode) {
	case RM_CHECK:
		aal_mess("Will check the consistency of "
			 "the Reiser4 SuperBlock.");
		break;
	case RM_FIX:
		aal_mess("Will fix minor corruptions of "
			 "the Reiser4 SuperBblock.");
		break;
	case RM_BUILD:
		aal_mess("Will build the Reiser4 SuperBlock.");
		break;
	default:
		break;
	}
	
	switch (data->fs_mode) {
	case RM_CHECK:
		aal_mess("Will check the consistency of "
			 "the Reiser4 FileSystem.");
		break;
	case RM_FIX:
		aal_mess("Will fix minor corruptions of "
			 "the Reiser4 FileSystem.");
		break;
	case RM_BUILD:
		aal_mess("Will build the Reiser4 FileSystem.");
		break;
	case RM_BACK:
		aal_mess("Will rollback changes saved in '%s' back "
			"onto (%s).", data->backup_file, host_name);
		break;
	default:
		break;
	}
	
	if (aal_yesno("Continue?") == EXCEPTION_OPT_NO) 
		return USER_ERROR;
     
	return NO_ERROR; 
}

static void fsck_init_streams() {
	int ex;

	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
	
	misc_exception_set_stream(EXCEPTION_TYPE_INFO, stdout);
}

enum fsck_mode {
	RM_SHOW_PARM = RM_LAST,
	RM_SHOW_PLUG = RM_LAST + 1
};

static errno_t fsck_init(fsck_parse_t *data, 
			 struct aal_device_ops *ops, 
			 int argc, char *argv[]) 
{
	static int mode = RM_CHECK, sb_mode = 0, fs_mode = 0;
	FILE *stream = NULL;
	char override[4096];
	int option_index;
	errno_t ret = 0;
	uint32_t cache;
	int mounted, c;

	static struct option options[] = {
		/* FSCK modes. */
		{"check", no_argument, &mode, RM_CHECK},
		{"fix", no_argument, &mode, RM_FIX},
		{"build-sb", no_argument, &sb_mode, RM_BUILD},
		{"build-fs", no_argument, &fs_mode, RM_BUILD},
		{"print-profile", no_argument, &mode, RM_SHOW_PARM},
		{"print-plugins", no_argument, &mode, RM_SHOW_PLUG},
		/* Fsck hidden modes. */
		{"rollback", no_argument, &mode, RM_BACK},
		/* Fsck options */
		{"logfile", required_argument, 0, 'L'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"no-log", no_argument, NULL, 'n'},
		{"auto", no_argument, NULL, 'a'},
		{"force", no_argument, NULL, 'f'},
		{"verbose", no_argument, NULL, 'v'},
		{"cashe", required_argument, 0, 'c'},
		{"override", required_argument, NULL, 'o'},
		/* Fsck hidden options. */
		{"passes-dump", required_argument, 0, 'U'},
		{"backup", required_argument, 0, 'b'},
		{"bitmap", required_argument, 0, 'B'},
		{0, 0, 0, 0}
	};

	fsck_init_streams();
	aux_gauge_set_handler(misc_progress_handler, GT_PROGRESS);
	memset(override, 0, sizeof(override));
	data->logfile = stderr;

	if (argc < 2) {
		fsck_print_usage(argv[0]);
		return USER_ERROR;
	}

	while ((c = getopt_long(argc, argv, "L:VhnqafvU:b:r?dB:plo:c:", 
				options, &option_index)) >= 0) 
	{
		switch (c) {
		case 0:
			/* Long options. */
			break;
		case 'L':
			if ((stream = fopen(optarg, "w")) == NULL) {
				aal_fatal("Cannot not open the "
					  "logfile (%s).", optarg);
				goto oper_error;
			} 
			
			data->logfile = stream;		
			break;
		case 'n':
			data->logfile = NULL;
			break;
		case 'U':
			break;
		case 'b':
			data->backup_file = optarg;
			
			break;
		case 'f':
			aal_set_bit(&data->options, FSCK_OPT_FORCE);
			break;
		case 'a':
			aal_set_bit(&data->options, FSCK_OPT_AUTO);
			break;
		case 'v':
			aal_set_bit(&data->options, FSCK_OPT_VERBOSE);
			break;
		case 'h': 
		case '?':
			fsck_print_usage(argv[0]);
			goto user_error;
		case 'V': 
			misc_print_banner(argv[0]);
			goto user_error;
		case 'q':
			aux_gauge_set_handler(NULL, GT_PROGRESS);
			break;
		case 'r':
			break;
		case 'd':
			aal_set_bit(&data->options, FSCK_OPT_DEBUG);
			break;
		case 'B':
			data->bitmap_file = optarg;
			break;
		case 'c':
			if ((cache = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_fatal("Invalid cache value specified (%s).",
					  optarg);
				return USER_ERROR;
			}

			misc_mpressure_setup(cache);
			break;
		case 'l':
			mode = RM_SHOW_PLUG;
			break;
		case 'p':
			mode = RM_SHOW_PARM;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
			break;
		}
	}
	
	/* Initializing libreiser4 with factory sanity check */
	if (libreiser4_init()) {
		aal_fatal("Cannot initialize the libreiser4.");
		goto oper_error;
	}

	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';

		aal_mess("Overriding the plugin profile by \"%s\".", 
			 override);
		
		if (misc_profile_override(override))
			goto oper_error;
	}

	if (mode == RM_SHOW_PARM) {
		misc_profile_print();
		goto user_error;
	}

	if (mode == RM_SHOW_PLUG) {
		misc_plugins_print();
		goto user_error;
	}

	if (optind != argc - 1) {
		fsck_print_usage(argv[0]);
		goto user_error;
	}
	
	if (aal_test_bit(&data->options, FSCK_OPT_AUTO)) {
		aal_mess("%s %s", argv[0], argv[optind]);
		exit(NO_ERROR);
	}

	data->sb_mode = sb_mode ? sb_mode : mode;
	data->fs_mode = fs_mode ? fs_mode : mode;
  
	if (data->backup_file) {
		data->backup = fopen(data->backup_file, 
				     mode == RM_BACK ? 
				     "r" : "w+");
		
		if (!data->backup) {
			aal_fatal("Cannot not open the backup file (%s).", 
				  optarg);
			goto oper_error;
		}
	}
	
	/* Check if device is mounted and we are able to fsck it. */ 
	if ((mounted = misc_dev_mounted(argv[optind])) > 0) {
		if (mounted == MF_RW) {
			aal_fatal("The partition (%s) is mounted with write "
				  "permissions, cannot fsck it.", argv[optind]);
			goto user_error;
		}
		
		aal_set_bit(&data->options, FSCK_OPT_RO);

		if (data->sb_mode != RM_CHECK || data->fs_mode != RM_CHECK) {
			/* If not CHECK mode, lock the process in the memory. */
			if (mlockall(MCL_CURRENT)) {
				aal_fatal("Failed to lock the process in the "
					  "memory: %s.", strerror(errno));
				goto oper_error;
			}
		}
	}
    
	if (!(data->host_device = aal_device_open(ops, argv[optind], 
						  512, O_RDONLY))) 
	{
		aal_fatal("Cannot open the partition (%s): %s.",
			  argv[optind], strerror(errno));
		goto oper_error;
	}

	misc_exception_set_stream(EXCEPTION_TYPE_FSCK, data->logfile);
	return fsck_ask_confirmation(data, argv[optind]);
	
 user_error:
	ret = USER_ERROR;
	goto error;
 oper_error:
	ret = OPER_ERROR;
 error:
	if (data->logfile && data->logfile != stderr)
		fclose(data->logfile);

	if (data->backup)
		fclose(data->backup);
	
	return ret;
}

static void fsck_fini(fsck_parse_t *data) {
	if (data->logfile != NULL && data->logfile != stderr)
		fclose(data->logfile);

	backup_fini();
}

static void fsck_time(char *string) {
	time_t t;

	time(&t);
	fprintf(stderr, "***** %s %s", string, ctime (&t));
}

/* Open the fs and init the tree. */
static errno_t fsck_check_init(repair_data_t *repair, 
			       aal_device_t *host, 
			       FILE *backup,
			       uint8_t sb_mode,
			       uint8_t fs_mode) 
{
	aal_stream_t stream;
	int flags = 0;
	int reop = 0;
	count_t len;
	errno_t res;
	
	/* Reopen device RW for fixing SB. */
	if (sb_mode != RM_CHECK) {
		flags = host->flags;
		if (aal_device_reopen(host, host->blksize, O_RDWR)) {
			aal_fatal("Failed to reopen the device RW.");
			return -EIO;
		}
		
		reop = 1;
	}

	repair->mode = sb_mode;
	if ((res = repair_fs_open(repair, host, host)))
		return res;

	if (repair->fs == NULL) {
		aal_fatal("Cannot open the FileSystem on (%s).", host->name);
		return res;
	}

	if (!reop) {
		/* Reopen device RW for replaying. */
		flags = host->flags;
		if (aal_device_reopen(host, host->blksize, O_RDWR)) {
			aal_fatal("Failed to reopen the device RW.");
			res = -EIO;
			goto error_close_fs;
		}
	}
	
	if ((res = repair_fs_replay(repair->fs)) < 0)
		goto error_close_fs;
	
	repair_error_count(repair, res);
	
	/* Leave device RW if not CHECK mode. */
	if (fs_mode == RM_CHECK) {
		reiser4_journal_sync(repair->fs->journal);
		reiser4_fs_sync(repair->fs);
		if (aal_device_reopen(host, host->blksize, flags)) {
			aal_fatal("Failed to reopen the device RW.");
			res = -EIO;
			goto error_close_fs;
		}
	}
	
	repair->sb_fixable = repair->fixable;
	repair->fixable = 0;
	repair->mode = fs_mode;

	repair_error_count(repair, res);
	
	repair->fs->tree->mpc_func = misc_mpressure_detect;
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	repair_master_print(repair->fs->master, &stream, 
			    misc_uuid_unparse);
	
	aal_stream_format(&stream, "\n");
	
	repair_format_print(repair->fs->format, &stream);
	aal_stream_format(&stream, "\n");
	
	aal_mess("Reiser4 fs was detected on %s.\n%s",
		repair->fs->device->name, (char *)stream.entity);
	
	aal_stream_fini(&stream);
	
	/* Initialize the backup. */
	len = reiser4_format_get_len(repair->fs->format);
	if ((res = backup_init(backup, host, len)))
		goto error_close_fs;

	return 0;
	
 error_close_fs:
	reiser4_fs_close(repair->fs);
	repair->fs = NULL;
	return res;
}

static errno_t fsck_check_fini(repair_data_t *repair) {
	reiser4_status_t *status;
	uint64_t state = 0;
	int flags;

	aal_assert("vpf-1338", repair != NULL);
	aal_assert("vpf-1339", repair->fs != NULL);
	aal_assert("vpf-1340", repair->fs->status != NULL);
	
	if (repair->mode == RM_CHECK)
		return 0;
	
	/* Fix the status block. */
	if (repair->fatal)
		state = FS_DAMAGED;
	else if (repair->fixable)
		state = FS_CORRUPTED;
	
	status = repair->fs->status;
	flags = status->device->flags;
	
	repair_status_state(status, state);
	
	if (aal_device_reopen(status->device, status->device->blksize, O_RDWR))
		return -EIO;
	
	if (reiser4_status_sync(repair->fs->status))
		return -EIO;

	if (aal_device_reopen(status->device, status->device->blksize, flags))
		return -EIO;

	return 0;
}

int main(int argc, char *argv[]) {
	struct aal_device_ops fsck_ops = file_ops;
	aal_device_t *device;
	repair_data_t repair;
	
	fsck_parse_t parse_data;
	errno_t ex = NO_ERROR;
	int stage = 0;
	errno_t res = 0;

	memset(&parse_data, 0, sizeof(parse_data));
	memset(&repair, 0, sizeof(repair));
	
	if ((ex = fsck_init(&parse_data, &fsck_ops, argc, argv)) != NO_ERROR)
		exit(ex);

	device = parse_data.host_device;
	
	if (parse_data.fs_mode == RM_BACK){
		if (aal_device_reopen(device, device->blksize, O_RDWR)) {
			ex = OPER_ERROR;
			goto free_device;
		}

		if ((res = backup_rollback(parse_data.backup, device)))
			ex = OPER_ERROR;

		goto free_device;
	}
	
	fsck_time("fsck.reiser4 started at");
	
	/* SB_mode is specified, otherwise  */
	repair.debug_flag = aal_test_bit(&parse_data.options, FSCK_OPT_DEBUG);
	repair.bitmap_file = parse_data.bitmap_file;
	
	res = fsck_check_init(&repair, device, parse_data.backup, 
			      parse_data.sb_mode, parse_data.fs_mode);
	
	if (res || repair.fatal)
		goto free_libreiser4;
		
	stage = 1;
	
	if ((res = repair_check(&repair)) || (res = fsck_check_fini(&repair)))
		goto free_fs;

	fsck_time("fsck.reiser4 finished at");
    
 free_fs:
	fprintf(stderr, "Closing fs...");
	repair_fs_close(repair.fs);
	repair.fs = NULL;
	fprintf(stderr, "done\n");
    
 free_libreiser4:
	libreiser4_fini();
    
 free_device:
	if (device) {
		if (aal_device_sync(device)) {
			aal_fatal("Cannot synchronize the device (%s).", 
				  device->name);
			ex = OPER_ERROR;
		}
		aal_device_close(device);
	}
	
	fprintf(stderr, "\n");
	
	/* Report about the results. */
	if (res < 0 || ex == OPER_ERROR) {
		ex = OPER_ERROR;
		fprintf(stderr, "Operational error occured while fscking.\n");
		goto free_fsck;
	} 
	
	if (parse_data.fs_mode == RM_BACK)
		goto free_fsck;
	
	if (repair.sb_fixable) {
		/* No fatal corruptions in SB, but some fixable ones. */
		fprintf(stderr, "%llu fixable corruptions were detected in "
			"the SuperBlock. Run with --fix option to fix them.\n",
			repair.sb_fixable);
		
		ex = FIXABLE_ERROR;
	}

	if (repair.fatal) {
		/* Some fatal corruptions in disk format or filesystem. */
		if (parse_data.fs_mode == RM_BUILD && stage) {
			/* Only if no metadata are found. */
			fprintf(stderr, "NO REISER4 METADATA WERE FOUND. "
				"FS RECOVERY IS NOT POSSIBLE.\n");
		} else {
			fprintf(stderr, "%llu fatal corruptions were detected in %s. "
				"Run with %s option to fix them.\n", repair.fatal, 
				stage ? "FileSystem" : "SuperBlock", 
				stage ? "--build-fs" : "--build-sb");
		}
		ex = stage ? FATAL_ERROR : FATAL_SB_ERROR;
	} else if (repair.fixable) {
		/* Some fixable corruptions in filesystem. */
		fprintf(stderr, "%llu fixable corruptions were detected in "
			"the FileSystem. Run with --fix option to fix them.\n",
			repair.fixable);
		
		ex = FIXABLE_ERROR;
	} else if (!repair.sb_fixable) {
		if (parse_data.fs_mode != RM_CHECK && 
		    aal_test_bit(&parse_data.options, FSCK_OPT_RO))
		{
			fprintf(stderr, "\nThe filesystem was mounted RO. It is "
				"better to umount and mount it again.\n");
		}
		
		fprintf(stderr, "FS is consistent.\n");
	}
	
	fprintf(stderr, "\n");

 free_fsck:
	fsck_fini(&parse_data);
	
	return ex;
}
