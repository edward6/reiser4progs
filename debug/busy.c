/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   busy.c -- program which contains differnt reiser4 stuff used in debug. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void busy_print_usage(void) {
	fprintf(stderr, "Usage: busy FILE MODE\n");
}

static void busy_init(void) {
	int i;
	for (i = 0; i < 5; i++)
		misc_exception_set_stream(i, stderr);
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

	/* Here will be some actions after we decide what we have to do. At
	   least some kind of stress will be here (creating huge number of
	   files, remove them , etc). This should be enough for first times. */
    
	reiser4_object_close(fs->root);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
	return 0xff;
}
