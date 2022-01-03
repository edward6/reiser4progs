/*
  Copyright (c) 2017-2022 Eduard O. Shishkin

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* Program for managing on-line Reiser4 logical volumes */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif
#include <misc/misc.h>
#include <reiser4/ioctl.h>
#include <reiser4/libreiser4.h>

#define INVAL_INDEX   0xffffffffffffffff

/* Known behavior flags */
typedef enum behav_flags {
	BF_FORCE        = 1 << 0,
	BF_YES          = 1 << 1,
	BF_WITH_BALANCE = 1 << 2
} behav_flags_t;

/* Prints options */
static void volmgr_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [options] [FILE] [MNT]\n", name);
	fprintf(stderr,
		"Common options:\n"
		"  -?, -h, --help                Print program usage.\n"
		"  -V, --version                 Print current version.\n"
		"  -y, --yes                     Assumes an answer 'yes' to all questions.\n"
		"Off-line options:\n"
	        "  -g, --register DEV            Register a brick associated with device DEV\n"
		"                                in the system.\n"
	        "  -u, --unregister DEV          Unregister a brick associated with device DEV\n"
		"                                in the system.\n"
		"  -l, --list                    Print list of all bricks registered in the\n"
		"                                system.\n"
		"On-line options:\n"
		"  -p, --print N                 Print information about a brick of serial\n"
		"                                number N in the volume mounted at MNT.\n"
		"  -P, --print-all               Print information about all bricks of the\n"
		"                                volume mounted at MNT.\n"
		"  -b, --balance                 Balance volume mounted at MNT.\n"
		"  -B, --with-balance            Complete a volume operation with balancing.\n"
	        "  -z, --resize DEV              Change data capacity of a brick accociated\n"
		"                                with device DEV in the volume mounted at MNT.\n"
		"                                The actual capacity has to be defined by the\n"
		"                                option \"-c (--capacity)\".\n"
	        "  -c, --capacity VALUE          Define new data capacity VALUE for a device\n"
		"                                specified by option \"-z (--resize).\n"
		"  -a, --add DEV                 Add a brick associated with device DEV to the\n"
	        "                                volume mounted at MNT.\n"
		"  -x, --add-proxy DEV           Add a proxy brick associated with device\n"
		"                                DEV to the volume mounted at MNT.\n"
	        "  -r, --remove DEV              Remove a brick associated with device DEV\n"
		"                                from the volume mounted at MNT.\n"
	        "  -R, --finish-removal          Complete a brick removal operation for the\n"
		"                                volume mounted at MNT.\n"
	        "  -q, --scale N                 increase 2^N times the upper limit for total\n"
		"                                number of bricks in the volume mounted at MNT.\n"
	        "  -m, --migrate N               Migrate all data blocks of regular FILE to a\n"
		"                                brick of serial number N.\n"
	        "  -i, --set-immobile            Set \"immobile\" property to regular FILE.\n"
		"  -e, --clear-immobile          Clear \"immobile\" property of regular FILE.\n"
		"  -S, --restore-regular         Restore regular distribution on the volume\n"
		"                                mounted at MNT.\n");
}

/* Initializes exception streams used by volume manager */
static void volmgr_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
}

