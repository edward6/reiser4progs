/*
  plugin.c -- a demo program which shows how to create and use new reiser4
  plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <stdio.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void plugin_print_usage(void) {
	fprintf(stderr, "Usage: plugin DEV\n");
}

static void plugin_init(void) {
	int i;
	for (i = 0; i < 5; i++)
		progs_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;

#ifndef ENABLE_COMPACT    
    
	if (argc < 2) {
		plugin_print_usage();
		return 0xfe;
	}
    
	plugin_init();

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		return 0xff;
	}
    
	if (!(device = aal_device_open(&file_ops, argv[1], 
		BLOCKSIZE, O_RDWR))) 
	{
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, device))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    aal_device_name(device));
		goto error_free_device;
	}

	/* Here will be some code */
	
	reiser4_fs_close(fs);
    
	libreiser4_done();
	aal_device_close(device);
    
	return 0;

 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_done();
    
#endif
	return 0xff;
}

