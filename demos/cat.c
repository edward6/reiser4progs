/*
  cat.c -- a demo program which works like standard cat utility.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void cat_print_usage(void) {
	fprintf(stderr, "Usage: ls DEV FILE\n");
}

static void cat_init(void) {
	int i;
	for (i = 0; i < 5; i++)
		misc_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;
	unsigned char buff[4096];

	reiser4_object_t *reg;

	if (argc < 3) {
		cat_print_usage();
		return 0xfe;
	}
    
	cat_init();

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		return 0xff;
	}
    
	if (!(device = aal_device_open(&file_ops, argv[1], 
		REISER4_SECSIZE, O_RDWR))) 
	{
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, misc_profile_find("smart40")))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    aal_device_name(device));
		goto error_free_device;
	}

	if (!(fs->tree = reiser4_tree_init(fs, misc_mpressure_detect)))
		goto error_free_fs;
    
	if (!(reg = reiser4_object_open(fs, argv[2], TRUE)))
		goto error_free_tree;

	if (reg->entity->plugin->h.group != FILE_OBJECT) {
		aal_exception_error("File %s is not a regular file.",
				    argv[2]);
		goto error_free_reg;
	}
	
	while (1) {
		int32_t read;
		
		aal_memset(buff, 0, sizeof(buff));

		read = reiser4_object_read(reg, buff, 
					   sizeof(buff));
		
		if (read <= 0)
			break;
		
		write(1, buff, read);
	}
    
	reiser4_object_close(reg);

	reiser4_tree_fini(fs->tree);
	reiser4_fs_sync(fs);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

 error_free_reg:
	reiser4_object_close(reg);
 error_free_tree:
	reiser4_tree_fini(fs->tree);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
	return 0xff;
}
