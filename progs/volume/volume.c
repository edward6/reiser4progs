/*
  Copyright (c) 2017-2019 Eduard O. Shishkin

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

/* Known behavior flags */
typedef enum behav_flags {
	BF_FORCE      = 1 << 0,
	BF_YES        = 1 << 1,
} behav_flags_t;

/* Prints options */
static void volmgr_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
	fprintf(stderr,
		"Volume managing options:\n"
	        "  -g, --register FILE           register a brick associated with a device\n"
		"                                \"FILE\" in the system.\n"
	        "  -u, --unregister FILE         unregister a brick associated with\n"
		"                                device\"FILE\" in the system.\n"
		"  -l, --list                    print list of all bricks registered in the\n"
		"                                system.\n"
		"  -p, --print N                 print information about a brick of serial\n"
		"                                number N in the mounted volume.\n"
		"  -b, --balance                 balance the volume.\n"
	        "  -z, --resize FILE             set new data capacity for a brick associated\n"
		"                                with device \"FILE\".\n"
	        "  -c, --capacity VALUE          specify VALUE of new data capacity\n"
		"  -a, --add FILE                add a brick associated with device \"FILE\"\n"
	        "                                to the volume.\n"
	        "  -r, --remove FILE             remove a brick associated with device\n"
		"                                \"FILE\" from the volume.\n"
	        "  -q, --scale N                 increase \"2^N\" times number of hash space\n"
		"                                segments.\n"
		"Common options:\n"
		"  -?, -h, --help                print program usage.\n"
		"  -V, --version                 print current version.\n"
		"  -y, --yes                     assumes an answer 'yes' to all questions.\n");
}

/* Initializes exception streams used by volume manager */
static void volmgr_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
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
	if (set_op(info, op))
		return USER_ERROR;
	strncpy(info->d.name, name, sizeof(info->d.name));
	return NO_ERROR;
}

static int set_op_value(struct reiser4_vol_op_args *info,
			char *value, reiser4_vol_op op)
{
	if ((info->s.val = misc_str2long(value, 10)) == INVAL_DIG)
		return USER_ERROR;
	if (set_op(info, op))
		return USER_ERROR;
	return NO_ERROR;
}

static int set_capacity(struct reiser4_vol_op_args *info, char *value)
{
	if ((info->new_capacity = misc_str2long(value, 10)) == INVAL_DIG)
		return USER_ERROR;
	return NO_ERROR;
}

static void print_separator(void)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/**
 * Print information about registered brick,
 * which is possibly not activated
 */
static void print_volume_header(struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);

	aal_stream_format(&stream, "%s", "Volume ");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*info->u.vol.id != '\0') {
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
 * Print information about volume, which is possibly not activated
 */
static void print_brick_header(struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	aal_stream_format(&stream, "%s", "Brick ");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*info->u.brick.ext_id != '\0') {
		char uuid[37];
		uuid[36] = '\0';
		uuid_unparse(info->u.brick.ext_id, uuid);
		aal_stream_format(&stream, "ID: %s, ", uuid);
	} else
		aal_stream_format(&stream, "ID: <none>, ");
	aal_stream_format(&stream, "Device name: %s\n", info->d.name);
#endif
	aal_stream_fini(&stream);
}

/**
 * Print all registered bricks of a volume.
 * Pre-condition: @info contains uuid of the volume
 */
static int list_bricks_of_volume(int fd, struct reiser4_vol_op_args *info)
{
	int i;
	int ret;

	for (i = 0;; i++) {
		info->error = 0;
		info->opcode = REISER4_BRICK_HEADER;
		info->s.brick_idx = i;

		ret = ioctl(fd, REISER4_IOC_SCAN_DEV, info);
		if (ret || info->error)
			break;
		print_brick_header(info);
	}
	return ret;
}

static int list_all_bricks(int fd, struct reiser4_vol_op_args *info)
{
	int i;
	int ret;

	for (i = 0;; i++) {
		info->error = 0;
		info->opcode = REISER4_VOLUME_HEADER;
		info->s.vol_idx = i;

		ret = ioctl(fd, REISER4_IOC_SCAN_DEV, info);
		if (ret || info->error)
			break;

		print_volume_header(info);
		list_bricks_of_volume(fd, info);
		print_separator();
	}
	return ret;
}

static void print_volume(struct reiser4_vol_op_args *info)
{
	rid_t vol, dst;
	aal_stream_t stream;
	reiser4_plug_t *vol_plug, *dst_plug;
	uint64_t stripe_size;
	uint64_t nr_segments;
	int nr_bricks;
	int bricks_in_dsa;

	aal_stream_init(&stream, stdout, &file_stream);

	vol = info->u.vol.vpid;
	dst = info->u.vol.dpid;

	stripe_size = 0;
	if (info->u.vol.stripe_bits != 0)
		stripe_size =  1ull << info->u.vol.stripe_bits;

	nr_segments = 0;
	if (info->u.vol.nr_sgs_bits != 0)
		nr_segments = 1ull << info->u.vol.nr_sgs_bits;

	nr_bricks = info->u.vol.nr_bricks;
	if (nr_bricks < 0) {
		/*
		 * negative number of bricks passed means
		 * that meta-data brick doesn't belong to
		 * data storage array
		 */
		nr_bricks = -nr_bricks;
		bricks_in_dsa = nr_bricks - 1;
	} else
		bricks_in_dsa = nr_bricks;

	if (!(dst_plug = reiser4_factory_ifind(DST_PLUG_TYPE, dst))) {
		aal_error("Can't find distrib plugin by its id 0x%x.", dst);
		return;
	}

	if (!(vol_plug = reiser4_factory_ifind(VOL_PLUG_TYPE, vol))) {
		aal_error("Can't find volume plugin by its id 0x%x.", vol);
		return;
	}

	aal_stream_format(&stream, "%s\n", "Logical Volume Info:");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*info->u.vol.id != '\0') {
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

	aal_stream_format(&stream, "bricks total:\t%d\n", nr_bricks);

	aal_stream_format(&stream, "bricks in DSA:\t%d\n", bricks_in_dsa);

	aal_stream_format(&stream, "slots:\t\t%u\n", info->u.vol.nr_mslots);

	aal_stream_format(&stream, "volinfo blocks:\t%llu\n",
			  info->u.vol.nr_volinfo_blocks);

	aal_stream_format(&stream, "balanced:\t%s\n",
			  aal_test_bit(&info->u.vol.fs_flags,
				       REISER4_UNBALANCED_VOL) ? "No" : "Yes");
	aal_stream_fini(&stream);
}

static void print_brick(struct reiser4_vol_op_args *info)
{
	aal_stream_t stream;
	int is_meta = (info->u.brick.int_id == 0);

	aal_stream_init(&stream, stdout, &file_stream);

	aal_stream_format(&stream, "%s\n", "Brick Info:");

	aal_stream_format(&stream, "internal ID:\t%u (%s)\n",
			  info->u.brick.int_id,
			  is_meta ? "meta-data brick" : "data brick");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*info->u.brick.ext_id != '\0') {
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

	aal_stream_fini(&stream);
}

int main(int argc, char *argv[]) {
	int c;
	int fd;
	int ret;
	struct stat st;
	char *name;
	int offline = 0;
	uint32_t flags = 0;
	struct reiser4_vol_op_args info;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"yes", no_argument, NULL, 'y'},
		{"register", required_argument, NULL, 'g'},
		{"unregister", required_argument, NULL, 'u'},
		{"list", no_argument, NULL, 'l'},
		{"print", required_argument, NULL, 'p'},
		{"balance", no_argument, NULL, 'b'},
		{"add", required_argument, NULL, 'a'},
		{"remove", required_argument, NULL, 'r'},
		{"resize", required_argument, NULL, 'z'},
		{"capacity", required_argument, NULL, 'c'},
		{"scale", required_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	volmgr_init();
	memset(&info, 0, sizeof(info));

	if (argc < 2) {
		volmgr_print_usage(argv[0]);
		return USER_ERROR;
	}
	while ((c = getopt_long(argc, argv, "hVyfblp:g:u:a:r:z:c:q:?",
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
		case 'r':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_REMOVE_BRICK);
			if (ret)
				return ret;
			break;
		case 'p':
			ret = set_op_value(&info, optarg,
					   REISER4_PRINT_BRICK);
			if (ret)
				return ret;
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
			ret = ioctl(fd, REISER4_IOC_SCAN_DEV, &info);
		}
	} else {
		struct stat buf;

		name = argv[optind];
		fd = open(name, O_NONBLOCK);
		if (fd == -1) {
			aal_error("Can't open %s. %s.", name, strerror(errno));
			goto error_free_libreiser4;
		}
		ret = fstat(fd, &buf);
		if (ret) {
			aal_error("%s: fstat failed %s.",
				  name, strerror(errno));
			goto error_free_libreiser4;
		}
		if (!S_ISDIR(buf.st_mode)) {
			ret = -1;
			aal_error("%s is not directory.", name);
			goto close;
		}
		ret = ioctl(fd, REISER4_IOC_VOLUME, &info);
		if (ret == -1)
			aal_error("Ioctl on %s failed. %s.",
				  name, strerror(errno));
	}
 close:
	if (close(fd) == -1)
		aal_error("Failed to close %s. %s.", name, strerror(errno));
	if (ret)
		goto error_free_libreiser4;

	switch(info.opcode) {
	case REISER4_PRINT_VOLUME:
		print_volume(&info);
		break;
	case REISER4_PRINT_BRICK:
		print_brick(&info);
		break;
	default:
		break;
	}
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
