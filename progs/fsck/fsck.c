/* Copyright (C) 1996-2002 Hans Reiser.
   
   fsck.c -- reiser4 filesystem checking and recovering program. */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <fsck.h>

static void fsck_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);

	fprintf(stderr, "Main options:\n"
		"  --check                checks the consistency (default)\n"
		"  --fix                  fixes minor corruptions\n"
		"  --build-sb             rebuilds the super block\n"
		"  --build-fs             rebuilds the filesystem\n"
		"Extra options:\n"
		"  -l, --logfile file     complains into the file\n"
		"  -V, --version          prints the current version\n"
		"  -?, -h, --help         prints program usage\n"
		"  -n, --no-log           makes fsck to not complain\n"
		"  -q, --quiet            suppresses the most of the progress\n"
		"  -a, --auto\n"
		"  -p, --preen            automatically checks the consistency\n"
		"                         without any questions.\n"
		"  -f, --force            forces checking even if the file system\n"
		"                         seems clean\n"
		"  -v, --verbose          makes fsck to be verbose\n"
		"  -r                     ignored\n");
}

#define REBUILD_WARNING \
"  *************************************************************\n\
  **     This is an experimental version of reiser4fsck.     **\n\
  **                 MAKE A BACKUP FIRST!                    **\n\
  ** Do not run rebuild unless something  is broken.  If you **\n\
  ** have bad sectors on a drive it is usually a bad idea to **\n\
  ** continue using it.  Then  you  probably  should  get  a **\n\
  ** working hard drive,  copy the file system from  the bad **\n\
  ** drive  to the good one -- dd_rescue is  a good tool for **\n\
  ** that -- and only then run this program.If you are using **\n\
  ** the latest reiser4progs and  it fails  please email bug **\n\
  ** reports to reiserfs-list@namesys.com, providing as much **\n\
  ** information  as  possible  --  your  hardware,  kernel, **\n\
  ** patches, settings, all reiser4fsck messages  (including **\n\
  ** version), the reiser4fsck  logfile,  check  the  syslog **\n\
  ** file for  any  related information.                     **\n\
  ** If you would like advice on using this program, support **\n\
  ** is available  for $25 at  www.namesys.com/support.html. **\n\
  *************************************************************\n\n"

#define CHECK_WARNING \
"  *************************************************************\n\
  ** If you are using the latest reiser4progs and  it fails **\n\
  ** please  email bug reports to reiserfs-list@namesys.com, **\n\
  ** providing  as  much  information  as  possible --  your **\n\
  ** hardware,  kernel,  patches,  settings,  all  reiserfsk **\n\
  ** messages  (including version), the reiser4fsck logfile, **\n\
  ** check  the  syslog file  for  any  related information. **\n\
  ** If you would like advice on using this program, support **\n\
  ** is available  for $25 at  www.namesys.com/support.html. **\n\
  *************************************************************\n\n"

static errno_t fsck_ask_confirmation(fsck_parse_t *data, char *host_name) {
	if (data->sb_mode == RM_BUILD || data->fs_mode == RM_BUILD)
		fprintf(stderr, REBUILD_WARNING);
	else
		fprintf(stderr, CHECK_WARNING);
	
	fprintf(stderr, "Fscking the %s block device.\n", host_name);
	
	switch (data->sb_mode) {
	case RM_CHECK:
		fprintf(stderr, "Will check the consistency of the Reiser4 "
			"SuperBlock.\n");
		break;
	case RM_FIX:
		fprintf(stderr, "Will fix minor corruptions of the Reiser4 "
			"SuperBblock.\n");
		break;
	case RM_BUILD:
		fprintf(stderr, "Will build the Reiser4 SuperBlock.\n");
		break;
	default:
		break;
	}
	
	switch (data->fs_mode) {
	case RM_CHECK:
		fprintf(stderr, "Will check the consistency of the Reiser4 "
			"FileSystem.\n");
		break;
	case RM_FIX:
		fprintf(stderr, "Will fix minor corruptions of the Reiser4 "
			"FileSystem.\n");
		break;
	case RM_BUILD:
		fprintf(stderr, "Will build the Reiser4 FileSystem.\n");
		break;
	case RM_BACK:
		fprintf(stderr, "Will rollback all data saved in (%s) "
			"into (%s).\n", "", host_name);
		break;
	default:
		break;
	}
	
	fprintf(stderr, "Will use (%s) params.\n", data->param->name);

	if (aal_exception_yesno("Continue?") == EXCEPTION_NO) 
		return USER_ERROR;
     
	return NO_ERROR; 
}

static void fsck_init_streams(fsck_parse_t *data) {
	
	misc_exception_set_stream(EXCEPTION_INFORMATION, 
				  aal_test_bit(&data->options,
					       FSCK_OPT_VERBOSE) ? stderr : NULL);
	
	misc_exception_set_stream(EXCEPTION_ERROR, data->logfile);
	misc_exception_set_stream(EXCEPTION_WARNING, data->logfile);
	misc_exception_set_stream(EXCEPTION_FATAL, stderr);
	misc_exception_set_stream(EXCEPTION_BUG, stderr);
}

