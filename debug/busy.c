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

#define TESTS_COUNT 3

char *tests[TESTS_COUNT] = {
	[0] = "reg",
	[1] = "sym",
	[2] = "read"
};

char *options[TESTS_COUNT] = {
	[0] = "DEVICE DIR",
	[1] = "DEVICE DIR",
	[2] = "DEVICE file_on_fs output"
};


static void busy_print_usage(void) {
	int i;

	fprintf(stderr, "Usage: busy test\n");
	fprintf(stderr, "tests: \n");
	
	for (i = 0; i < TESTS_COUNT; i++)
		fprintf(stderr, "\t%s %s\n", tests[i], options[i]);
		
	fprintf(stderr, "\n");
}

static int busy_test(char *name) {
	int i;

	for (i = 0; i < TESTS_COUNT; i++) {
		if (aal_strlen(name) != aal_strlen(tests[i]))
			continue;
		
		if (!aal_memcmp(name , tests[i], aal_strlen(tests[i])))
			return i;
	}

	return -1;
}

static void busy_init(void) {
	int i;
	for (i = 0; i < EXCEPTION_TYPE_LAST; i++)
		misc_exception_set_stream(i, stderr);
}

static void sym_test(reiser4_fs_t *fs, char *path) {
	reiser4_object_t *dir0, *dir1, *dir2, *sym0, *sym1, *reg0;
	char name[256];
	
	if (!(dir0 = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return;
        }

	if (!(dir1 = reiser4_dir_create(fs, dir0, "test1 dir1"))) {
		aal_error("Failed to create a dir 'test1 dir1'.");
		return;
	}

	if (!(dir2 = reiser4_dir_create(fs, dir0, "test1 dir2"))) {
		aal_error("Failed to create a dir 'test1 dir2'.");
		return;
	}
	
	if (!(reg0 = reiser4_reg_create(fs, dir1, "test1 file0"))) {
		aal_error("Failed to create a reg 'test1 file0'.");
		return;
	}
	
	if (!(sym0 = reiser4_sym_create(fs, dir2, "test1 sym0", 
					"../test1 dir1/test1 file0")))
	{
		aal_error("Failed to create a sym 'test1 sym0'.");
		return;
	}
	
	aal_snprintf(name, 256, "%s/test1 dir1/test1 file0", path);
	
	if (!(sym1 = reiser4_sym_create(fs, dir2, "test1 sym1", name))) {
		aal_error("Failed to create a sym 'test1 sym1'.");
		return;
	}
	
	reiser4_object_close(dir0);
	reiser4_object_close(dir1);
	reiser4_object_close(dir2);
	reiser4_object_close(reg0);
	reiser4_object_close(sym0);
	reiser4_object_close(sym1);
	
	aal_snprintf(name, 256, "%s/test1 dir2/test1 sym0", path);

	if (!(sym0 = reiser4_semantic_open(fs->tree, name, NULL, 1))) {
		aal_error("Failed to open a sym '%s'.", name);
		return;
	}

	aal_snprintf(name, 256, "%s/test1 dir2/test1 sym1", path);
	
	if (!(sym1 = reiser4_semantic_open(fs->tree, name, NULL, 1))) {
		aal_error("Failed to create a sym '%s'.", name);
		return;
	}
	
	reiser4_object_close(sym0);
	reiser4_object_close(sym1);

	reiser4_tree_compress(fs->tree);
	return;
}

static void read_test(reiser4_fs_t *fs, char *path, char *out) {
	reiser4_object_t *obj;
	FILE *file;
	char buf[40960];
	int read, wrote;

#define ENABLE_STAND_ALONE	
	if (!path) {
		aal_error("No file on the filesystem is specified.");
		return;
	}
		
	if (!out) {
		aal_error("No output file is specified.");
		return;
	}
	
	if (!(obj = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return;
        }

	if (obj->ent->opset.plug[OPSET_OBJ]->id.group != REG_OBJECT) {
		aal_error("Not regular file is specified %s.", path);
		goto error;
	}

	
	if (!(file = fopen(out, "w"))) {
		aal_error("Failed to open the output file %s.", out);
		goto error;
	}
	
	while ((read = reiser4_object_read(obj, buf, 40960)) > 0) {
		wrote = fwrite(buf, 1, read, file);
		
		if (read != wrote)
			aal_error("Write failed: read %d bytes, wrote %d.", read, wrote);
	}

	if (read < 0)
		aal_error("Read failed.");
	
	fclose(file);
	reiser4_object_close(obj);
#undef ENABLE_STAND_ALONE	
	
	return;
	
 error:
	reiser4_object_close(obj);
	return;
}

static void reg_test(reiser4_fs_t *fs, char *path) {
	reiser4_object_t *dir;
	
	int i, k;
	char name[6][256];

	//FILE *file;
	reiser4_object_t *object;
	entry_hint_t entry;

//	misc_param_override("hash=deg_hash");
//	misc_param_override("policy=tails");

	if (!(dir = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return;
        }
	
	srandom(time(0));

	//		file = fopen("/home/umka/tmp/out", "r");

	//		while (!feof(file)) {
	for (i = 0; i < 1000; i++) {
		//                        int j, count;
		//			char part[256];
		uint64_t size[3] = {51200, 0, 1024};

		aal_snprintf(name[0], 256, "test0 file name%d", i/*random()*/);
		aal_snprintf(name[1], 256, "test0 file name%d.c", i/*random()*/);
		aal_snprintf(name[2], 256, "test0 file name%d.o", i/*random()*/);
		/*
		aal_snprintf(name[3], 256, "test0 very very long file name%d", i);
		aal_snprintf(name[4], 256, "test0 very very long file name%d.c", i);
		aal_snprintf(name[5], 256, "test0 very very long file name%d.o", i);

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
				/* reiser4_object_seek(object, 
				   reiser4_object_offset(object) + 8193);*/

				if (reiser4_object_write(object, name[k],
							 aal_strlen(name[k])) < 0)
				{
					aal_error("Can't write data "
						  "to file %s.", name[k]);
					break;
				}
			}
#endif
			if (object) {
				reiser4_object_close(object);
			} else {
				aal_error("Can't create an object '%s'.", 
					  name[k]);
			}

			/* Get the second file and write there some more. */
			if (reiser4_object_lookup(dir, name[k], &entry) != 
			    PRESENT)  
			{
				aal_error("Can't find just created object %s.",
					  name[1]);
				break;
			}

			if (!(object = reiser4_object_obtain(fs->tree, dir, 
							     &entry.object))) 
			{
				aal_error("Can't open the object %s.", name[1]);
				break;
			}

			if (reiser4_object_write(object, NULL, 1024) < 0) {
				aal_error("Can't write data to file %s.", 
					  name[k]);
				break;
			}

			reiser4_object_close(object);
		}
	}
	
	reiser4_object_close(dir);
	reiser4_tree_compress(fs->tree);
	return ;
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;
	
	if (argc < 3) {
		busy_print_usage();
		return 0xfe;
	}

	busy_init();

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		return 0xff;
	}

	if (!(device = aal_device_open(&file_ops, argv[2], 512, O_RDWR))) {
		aal_error("Can't open device %s.", argv[2]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, 1))) {
		aal_error("Can't open filesystem on %s.", 
			  device->name);
		goto error_free_device;
	}

	fs->tree->mpc_func = misc_mpressure_detect;
    
	switch(busy_test(argv[1])) {
	case 0:
		reg_test(fs, argv[3]);
		break;
	case 1:
		sym_test(fs, argv[3]);
		break;
	case 2:
		read_test(fs, argv[3], argv[4]);
		break;
	default:
		aal_error("Can't find test '%s'.", argv[1]);
	}
		
	reiser4_fs_close(fs);
	aal_device_close(device);
	libreiser4_fini();
    
	return 0;

 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
	return 0xff;
}