static void print_vol_op_error(struct reiser4_vol_op_args *info)
{
	switch (info->error) {
	case E_NO_BRICK:
		aal_mess("There is no brick with index %llu in the volume",
			 (unsigned long long)info->s.val);
		break;
	case E_NO_VOLUME:
		aal_mess("Volume not found");
		break;
	case E_RESIZE_PROXY:
		aal_mess("Resize is undefined for proxy brick");
		break;
	case E_RESIZE_SIMPLE:
		aal_mess("Resize is undefined for simple volumes");
		break;
	case E_RESIZE_TO_ZERO:
		aal_mess("Resize to zero capacity is undefined. Consider brick removal");
		break;
	case E_ADD_INVAL_CAPA:
		aal_mess("Can't add brick of unacceptable capacity");
		break;
	case E_ADD_NOT_EMPTY:
		aal_mess("Can't add not empty brick");
		break;
	case E_ADD_SIMPLE:
		aal_mess("Can't add brick to a simple volume");
		break;
	case E_ADD_INAPP_VOL:
		aal_mess("Brick %s doesn't match the volume", info->d.name);
		break;
	case E_ADD_SECOND_PROXY:
		aal_mess("Can't add second proxy brick to the same volume");
		break;
	case E_ADD_SNGL_PROXY:
		aal_mess("The single brick can not be proxy");
		break;
	case E_BRICK_EXIST:
		aal_mess("Can't add brick to DSA twice");
		break;
	case E_BRICK_NOT_IN_VOL:
		aal_mess("Brick %s doesn't belong to the volume",
			 info->d.name);
		break;
	case E_REMOVE_SIMPLE:
		aal_mess("Brick removal is undefined for simple volumes");
		break;
	case E_REMOVE_UNDEF:
		aal_mess("Can't remove any brick from this volume");
		break;
	case E_REMOVE_NOSPACE:
		aal_mess("Not enough space in the volume for brick removal");
		break;
	case E_REMOVE_MTD:
		aal_mess("Brick is not in DSA. Can't remove");
		break;
	case E_REMOVE_TAIL_SIMPLE:
		aal_mess("Removal completion is undefined for simple volumes");
		break;
	case E_REMOVE_TAIL_NOT_EMPTY:
		aal_mess("Can't complete removal: brick %s is not empty",
			 info->d.name);
		break;
	case E_BALANCE:
		aal_mess("Balancing aborted");
		break;
	case E_INCOMPL_REMOVAL:
		aal_mess("Can't perform the operation. First, complete removal on the volume");
		break;
	case E_VOLUME_BUSY:
		aal_mess("Volume is busy by another operation");
		break;
	case E_UNSUPP_OP:
		aal_mess("Operation unsupported");
		break;
	case E_REG_NO_MASTER:
		aal_mess("There is no reiser4 on %s", info->d.name);
		break;
	case E_UNREG_ACTIVE:
		aal_mess("Can't unregister brick of mounted volume");
		break;
	case E_UNREG_NO_BRICK:
		aal_mess("Can't find registered brick %s", info->d.name);
		break;
	case E_SCAN_UNSUPP:
		aal_mess("Unsupported plugin found on %s", info->d.name);
		break;
	case E_SCAN_UNMATCH:
		aal_mess("Inconsistent mirror parameters found on %s",
			 info->d.name);
		break;
	case E_SCAN_BAD_STRIPE:
		aal_mess("Bad stripe size found on %s", info->d.name);
		break;
	default:
		break;
	}
}

static int set_op(struct reiser4_vol_op_args *info,
		  reiser4_vol_op op)
{
	if (info->opcode != REISER4_INVALID_OPT) {
		aal_error("Incompatible options were specified");
		return USER_ERROR;
	}
	info->opcode = op;
	return NO_ERROR;
}

static int set_op_name(struct reiser4_vol_op_args *info,
		       char *name, struct stat *st, reiser4_vol_op op)
{
	if (stat(name, st) == -1) {
		aal_error("Can't stat %s. %s.", name, strerror(errno));
		return USER_ERROR;
	}
	strncpy(info->d.name, name, sizeof(info->d.name) - 1);
	return set_op(info, op);
}

static int set_op_value(struct reiser4_vol_op_args *info,
			char *value, reiser4_vol_op op)
{
	if ((info->s.val = misc_str2long(value, 10)) == INVAL_DIG) {
		aal_error("Invalid value %s.", value);
		return USER_ERROR;
	}
	return set_op(info, op);
}

static int set_capacity(struct reiser4_vol_op_args *info, char *value)
{
	if ((info->new_capacity = misc_str2long(value, 10)) == INVAL_DIG) {
		aal_error("Invalid value %s.", value);
		return USER_ERROR;
	}
	if (info->new_capacity == 0) {
		aal_error("Invalid capacity (0)");
		return USER_ERROR;
	}
	return NO_ERROR;
}

