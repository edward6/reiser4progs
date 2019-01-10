/*
  Copyright (c) 2017 Eduard O. Shishkin

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* Program for managing Reiser4 logical volumes */

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

/* Known volmgr behavior flags */
typedef enum behav_flags {
	BF_FORCE      = 1 << 0,
	BF_YES        = 1 << 1,
} behav_flags_t;

/* Prints options */
static void volmgr_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
	fprintf(stderr,
		"Volume manager options:\n"
		"Plugins options:\n"
		"  -p, --print N                 print information about brick with id N.\n"
	        "  -g, --register FILE           register a subvolume accociated with \n"
		"                                device\"FILE\".\n"
		"  -b, --balance                 balance logical volume.\n"
	        "  -e, --expand SIZE             augment data room of a brick on specified \"SIZE\".\n"
	        "  -s, --shrink SIZE             shrink data room of a brick on specified SIZE.\n"
		"  -a, --add FILE                add a brick accociated with device\"FILE\" \n"
	        "                                to the logical volume\n"
	        "  -r, --remove FILE             remove a brick accociated with device \n"
		"                                \"FILE\" from the logical volume\n"
	        "  -q, --scale N                 increase in \"N\" times maximal allowed\n"
		"                                number of bricks in the logical volume\n"
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

static int set_op_delta(struct reiser4_vol_op_args *info,
			char *num, reiser4_vol_op op)
{
	if ((info->delta = misc_str2long(num, 10)) == INVAL_DIG)
		return USER_ERROR;
	if (set_op(info, op))
		return USER_ERROR;
	return NO_ERROR;
}

static int set_op_brick_idx(struct reiser4_vol_op_args *info,
			    char *num, reiser4_vol_op op)
{
	if ((info->s.brick_idx = misc_str2long(num, 10)) == INVAL_DIG)
		return USER_ERROR;
	if (set_op(info, op))
		return USER_ERROR;
	return NO_ERROR;
}

static void print_volume(struct reiser4_vol_op_args *info)
{
	rid_t vol, dst;
	aal_stream_t stream;
	reiser4_plug_t *vol_plug, *dst_plug;
	int nr_bricks;
	int bricks_in_dsa;

	aal_stream_init(&stream, stdout, &file_stream);

	vol = info->u.vol.vpid;
	dst = info->u.vol.dpid;

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

	aal_stream_format(&stream, "volume:\t\t0x%x (%s)\n",
			  vol, vol_plug ? vol_plug->label : "absent");

	aal_stream_format(&stream, "distribution:\t0x%x (%s)\n",
			  dst, dst_plug ? dst_plug->label : "absent");

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

	aal_stream_init(&stream, stdout, &file_stream);

	aal_stream_format(&stream, "%s\n", "Brick Info:");

	aal_stream_format(&stream, "internal ID:\t%u\n",
			  info->u.brick.int_id);

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

	aal_stream_format(&stream, "data room:\t%llu\n",
			  info->u.brick.data_room);

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
	char *mnt;
	uint32_t flags = 0;
	struct reiser4_vol_op_args info;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"yes", no_argument, NULL, 'y'},
		{"register", required_argument, NULL, 'g'},
		{"print", required_argument, NULL, 'p'},
		{"balance", no_argument, NULL, 'b'},
		{"check", no_argument, NULL, 'c'},
		{"expand", required_argument, NULL, 'e'},
		{"add", required_argument, NULL, 'a'},
		{"shrink", required_argument, NULL, 's'},
		{"remove", required_argument, NULL, 'r'},
		{"scale", required_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	volmgr_init();
	memset(&info, 0, sizeof(info));

	if (argc < 2) {
		volmgr_print_usage(argv[0]);
		return USER_ERROR;
	}
	while ((c = getopt_long(argc, argv, "hVyfbcp:g:e:a:s:r:q:?",
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
		case 'c':
			ret = set_op(&info, REISER4_CHECK_VOLUME);
			if (ret)
				return ret;
			break;
		case 'g':
			ret = set_op_name(&info, optarg, &st,
					  REISER4_REGISTER_BRICK);
			if (ret)
				return ret;
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
			ret = set_op_brick_idx(&info, optarg,
					       REISER4_PRINT_BRICK);
			if (ret)
				return ret;
			break;
		case 'e':
			ret = set_op_delta(&info, optarg,
					   REISER4_EXPAND_BRICK);
			if (ret)
				return ret;
			break;
		case 's':
			ret = set_op_delta(&info, optarg,
					   REISER4_SHRINK_BRICK);
			if (ret)
				return ret;
			break;
		case 'q':
			ret = set_op_delta(&info, optarg,
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
	mnt = argv[optind];
#if 0
	/*
	 * Checking if passed argument is a mounted volume
	 */
	if (misc_dev_mounted(mnt) <= 0) {
		aal_error("Device %s is not mounted at the moment. "
			  "Mount the device.", mnt);
		goto error_free_libreiser4;
	}
#endif
	fd = open(mnt, O_NONBLOCK);
	if (fd == -1) {
		aal_error("Can't open %s. %s.", mnt, strerror(errno));
		goto error_free_libreiser4;
	}
	ret = ioctl(fd, REISER4_IOC_VOLUME, &info);

	if (close(fd) == -1)
		aal_error("Failed to close %s. %s.", mnt, strerror(errno));
	if (ret == -1) {
		aal_error("Ioctl on %s failed. %s.", mnt, strerror(errno));
		goto error_free_libreiser4;
	}

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
