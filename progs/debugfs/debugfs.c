/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debugfs.c -- program for debugging reiser4 filesystem. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "debugfs.h"

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Browsing options:\n"
		"  -k, --cat FILE                browses passed file like standard\n"
		"                                cat and ls programs.\n"
		"Print options:\n"
		"  -s, --print-super             prints the both super blocks.\n"
		"  -t, --print-tree              prints the whole tree.\n"
		"  -j, --print-journal           prints journal.\n"
		"  -d, --print-oid               prints oid allocator data.\n"
		"  -a, --print-alloc             prints block allocator data.\n"
		"  -b, --print-block N           prints block by its number.\n"
		"  -n, --print-nodes FILE        prints all nodes file lies in.\n"
		"  -i, --print-file FILE         prints all items specified file\n"
		"                                consists of.\n"
		"Metadata options:\n"
		"  -P, --pack-metadata           fetches filesystem metadata and writes it\n"
		"                                to standard output.\n"
		"  -U, --unpack-metadata         uses metadata stream from stdandard input\n"
		"                                to construct filesystem by it.\n"
		"Space options:\n"
		"  -O, --occupied-blocks         works with occupied blocks only(default).\n"
		"  -F, --free-blocks             works with free blocks only.\n"
		"  -W, --whole-partition         works with the whole partition.\n"
		"  -B, --bitmap                  works with blocks marked in the bitmap only.\n"
		"Plugins options:\n"
		"  -p, --print-profile           prints the plugin profile.\n"
		"  -l, --print-plugins           prints known plugins.\n"
	        "  -o, --override TYPE=PLUGIN    overrides the default plugin of the type\n"
	        "                                \"TYPE\" by the plugin \"PLUGIN\" in the\n"
		"                                profile.\n"
		"Common options:\n"
		"  -?, -h, --help                prints program usage.\n"
		"  -V, --version                 prints current version.\n"
		"  -q, --quiet                   forces using filesystem without\n"
		"                                any questions.\n"
		"  -f, --force                   makes debugfs to use whole disk, not\n"
		"                                block device or mounted partition.\n"
		"  -c, --cache N                 number of nodes in tree buffer cache\n");
}

/* Initializes exception streams used by debugfs */
static void debugfs_init(void) {
	int ex;

	/* Setting up exception streams. */
	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
	
	/* All steams should go to stderr, as in/out are used for pack/unpack 
	   and other needs */
	misc_exception_set_stream(EXCEPTION_TYPE_MESSAGE, stderr);
	misc_exception_set_stream(EXCEPTION_TYPE_INFO, stderr);
	misc_exception_set_stream(EXCEPTION_TYPE_FSCK, NULL);
}