static errno_t fsck_init(fsck_parse_t *data, int argc, char *argv[]) 
{
	int c;
	FILE *stream;
	int option_index;
	static int mode = RM_CHECK, sb_mode = 0, fs_mode = 0;

	static struct option options[] = {
		/* SB modes. */
		{"check", no_argument, &mode, RM_CHECK},
		{"fix", no_argument, &mode, RM_FIX},
		{"build-sb", no_argument, &sb_mode, RM_BUILD},
		{"build-fs", no_argument, &fs_mode, RM_BUILD},
		/* Fsck hidden modes. */
		{"rollback", no_argument, &mode, RM_BACK},
		/* Fsck options */
		{"logfile", required_argument, 0, 'l'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"no-log", no_argument, NULL, 'n'},
		{"auto", no_argument, NULL, 'a'},
		{"preen", no_argument, NULL, 'p'},
		{"force", no_argument, NULL, 'f'},
		{"verbose", no_argument, NULL, 'v'},
		/* Fsck hidden options. */
		{"passes-dump", required_argument, 0, 'U'},
		{"backup", required_argument, 0, 'R'},
		{0, 0, 0, 0}
	};

	data->param = &default_param;
	misc_exception_set_stream(EXCEPTION_FATAL, stderr);
	data->logfile = stderr;

	if (argc < 2) {
		fsck_print_usage(argv[0]);
		return USER_ERROR;
	}

	while ((c = getopt_long(argc, argv, "l:VhnqapfvU:R:r?d", 
				options, &option_index)) >= 0) 
	{
		switch (c) {
		case 0:
			/* Long options. */
			break;
		case 'l':
			if ((stream = fopen(optarg, "w")) == NULL)
				aal_exception_fatal("Cannot not open the "
						    "logfile (%s).", optarg);
			else 
				data->logfile = stream;		
			break;
		case 'n':
			data->logfile = NULL;
			break;
		case 'U':
			break;
		case 'R':
			break;
		case 'f':
			aal_set_bit(&data->options, FSCK_OPT_FORCE);
			break;
		case 'a':
		case 'p':
			aal_set_bit(&data->options, FSCK_OPT_AUTO);
			break;
		case 'v':
			aal_set_bit(&data->options, FSCK_OPT_VERBOSE);
			break;
		case 'h': 
		case '?':
			fsck_print_usage(argv[0]);
			return USER_ERROR;	    
		case 'V': 
			misc_print_banner(argv[0]);
			return USER_ERROR;
		case 'q':
			aal_gauge_set_handler(GAUGE_PERCENTAGE, NULL);
			aal_gauge_set_handler(GAUGE_INDICATOR, NULL);
			aal_gauge_set_handler(GAUGE_SILENT, NULL);
			break;
		case 'r':
			break;
		case 'd':
			aal_set_bit(&data->options, FSCK_OPT_DEBUG);
			break;
		}
	}

	fsck_init_streams(data);
    
	if (optind != argc - 1) {
		fsck_print_usage(argv[0]);
		return USER_ERROR;
	}
	
	data->sb_mode = sb_mode ? sb_mode : mode;
	data->fs_mode = fs_mode ? fs_mode : mode;
   
	/* Check if device is mounted and we are able to fsck it. */ 
	if (misc_dev_mounted(argv[optind], NULL)) {
		if (!misc_dev_mounted(argv[optind], "ro")) {
			aal_exception_fatal("The partition (%s) is mounted "
					    "w/ write permissions, cannot "
					    "fsck it.", argv[optind]);
			return USER_ERROR;
		} else {
			aal_set_bit(&data->options, FSCK_OPT_READ_ONLY);
		}
	}
    
	if (!(data->host_device = aal_device_open(&file_ops, argv[optind], 
						  512, O_RDONLY))) 
	{
		aal_exception_fatal("Cannot open the partition (%s): %s.",
				    argv[optind], strerror(errno));
		return OPER_ERROR;
	}

	aal_gauge_set_handler(GAUGE_PERCENTAGE, gauge_rate);
	aal_gauge_set_handler(GAUGE_TREE, gauge_tree);
    
	return fsck_ask_confirmation(data, argv[optind]);
}

static void fsck_time(char *string) {
	time_t t;

	time(&t);
	fprintf(stderr, "\n***** %s %s", string, ctime (&t));
}