static int check_deps(struct reiser4_vol_op_args *info)
{
	switch (info->opcode) {
	case REISER4_RESIZE_BRICK:
		if (info->new_capacity == 0) {
			aal_error("Option \"-z (--resize)\" requires option \"-c (--capacity)\"");
			return -1;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void print_separator(void)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/**
 * Print early information about a brick, which is possibly not yet activated
 */
static void print_volume_header(struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);

	aal_stream_format(&stream, "%s", "Volume ");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (!uuid_is_null((unsigned char *)info->u.vol.id)) {
		char uuid[37];
		uuid[36] = '\0';
		uuid_unparse(info->u.vol.id, uuid);
		aal_stream_format(&stream, "ID: %s ", uuid);
	} else
		aal_stream_format(&stream, "ID: <none> ");
#endif
	aal_stream_format(&stream, "%s\n",
			  aal_test_bit(&info->u.vol.fs_flags,
				       REISER4_ACTIVATED_VOL) ?
			  "(Active)" : "(Inactive)");
	aal_stream_fini(&stream);
}

/**
 * Print "early" information about volume, which is possibly not yet activated
 */
static int print_brick_header(int fd, struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	aal_stream_format(&stream, "%s", "Brick ");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (!uuid_is_null((unsigned char *)info->u.brick.ext_id)) {
		char uuid[37];
		uuid[36] = '\0';
		uuid_unparse(info->u.brick.ext_id, uuid);
		aal_stream_format(&stream, "ID: %s, ", uuid);
	} else
		aal_stream_format(&stream, "ID: <none>, ");
	aal_stream_format(&stream, "Device name: %s\n", info->d.name);
#endif
	aal_stream_fini(&stream);
	return 0;
}

/**
 * Call ioclt(2) followed by optional header, body and footer
 */
static int ioctl_seq(int fd, unsigned long ioctl_req,
		     struct reiser4_vol_op_args *info,
		     reiser4_vol_op_error *errp,
		     void (*header)(struct reiser4_vol_op_args *info),
		     int (*body)(int, struct reiser4_vol_op_args *info),
		     void (*footer)(void))
{
	int ret = 0;

	info->error = 0;
	ret = ioctl(fd, ioctl_req, info);
	if (ret == -1) {
		aal_error("Ioctl failed (%s)", strerror(errno));
		return ret;
	}
	if (errp)
		*errp = info->error;
	if (info->error)
		return 0;

	if (header)
		header(info);
	if (body)
		ret = body(fd, info);
	if (footer)
		footer();
	return ret;
}

static int ioctl_iter(int fd, unsigned long ioctl_req, reiser4_vol_op op,
		      struct reiser4_vol_op_args *info, uint64_t *index,
		      void (*header)(struct reiser4_vol_op_args *info),
		      int (*body)(int, struct reiser4_vol_op_args *),
		      void (*footer)(void))
{
	reiser4_vol_op_error error = 0;
	int i = 0;
	int ret;

	while (1) {
		*index = i++;
		info->opcode = op;
		ret = ioctl_seq(fd, ioctl_req, info, &error,
				header, body, footer);
		if (ret || error)
			break;
	}
	return ret;
}

/**
 * Print all registered bricks of a volume.
 * Pre-condition: @info contains uuid of the volume
 */
static int list_bricks_of_volume(int fd, struct reiser4_vol_op_args *info)
{
	return ioctl_iter(fd, REISER4_IOC_SCAN_DEV,
			  REISER4_BRICK_HEADER, info,
			  &info->s.brick_idx, NULL,
			  print_brick_header, NULL);
}

/**
 * Print all bricks registered in the system
 */
static int list_all_bricks(int fd, struct reiser4_vol_op_args *info)
{
	return ioctl_iter(fd, REISER4_IOC_SCAN_DEV,
			  REISER4_VOLUME_HEADER, info,
			  &info->s.vol_idx, print_volume_header,
			  list_bricks_of_volume, print_separator);
}

/**
 * Print information about active (mounted) volume
 */
static int print_volume(int fd, struct reiser4_vol_op_args *info)
{
	rid_t vol, dst;
	aal_stream_t stream;
	reiser4_plug_t *vol_plug, *dst_plug;
	uint64_t stripe_size;
	uint64_t nr_segments;

	aal_stream_init(&stream, stdout, &file_stream);

	vol = info->u.vol.vpid;
	dst = info->u.vol.dpid;

	stripe_size = 0;
	if (info->u.vol.stripe_bits != 0)
		stripe_size =  1ull << info->u.vol.stripe_bits;

	nr_segments = 0;
	if (info->u.vol.nr_sgs_bits != 0)
		nr_segments = 1ull << info->u.vol.nr_sgs_bits;

	if (!(dst_plug = reiser4_factory_ifind(DST_PLUG_TYPE, dst))) {
		aal_error("Can't find distrib plugin by its id 0x%x.", dst);
		return -1;
	}
	if (!(vol_plug = reiser4_factory_ifind(VOL_PLUG_TYPE, vol))) {
		aal_error("Can't find volume plugin by its id 0x%x.", vol);
		return -1;
	}
	aal_stream_format(&stream, "%s\n", "Logical Volume Info:");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (!uuid_is_null((unsigned char *)info->u.vol.id)) {
		char uuid[37];
		uuid[36] = '\0';
		uuid_unparse(info->u.vol.id, uuid);
		aal_stream_format(&stream, "ID:\t\t%s\n", uuid);
	} else
		aal_stream_format(&stream, "ID:\t\t<none>\n");
#endif
	aal_stream_format(&stream, "volume:\t\t0x%x (%s)\n",
			  vol, vol_plug ? vol_plug->label : "absent");

	aal_stream_format(&stream, "distribution:\t0x%x (%s)\n",
			  dst, dst_plug ? dst_plug->label : "absent");

	aal_stream_format(&stream, "stripe:\t\t%llu %s\n",
			  stripe_size, stripe_size != 0 ? "" : "(infinite)");

	aal_stream_format(&stream, "segments:\t%llu\n", nr_segments);

	aal_stream_format(&stream, "bricks total:\t%d\n", info->u.vol.nr_bricks);

	aal_stream_format(&stream, "bricks in DSA:\t%d\n", info->u.vol.bricks_in_dsa);

	aal_stream_format(&stream, "slots:\t\t%u\n", info->u.vol.nr_mslots);

	aal_stream_format(&stream, "map blocks:\t%llu\n",
			  info->u.vol.nr_volinfo_blocks);

	aal_stream_format(&stream, "balanced:\t%s\n",
			  aal_test_bit(&info->u.vol.fs_flags,
				       REISER4_UNBALANCED_VOL) ? "No" : "Yes");
	aal_stream_format(&stream, "health:\t\t%s\n",
			  aal_test_bit(&info->u.vol.fs_flags,
			     REISER4_INCOMPLETE_BRICK_REMOVAL) ?
			  "Incomplete brick removal" : "OK");
	aal_stream_fini(&stream);
	return 0;
}

/**
 * Print information about active brick (i.e. brick of a mounted volume)
 */
static int print_brick(int fd, struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;
	int is_meta = (info->u.brick.int_id == 0);

	aal_stream_init(&stream, stdout, &file_stream);

	aal_stream_format(&stream, "%s\n", "Brick Info:");

	aal_stream_format(&stream, "internal ID:\t%u (%s)\n",
			  info->u.brick.int_id,
			  is_meta ? "meta-data brick" : "data brick");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (!uuid_is_null((unsigned char *)info->u.brick.ext_id)) {
		char uuid[37];
		uuid[36] = '\0';
		uuid_unparse(info->u.brick.ext_id, uuid);
		aal_stream_format(&stream, "external ID:\t%s\n", uuid);
	} else
		aal_stream_format(&stream, "external ID:\t<none>\n");
#endif
	aal_stream_format(&stream, "device name:\t%s\n", info->d.name);

	aal_stream_format(&stream, "num replicas:\t%u\n",
			  info->u.brick.nr_replicas);

	aal_stream_format(&stream, "block count:\t%llu\n",
			  info->u.brick.block_count);

	aal_stream_format(&stream, "blocks used:\t%llu\n",
			  info->u.brick.blocks_used);

	aal_stream_format(&stream, "system blocks:\t%llu\n",
			  info->u.brick.system_blocks);

	aal_stream_format(&stream, "data capacity:\t%llu\n",
			  info->u.brick.data_capacity);

	aal_stream_format(&stream, "space usage:\t%.3f\n",
			  ((double)info->u.brick.blocks_used -
			   info->u.brick.system_blocks) /
			  ((double)info->u.brick.block_count -
			   info->u.brick.system_blocks));

	aal_stream_format(&stream, "volinfo addr:\t%llu %s\n",
			  info->u.brick.volinfo_addr,
			  info->u.brick.volinfo_addr ? "" : "(none)");

	aal_stream_format(&stream, "in DSA:\t\t%s\n",
			  info->u.brick.subv_flags & (1 << SUBVOL_HAS_DATA_ROOM) ?
			  "Yes" : "No");

	aal_stream_format(&stream, "is proxy:\t%s\n",
			  info->u.brick.subv_flags & (1 << SUBVOL_IS_PROXY) ?
			  "Yes" : "No");

	aal_stream_fini(&stream);
	return 0;
}

int main(int argc, char *argv[]) {
	struct reiser4_vol_op_args info;
	uint32_t flags = 0;
	int offline = 0;
	struct stat st;
	char *name;
	int ret;
	int fd;
	int c;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"yes", no_argument, NULL, 'y'},
		{"register", required_argument, NULL, 'g'},
		{"unregister", required_argument, NULL, 'u'},
		{"list", no_argument, NULL, 'l'},
		{"print", required_argument, NULL, 'p'},
		{"print-all", no_argument, NULL, 'P'},
		{"balance", no_argument, NULL, 'b'},
		{"with-balance", no_argument, NULL, 'B'},
		{"add", required_argument, NULL, 'a'},
		{"add-proxy", required_argument, NULL, 'x'},
		{"remove", required_argument, NULL, 'r'},
		{"finish-removal", no_argument, NULL, 'R'},
		{"resize", required_argument, NULL, 'z'},
		{"capacity", required_argument, NULL, 'c'},
		{"scale", required_argument, NULL, 'q'},
		{"migrate-file", required_argument, NULL, 'm'},
		{"set-immobile", no_argument, NULL, 'i'},
		{"clear-immobile", no_argument, NULL, 'e'},
		{"restore-regular", no_argument, NULL, 'S'},
		{0, 0, 0, 0}
	};

	volmgr_init();
	memset(&info, 0, sizeof(info));

	if (argc < 2) {
		volmgr_print_usage(argv[0]);
		return USER_ERROR;
	}
	while ((c = getopt_long(argc, argv, "hVRSByfbliePp:g:u:a:x:r:z:c:q:m:?",
				long_options, (int *)0)) != EOF)
	{
		switch (c) {
		case 'h':
		case '?':
			volmgr_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner_noname(argv[0]);
			return NO_ERROR;
		case 'f':
			flags |= BF_FORCE;
			break;
		case 'y':
			flags |= BF_YES;
			break;
		case 'B':
			flags |= BF_WITH_BALANCE;
			break;
		case 'b':
			ret = set_op(&info, REISER4_BALANCE_VOLUME);
			if (ret)
				return ret;
			break;
		case 'g':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_REGISTER_BRICK);
			if (ret)
				return ret;
			offline = 1;
			break;
		case 'u':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_UNREGISTER_BRICK);
			if (ret)
				return ret;
			offline = 1;
			break;
		case 'l':
			ret = set_op(&info, REISER4_LIST_BRICKS);
			if (ret)
				return ret;
			offline = 1;
			break;
		case 'a':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_ADD_BRICK);
			if (ret)
				return ret;
			break;
		case 'x':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_ADD_PROXY);
			if (ret)
				return ret;
			break;
		case 'r':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_REMOVE_BRICK);
			if (ret)
				return ret;
			break;
		case 'R':
			ret = set_op(&info, REISER4_FINISH_REMOVAL);
			if (ret)
				return ret;
			break;
		case 'p':
			ret = set_op_value(&info, optarg,
					   REISER4_PRINT_BRICK);
			if (ret)
				return ret;
			break;
		case 'P':
			ret = set_op(&info, REISER4_PRINT_BRICK);
			if (ret)
				return ret;
			info.s.val = INVAL_INDEX;
			break;
		case 'z':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_RESIZE_BRICK);
			if (ret)
				return ret;
			break;
		case 'c':
			ret = set_capacity(&info, optarg);
			if (ret)
				return ret;
			break;
		case 'q':
			ret = set_op_value(&info, optarg,
					   REISER4_SCALE_VOLUME);
			if (ret)
				return ret;
			break;
		case 'm':
			ret = set_op_value(&info, optarg,
					   REISER4_MIGRATE_FILE);
			if (ret)
				return ret;
			break;
		case 'i':
			ret = set_op(&info, REISER4_SET_FILE_IMMOBILE);
			if (ret)
				return ret;
			break;
		case 'e':
			ret = set_op(&info, REISER4_CLR_FILE_IMMOBILE);
			if (ret)
				return ret;
			break;
		case 'S':
			ret = set_op(&info, REISER4_RESTORE_REGULAR_DST);
			if (ret)
				return ret;
			break;
		}
	}
	if (info.opcode == REISER4_INVALID_OPT)
		/*
		 * no operations were specified,
		 * print common volume info
		 */
		info.opcode = REISER4_PRINT_VOLUME;

	if (!(flags & BF_YES))
		misc_print_banner_noname(argv[0]);

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		goto error;
	}
	if (offline) {
		fd = open("/dev/reiser4-control", O_NONBLOCK);
		if (fd == -1) {
			aal_error("Can't open %s. %s.", name, strerror(errno));
			goto error_free_libreiser4;
		}
		switch(info.opcode) {
		case REISER4_LIST_BRICKS:
			ret = list_all_bricks(fd, &info);
			break;
		default:
			ret = ioctl_seq(fd, REISER4_IOC_SCAN_DEV,
					&info, NULL, NULL, NULL, NULL);
			print_vol_op_error(&info);
		}
	} else {
		ret = check_deps(&info);
		if (ret)
			goto error_free_libreiser4;
		name = argv[optind];
		if (name == NULL) {
			libreiser4_fini();
			volmgr_print_usage(argv[0]);
			return USER_ERROR;
		}
		fd = open(name, O_NONBLOCK);
		if (fd == -1) {
			aal_error("Can't open %s. %s.", name, strerror(errno));
			goto error_free_libreiser4;
		}
		if (flags & BF_WITH_BALANCE)
			info.flags |= COMPLETE_WITH_BALANCE;

		switch(info.opcode) {
		case REISER4_PRINT_VOLUME:
			ret = ioctl_seq(fd, REISER4_IOC_VOLUME, &info, NULL,
					NULL, print_volume, print_separator);
			print_vol_op_error(&info);
			break;
		case REISER4_PRINT_BRICK:
			if (info.s.val != INVAL_INDEX) {
				/* print one brick */
				ret = ioctl_seq(fd, REISER4_IOC_VOLUME,
						&info, NULL, NULL,
						print_brick, print_separator);
				print_vol_op_error(&info);
			} else {
				/* print all bricks */
				ret = ioctl_iter(fd, REISER4_IOC_VOLUME,
						 REISER4_PRINT_BRICK, &info,
						 &info.s.val, NULL, print_brick,
						 print_separator);
			}
			break;
		default:
			ret = ioctl_seq(fd, REISER4_IOC_VOLUME, &info,
					NULL, NULL, NULL, NULL);
			print_vol_op_error(&info);
			break;
		}
	}
 close:
	if (close(fd) == -1)
		aal_error("Failed to close %s (%s)", name, strerror(errno));
	if (ret)
		goto error_free_libreiser4;
	/*
	 * Deinitializing libreiser4.
	 */
	libreiser4_fini();
	return NO_ERROR;

 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}

/*
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */
