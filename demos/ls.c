/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ls.c -- a demo program which works like simple variant of standard ls
   utility. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

static void ls_print_usage(void) {
	fprintf(stderr, "Usage: ls FILE DIR\n");
}

static void ls_init(void) {
	int i;
	for (i = 0; i < EXCEPTION_TYPE_LAST; i++)
		misc_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	char buff[4096];
	reiser4_fs_t *fs;
	aal_device_t *device;

	entry_hint_t entry;
	reiser4_object_t *dir;

	if (argc < 3) {
		ls_print_usage();
		return 0xfe;
	}
    
	ls_init();

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
    
	if (!(fs->root = reiser4_semantic_open(fs->tree, "/", 1))) {
		aal_error("Can't open root dir.");
		goto error_free_fs;
	}
    
	if (!(dir = reiser4_semantic_open(fs->tree, argv[2], 1))) {
		aal_error("Can't open dir %s.", argv[2]);
		goto error_free_root;
	}

	while (reiser4_object_readdir(dir, &entry) > 0) {
		aal_snprintf(buff, sizeof(buff), "[%s] %s\n",
			     reiser4_print_key(&entry.object, PO_DEFAULT),
			     entry.name);

		printf(buff);
	}
	
	reiser4_object_close(dir);
	reiser4_object_close(fs->root);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

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