/* Open the fs and init the tree. */
static errno_t fsck_prepare(repair_data_t *repair, aal_device_t *host) {
	aal_stream_t stream;
	errno_t res;
	
	fprintf(stderr, "***** Openning the fs.\n");
	
	if ((res = repair_fs_open(repair, host, host)))
		return res;

	if (repair->fs == NULL) {
		aal_exception_fatal("Cannot open the FileSystem on (%s).", 
				    host->name);
		
		return res;
	}

	aal_stream_init(&stream);
	reiser4_master_print(repair->fs->master, &stream);
	aal_stream_format(&stream, "\n");
	reiser4_format_print(repair->fs->format, &stream);
	aal_stream_format(&stream, "\n");
	
	fprintf(stderr, "Reiser4 fs was detected on the %s.\n%s",
		repair->fs->device->name, (char *)stream.data);
	
	aal_stream_fini(&stream);
	
	/* Init the tree on the openned fs. */
	if (!(repair->fs->tree = reiser4_tree_init(repair->fs, 
						   misc_mpressure_detect)))
	{
		aal_exception_fatal("Failed to initialize the tree on the "
				    "openned fs.");
		goto error_free_fs;
	}
	
	if (repair->mode != RM_CHECK) {
		aal_device_t *device = repair->fs->device;
		
		if (aal_device_reopen(device, device->blksize, O_RDWR))
			return -EIO;
	}
	
	return 0;
	
 error_free_fs:
	repair_fs_close(repair->fs);
	repair->fs = NULL;
	return -EINVAL;
}

static errno_t fsck_fini(repair_data_t *repair) {
	uint64_t state = 0;
	
	aal_assert("vpf-1338", repair != NULL);
	aal_assert("vpf-1339", repair->fs != NULL);
	aal_assert("vpf-1340", repair->fs->status != NULL);
	
	if (repair->fatal)
		state = FS_DAMAGED;
	else if (repair->fixable)
		state = FS_CORRUPTED;
	
	return repair_status_state(repair->fs->status, state);
}

int main(int argc, char *argv[]) {
	errno_t exit_code = NO_ERROR;
	uint64_t df_fixable = 0;
	fsck_parse_t parse_data;
	repair_data_t repair;
	int stage = 0;
	errno_t res;
 
	memset(&parse_data, 0, sizeof(parse_data));
	memset(&repair, 0, sizeof(repair));

	if ((exit_code = fsck_init(&parse_data, argc, argv)) != NO_ERROR)
		exit(exit_code);
    
	/* Initializing libreiser4 with factory sanity check */
	if ((res = libreiser4_init())) {
		aal_exception_fatal("Cannot initialize the libreiser4.");
		goto free_device;
	}
	
	/* SB_mode is specified, otherwise  */
	repair.mode = parse_data.sb_mode;
	repair.debug_flag = aal_test_bit(&parse_data.options, FSCK_OPT_DEBUG);
	repair.progress_handler = gauge_handler;    
	
	if ((res = fsck_prepare(&repair, parse_data.host_device)) || 
	    repair.fatal)
	{
		goto free_libreiser4;
	}
	
	df_fixable = repair.fixable;
	repair.fixable = 0;
	
	repair.mode = parse_data.fs_mode;
	stage = 1;
	
	if ((res = repair_check(&repair)) || (res = fsck_fini(&repair)))
		goto free_fs;

	fsck_time("fsck.reiser4 finished at");
    
 free_fs:
	fprintf(stderr, "Closing fs...");
	reiser4_tree_close(repair.fs->tree);
	repair.fs->tree = NULL;
	repair_fs_close(repair.fs);
	repair.fs = NULL;
	fprintf(stderr, "done\n");
    
 free_libreiser4:
	libreiser4_fini();
    
 free_device:
	if (parse_data.host_device) {
		if (aal_device_sync(parse_data.host_device)) {
			aal_exception_fatal("Cannot synchronize the device (%s).", 
					    parse_data.host_device->name);
			exit_code = OPER_ERROR;
		}
		aal_device_close(parse_data.host_device);
	}
	
	fprintf(stderr, "\n");
	
	/* Report about the results. */
	if (res < 0) {
		aal_exception_mess("Operational error occured while fscking.");
		return OPER_ERROR;
	} 
	
	if (df_fixable) {
		/* No fatal corruptions in SB, but some fixable ones. */
		fprintf(stderr, "%llu fixable corruptions were detected in "
			"the SuperBlock. Run with --fix option to fix them.\n",
			df_fixable);
		
		exit_code = FIXABLE_ERROR;
	}

	if (repair.fatal) {
		/* Some fatal corruptions in disk format or filesystem. */
		fprintf(stderr, "%llu fatal corruptions were detected in %s. "
			"Run with %s option to fix them.\n", repair.fatal, 
			stage ? "FileSystem" : "SuperBlock", 
			stage ? "--build-fs" : "--build-sb");
		
		exit_code = stage ? FATAL_ERROR : FATAL_SB_ERROR;
	} else if (repair.fixable) {
		/* Some fixable corruptions in filesystem. */
		fprintf(stderr, "%llu fixable corruptions were detected in "
			"the FileSystem. Run with --fix option to fix them.\n",
			repair.fixable);
		
		exit_code = FIXABLE_ERROR;
	} else if (!df_fixable)
		fprintf(stderr, "No corruption found.\n\n");
	
	return exit_code;
}