int main(int argc, char *argv[]) {
	int c;

	uint32_t cache;
	struct stat st;
	char *host_dev;

	uint32_t print_flags = 0;
	uint32_t behav_flags = 0;
	uint32_t space_flags = 0;

	char override[4096];
	char *cat_filename = NULL;
	char *print_filename = NULL;
    
	aal_device_t *device;
	reiser4_fs_t *fs = NULL;
	blk_t blocknr = 0;

	FILE *file = NULL;
	char *bm_file = NULL;
	aux_bitmap_t *bitmap = NULL;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"quiet", no_argument, NULL, 'q'},
		{"cat", required_argument, NULL, 'k'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-alloc", no_argument, NULL, 'a'},
		{"print-oid", no_argument, NULL, 'd'},
		{"print-block", required_argument, NULL, 'b'},
		{"print-nodes", required_argument, NULL, 'n'},
		{"print-file", required_argument, NULL, 'i'},
		{"pack-metadata", no_argument, NULL, 'P'},
		{"unpack-metadata", no_argument, NULL, 'U'},
		{"print-profile", no_argument, NULL, 'p'},
		{"print-plugins", no_argument, NULL, 'l'},
		{"override", required_argument, NULL, 'o'},
		{"occupied-blocks", no_argument, NULL, 'O'},
		{"free-blocks", no_argument, NULL, 'F'},
		{"bitmap", required_argument, NULL, 'B'},
		{"whole-partition", no_argument, NULL, 'W'},
		{"cache", required_argument, NULL, 'c'},
		{0, 0, 0, 0}
	};

	debugfs_init();
	memset(override, 0, sizeof(override));
	
	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVqftb:djk:n:i:o:plsaPUOFWB:c:",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'd':
			print_flags |= PF_OID;
			break;
		case 'a':
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
		case 'b': {
			print_flags |= PF_BLOCK;

			/* Parsing block number */
			if ((blocknr = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid block number (%s).", optarg);
				return USER_ERROR;
			}
			
			break;
		}
		case 'n':
			print_flags |= PF_NODES;
			print_filename = optarg;
			break;
		case 'i':
			print_flags |= PF_ITEMS;
			print_filename = optarg;
			break;
		case 'k':
			behav_flags |= BF_CAT;
			cat_filename = optarg;
			break;
		case 'c':
			if ((cache = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid cache value specified (%s).",
					  optarg);
				return USER_ERROR;
			}

			misc_mpressure_setup(cache);
			break;
		case 'f':
			behav_flags |= BF_FORCE;
			break;
		case 'q':
			behav_flags |= BF_QUIET;
			break;
		case 'P':
			behav_flags |= BF_PACK_META;
			break;
		case 'U':
			behav_flags |= BF_UNPACK_META;
			break;
		case 'l':
			behav_flags |= BF_SHOW_PLUG;
			break;
		case 'p':
			behav_flags |= BF_SHOW_PARM;
			break;
		case 'o':
			aal_strncat(override, optarg, aal_strlen(optarg));
			aal_strncat(override, ",", 1);
			break;
		case '?':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'O':
			space_flags = 0;
			break;
		case 'F':
			space_flags |= SF_FREE;
			break;
		case 'W':
			space_flags |= SF_WHOLE;
			break;
		case 'B':
			bm_file = optarg;
			break;
		}
	}
    
	if (!(behav_flags & BF_QUIET))
		misc_print_banner(argv[0]);

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Overriding default params by passed values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';
		
		if (!(behav_flags & BF_QUIET)) {
			aal_mess("Overriding the plugin profile by \"%s\".", 
				 override);
		}
		
		if (misc_profile_override(override))
			goto error_free_libreiser4;
	}
	
	if (behav_flags & BF_SHOW_PARM)
		misc_profile_print();

	if (behav_flags & BF_SHOW_PLUG)
		misc_plugins_print();

	if (optind >= argc)
		goto error_free_libreiser4;
		
	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_error("Can't stat %s. %s.", host_dev, strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(host_dev, "rw") && !(behav_flags & BF_FORCE)) {
		aal_error("Device %s is mounted 'rw' at the moment. "
			  "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev, 512,
				       (behav_flags & BF_UNPACK_META) ?
				       O_RDWR : O_RDONLY)))
	{
		aal_error("Can't open %s. %s.", host_dev, strerror(errno));
		goto error_free_libreiser4;
	}

			
	if (behav_flags & BF_UNPACK_META) {
		aal_stream_t stream;
		
		/* Prepare the bitmap if needed. */
		if (bm_file) {
			if ((file = fopen(bm_file, "w+")) == NULL) {
				aal_fatal("Can't open the bitmap "
					  "file (%s).", bm_file);
				goto error_free_device;
			}

			if (!(bitmap = aux_bitmap_create(0))) {
				aal_error("Can't allocate a bitmap.");
				goto error_close_file;
			}
		}
		
		aal_stream_init(&stream, stdin, &file_stream);
		
		if (!(fs = repair_fs_unpack(device, bitmap, &stream))) {
			aal_error("Can't unpack filesystem.");
			goto error_free_bitmap;
		}

		aal_stream_fini(&stream);

		if (reiser4_fs_sync(fs)) {
			aal_error("Can't save unpacked filesystem.");
			goto error_free_bitmap;
		}

		/* Save the bitmap. */
		if (bitmap) {
			aal_stream_init(&stream, file, &file_stream);

			if (aux_bitmap_pack(bitmap, &stream)) {
				aal_error("Can't pack the bitmap of "
					  "unpacked blocks.");
				goto error_free_bitmap;
			}

			aal_stream_fini(&stream);
			aux_bitmap_close(bitmap);
			fclose(file);
			bitmap = NULL;
			file = NULL;
		}

		goto done;
	} else {
		/* Open file system on the device */
		if (!(fs = reiser4_fs_open(device, 0))) {
			aal_error("Can't open reiser4 on %s", host_dev);
			goto error_free_device;
		}

		reiser4_opset_profile(fs->tree->ent.opset);
	}
	
	if (behav_flags & BF_PACK_META /* || print disk blocks */) {
		uint64_t len = reiser4_format_get_len(fs->format);
		aal_stream_t stream;
		
		if (bm_file) {
			/* Read the bitmap from the file. */
			if ((file = fopen(bm_file, "r")) == NULL) {
				aal_fatal("Can't open the bitmap "
					  "file (%s).", bm_file);
				goto error_free_fs;
			}
		
			aal_stream_init(&stream, file, &file_stream);

			if (!(bitmap = aux_bitmap_unpack(&stream))) {
				aal_error("Can't unpack the bitmap of "
					  "packed blocks.");
				goto error_close_file;
			}

			aal_stream_fini(&stream);
			fclose(file);
			file = NULL;
		} else {
			if (!(bitmap = aux_bitmap_create(len))) {
				aal_error("Can't allocate a bitmap.");
				goto error_free_fs;
			}

			/* Used || free blocks - extract allocator to bitmap. */
			if (space_flags == 0 || space_flags & SF_FREE) {
				if (reiser4_alloc_extract(fs->alloc, bitmap)) {
					aal_error("Can't extract the space "
						  "allocator data to bitmap.");
					goto error_free_bitmap;
				}
			}

			/* Whole partition || free blocks - invert bitmap. */
			if (space_flags & SF_WHOLE || space_flags & SF_FREE) {
				aux_bitmap_invert(bitmap);
			}
		}
	}

	/* Opening the journal */
	if (!(fs->journal = reiser4_journal_open(fs, device))) {
		aal_error("Can't open journal on %s", host_dev);
		goto error_free_bitmap;
	}
		
	/* In the case no print flags was specified, debugfs will print super
	   blocks by defaut. */
	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;

	/* Handling print options */
	if ((behav_flags & BF_CAT)) {
		if (debugfs_browse(fs, cat_filename))
			goto error_free_journal;
	}
	
	if (print_flags & PF_SUPER) {
		debugfs_print_master(fs);
		debugfs_print_format(fs);
		debugfs_print_status(fs);
	}
    
	if (print_flags & PF_OID)
		debugfs_print_oid(fs);
    
	if (print_flags & PF_ALLOC)
		debugfs_print_alloc(fs);
    
	if (print_flags & PF_JOURNAL)
		debugfs_print_journal(fs);
    
	if (print_flags & PF_TREE)
		debugfs_print_tree(fs);

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr))
			goto error_free_journal;
	}
    
	if (print_flags & PF_NODES || print_flags & PF_ITEMS) {
		if (debugfs_print_file(fs, print_filename, print_flags))
			goto error_free_journal;
	}

	if (behav_flags & BF_PACK_META) {
//		void *error;
		aal_stream_t stream;

		aal_stream_init(&stream, stdout, &file_stream);
		
		/* FIXME-VITALY: This is needed to not print all found node
		   corruptions, but this also avoid prining useful pack
		   errors. */

		/* FIXME-UMKA: Probably some kind of mask in libaal/exception
		   for masking not needed exceptions would be helpful. */
//		error = misc_exception_get_stream(EXCEPTION_TYPE_ERROR);
//		misc_exception_set_stream(EXCEPTION_TYPE_ERROR, NULL);
		
		if (repair_fs_pack(fs, bitmap, &stream)) {
//			misc_exception_set_stream(EXCEPTION_TYPE_ERROR, error);
			aal_error("Can't pack filesystem.");
			goto error_free_journal;
		}
		
//		misc_exception_set_stream(EXCEPTION_TYPE_ERROR, error);
		aal_stream_fini(&stream);
		aux_bitmap_close(bitmap);
		bitmap = NULL;
	}
	
	/* Closing the journal */
	reiser4_journal_close(fs->journal);
	
 done:
	/* Closing filesystem itself */
	reiser4_fs_close(fs);

	/* Closing device */
	aal_device_close(device);
    
	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   while doing this. */
	libreiser4_fini();
	return NO_ERROR;

 error_free_journal:
	reiser4_journal_close(fs->journal);
 error_free_bitmap:
	if (bitmap) {
		aux_bitmap_close(bitmap);
	}
 error_close_file:
	if(file) {
		fclose(file);
	}
 error_free_fs:
	if (fs) {
		reiser4_fs_close(fs);
	}
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}

