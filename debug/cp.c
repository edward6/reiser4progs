/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cat.c -- a demo program which works like standard cat utility. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

static void cat_print_usage(void) {
	fprintf(stderr, "Usage: ls DEV FILE\n");
}

static void cat_init(void) {
	int i;
	for (i = 0; i < EXCEPTION_TYPE_LAST; i++)
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
	
	if (!(reg = reiser4_semantic_open(fs->tree, argv[2], NULL, 1)))
		goto error_free_fs;

	if (reg->ent->opset.plug[OPSET_OBJ]->id.group != REG_OBJECT) {
		aal_error("File %s is not a regular file.",
			  argv[2]);
		goto error_free_reg;
	}

	while (1) {
		int32_t read;
		
		aal_memset(buff, 0, sizeof(buff));
		read = reiser4_object_read(reg, buff, sizeof(buff));
		if (read <= 0)
			break;
		
		write(1, buff, read);
	}
    
	reiser4_object_close(reg);
	reiser4_fs_close(fs);
	
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

 error_free_reg:
	reiser4_object_close(reg);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
	return 0xff;
}
