/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   busy.c -- program which contains differnt reiser4 stuff used in debug. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

static void busy_print_usage(void) {
	fprintf(stderr, "Usage: busy FILE DIR\n");
}

static void busy_init(void) {
	int i;
	for (i = 0; i < 5; i++)
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

//	misc_param_override("hash=deg_hash");
//	misc_param_override("policy=tails");
		
	if (!(device = aal_device_open(&file_ops, argv[1], 
				       512, O_RDWR))) 
	{
		aal_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, TRUE))) {
		aal_error("Can't open filesystem on %s.", 
			  device->name);
		goto error_free_device;
	}

	fs->tree->mpc_func = misc_mpressure_detect;
    
	if (!(fs->root = reiser4_object_open(fs->tree, "/", TRUE))) {
		aal_error("Can't open root dir.");
		goto error_free_fs;
	}

	if (!(dir = reiser4_object_open(fs->tree, argv[2], TRUE))) {
                aal_error("Can't open dir %s.", argv[2]);
                goto error_free_root;
        }
	
        {
                int i;
                char name[256];
                reiser4_object_t *object;
                                                                                       
                for (i = 0; i < 1000; i++) {
                        int j, count;
                                                                                       
                        aal_snprintf(name, 256, "file name%d", i);
                                                                                       
                        if (!(object = reiser4_reg_create(fs, dir, name)))
                                goto error_free_dir;
                                                                                       
                        count = 1000;
                                                                                       
                        for (j = 0; j < count; j++) {
                                if (reiser4_object_write(object, name,
                                                         aal_strlen(name)) < 0)
                                {
                                        aal_error("Can't write data "
                                                  "to file %s.", name);
                                }
                        }
                                                                                       
                        reiser4_object_close(object);
                }
        }
	
	reiser4_object_close(fs->root);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

 error_free_dir:
	reiser4_object_close(dir);
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
