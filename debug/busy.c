/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   busy.c -- program which contains differnt reiser4 stuff used in debug. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

static void busy_print_usage(void) {
	fprintf(stderr, "Usage: busy FILE DIR\n");
}

static void busy_init(void) {
	int i;
	for (i = 0; i < EXCEPTION_TYPE_LAST; i++)
		misc_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_object_t *dir;

	if (argc < 3) {
		busy_print_usage();
		return 0xfe;
	}
    
	busy_init();

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		return 0xff;
	}

	if (!(device = aal_device_open(&file_ops, argv[1], 
				       512, O_RDWR))) 
	{
		aal_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, 1))) {
		aal_error("Can't open filesystem on %s.", 
			  device->name);
		goto error_free_device;
	}

	fs->tree->mpc_func = misc_mpressure_detect;
    
//	misc_param_override("hash=deg_hash");
//	misc_param_override("policy=tails");
		
	if (!(fs->root = reiser4_object_open(fs->tree, "/", 1))) {
		aal_error("Can't open root dir.");
		goto error_free_fs;
	}

	if (!(dir = reiser4_object_open(fs->tree, argv[2], 1))) {
                aal_error("Can't open dir %s.", argv[2]);
                goto error_free_root;
        }
	
        {
                int i, k;
                char name[6][256];
		
//		FILE *file;
                reiser4_object_t *object;
		entry_hint_t entry;

		srandom(time(0));

//		file = fopen("/home/umka/tmp/out", "r");

//		while (!feof(file)) {
		for (i = 0; i < 1000; i++) {
//                        int j, count;
//			char part[256];
			uint64_t size[3] = {51200, 0, 1024};

			aal_snprintf(name[0], 256, "file name%d", i/*random()*/);
			aal_snprintf(name[1], 256, "file name%d.c", i/*random()*/);
			aal_snprintf(name[2], 256, "file name%d.o", i/*random()*/);
/*			aal_snprintf(name[3], 256, "very very very long file name%d", i);
			aal_snprintf(name[4], 256, "very very very long file name%d.c", i);
			aal_snprintf(name[5], 256, "very very very long file name%d.o", i);

//			fscanf(file, "%s %s\n", name, part);
//			strcat(name, " ");
//			strcat(name, part);
//			printf("%s\n", name);
*/
			for (k = 0; k < 3; k++) {
				if (!(object = reiser4_reg_create(fs, dir, name[k])))
					continue;

				
				if (reiser4_object_write(object, NULL, size[k]) < 0) {
					aal_error("Can't write data to file %s.", 
						  name[k]);
					break;
				}
#if 0
				count = 1000;

				for (j = 0; j < count; j++) {
					/* reiser4_object_seek(object, reiser4_object_offset(object) + 8193);*/

					if (reiser4_object_write(object, name[k],
								 aal_strlen(name[k])) < 0)
					{
						aal_error("Can't write data "
							  "to file %s.", name[k]);
						break;
					}
				}
#endif
				reiser4_object_close(object);
			}

			/* Get the second file and write there some more. */
			if (reiser4_object_lookup(dir, name[1], &entry) != PRESENT)  {
				aal_error("Cannot find just created object %s.", name[1]);
				break;
			}

			if (!(object = reiser4_object_launch(fs->tree, dir, 
							     &entry.object))) 
			{
				aal_error("Cannot open the object %s.", name[1]);
				break;
			}
			
			if (reiser4_object_write(object, NULL, 1024) < 0) {
				aal_error("Can't write data to file %s.", 
					  name[k]);
					break;
			}
                }
        }
	
	reiser4_object_close(fs->root);
	reiser4_tree_compress(fs->tree);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

/* error_free_dir:
	reiser4_object_close(dir);*/
 error_free_root:
	reiser4_object_close(fs->root);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
	return 0xff;
}
