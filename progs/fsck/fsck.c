/* Copyright (C) 1996-2002 Hans Reiser.
   
   fsck.c -- reiser4 filesystem checking and recovering program. */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <fsck.h>

static void fsck_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, "Modes:\n"
		"  --check                         consistency checking (default).\n"
		"  --fixable                       fixes what can be fixed without\n"
		"                                  rebuild.\n"
		"  --rebuild                       fixes all fs corruptions.\n"
		"Options:\n"
		"  -l, --logfile                   complains into the logfile\n"
		"  -V, --version                   prints the current version.\n"
		"  -?, -h, --help                  prints program usage.\n"
		"  -n, --no-log                    makes fsck to not complain.\n"
		"  -q, --quiet                     suppresses the most of the progress.\n"
		"  -a, -p, --auto, --preen         automatically checks the file system\n"
		"                                  without any questions.\n"
		"  -f, --force                     forces checking even if the file system\n"
		"                                  seems clean.\n"
		"  -v, --verbose                   makes fsck to be verbose.\n"
		"  -r                              ignored.\n");
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
  *************************************************************\n\
\nWill fix the filesystem on (%s).\n"

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
  *************************************************************\n\
\nWill check consistency of the filesystem on (%s).\n"

static errno_t fsck_ask_confirmation(fsck_parse_t *data, char *host_name) {
	if (data->mode == RM_CHECK) {
		fprintf(stderr, CHECK_WARNING, host_name);
	} else if (data->mode == RM_FIX) {
		fprintf(stderr, CHECK_WARNING, host_name);
		fprintf(stderr, "Will fix corruptions which can be "
			"fixed without rebuilding the tree.\n");
	} else if (data->mode == RM_BUILD) {
		fprintf(stderr, REBUILD_WARNING, host_name);	
	} else if (data->mode == RM_BACK) {
		fprintf(stderr, "Will rollback all data saved in (%s) "
			"into (%s).\n", "", host_name);
	}

	fprintf(stderr, "Will use (%s) profile.\n", data->profile->name);

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
	char *str;
	FILE *stream;
	static int flag;
	static int mode = RM_CHECK;

	static struct option long_options[] = {
		/* Fsck modes */
		{"check", no_argument, &mode, RM_CHECK},
		{"fixable", no_argument, &mode, RM_FIX},
		{"rebuild", no_argument, &mode, RM_BUILD},
		/* Fsck hidden modes. */
		{"rollback-fsck-changes", no_argument, &mode, RM_BACK},
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
		{"rollback-data", required_argument, 0, 'R'},
		{0, 0, 0, 0}
	};

	data->profile = &default_profile;
	misc_exception_set_stream(EXCEPTION_FATAL, stderr);
	data->logfile = stderr;

	if (argc < 2) {
		fsck_print_usage(argv[0]);
		return USER_ERROR;
	}

	misc_print_banner(argv[0]);
    
	while ((c = getopt_long(argc, argv, "l:VhnqapfvU:R:r?d", long_options, 
				(int *)0)) != EOF) 
	{
		switch (c) {
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
			aal_set_bit(&data->options, FSCK_OPT_DEBUGGING);
			break;
		}
	}

	fsck_init_streams(data);
    
	if (optind != argc - 1) {
		fsck_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	data->mode = mode;
   
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
						  512, O_RDWR))) 
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
	fprintf(stderr, "\n***** %s %s\n", string, ctime (&t));
}

int main(int argc, char *argv[]) {
	errno_t exit_code = NO_ERROR;
	fsck_parse_t parse_data;
	repair_data_t repair;
	aal_stream_t stream;
	uint16_t mask = 0;
    
	memset(&parse_data, 0, sizeof(parse_data));
	memset(&repair, 0, sizeof(repair));
	aal_stream_init(&stream);

	if ((exit_code = fsck_init(&parse_data, argc, argv)) != NO_ERROR)
		exit(exit_code);
    
	/* Initializing libreiser4 with factory sanity check */
	if (libreiser4_init()) {
		aal_exception_fatal("Cannot initialize the libreiser4.");
		exit_code = OPER_ERROR;
		goto free_device;
	}
    
	repair.mode = parse_data.mode;
	repair.debug_flag = aal_test_bit(&parse_data.options,
					 FSCK_OPT_DEBUGGING);
	repair.progress_handler = gauge_handler;    

	fsck_time("fsck.reiser4 started at");

	fprintf(stderr, "***** Openning the fs.\n");
	if (repair_fs_open(&repair, parse_data.host_device,
			   parse_data.host_device))
	{
		exit_code = OPER_ERROR;	
		goto free_libreiser4;
	}
    
	if (repair.fs == NULL) {
		aal_exception_fatal("Cannot open the filesystem on (%s).", 
				    parse_data.host_device->name);
	
		goto free_libreiser4;
	}
     
	reiser4_master_print(repair.fs->master, &stream);
	aal_stream_format(&stream, "\n");
	reiser4_format_print(repair.fs->format, &stream);
	aal_stream_format(&stream, "\n");
    
	fprintf(stderr, "Reiser4 fs was detected on the %s.\n%s", 
		parse_data.host_device->name, (char *)stream.data);

	if (!(repair.fs->tree = reiser4_tree_init(repair.fs,
						  misc_mpressure_detect)))
	{
		aal_exception_fatal("Cannot open the filesystem on (%s).", 
				    parse_data.host_device->name);
		exit_code = OPER_ERROR;
		goto free_fs;
	}
    
    
	if (repair_check(&repair)) {
		exit_code = OPER_ERROR;
		goto free_tree;
	}
    
	fsck_time("fsck.reiser4 finished at");
    
 free_tree:
	reiser4_tree_fini(repair.fs->tree);
	repair.fs->tree = NULL;

 free_fs:
	fprintf(stderr, "Closing fs ...");
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
    
	/* Report about the results. */
	if (exit_code == 0) {
		if (repair.fatal) {
			fprintf(stderr, "\n%llu fatal corruptions were "
				"detected. Run in --rebuild mode to fix "
				"them.\n", repair.fatal);
			exit_code = FATAL_ERROR;
		} else if (repair.fixable) {
			fprintf(stderr, "\n%llu fixable corruptions were "
				"detected. Run in --fixable mode to fix "
				"them.\n", repair.fixable);
			exit_code = FIXABLE_ERROR;
		}
	}
    
	aal_stream_init(&stream);

	return exit_code;
}

int fsck_children_check(reiser4_node_t *node) {
	aal_list_t *list, *walk;
	int i = 0;
    
	list = node->children;
	aal_list_foreach_forward(list, walk) {
		reiser4_node_t *child = (reiser4_node_t *)walk->data;

		if (child->p.node != node)
			return i;

		i++;
	}

	return -1;
}
