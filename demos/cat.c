/*
    cat.c -- a demo program which works like standard cat utility.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <stdio.h>
#  include <fcntl.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void cat_print_usage(void) {
    fprintf(stderr, "Usage: ls DEV FILE\n");
}

static void cat_init(void) {
    int i;
    for (i = 0; i < 5; i++)
	progs_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
    char buff[256];
    reiser4_fs_t *fs;
    aal_device_t *device;

    reiser4_file_t *reg;
    reiser4_entry_hint_t entry;

#ifndef ENABLE_COMPACT    
    
    if (argc < 3) {
	cat_print_usage();
	return 0xfe;
    }
    
    cat_init();

    if (libreiser4_init()) {
	aal_exception_error("Can't initialize libreiser4.");
	return 0xff;
    }
    
    if (!(device = aal_file_open(argv[1], DEFAULT_BLOCKSIZE, O_RDONLY))) {
	aal_exception_error("Can't open device %s.", argv[1]);
	goto error_free_libreiser4;
    }
    
    if (!(fs = reiser4_fs_open(device, device, 0))) {
	aal_exception_error("Can't open filesystem on %s.", 
	    aal_device_name(device));
	goto error_free_device;
    }
    
    if (!(reg = reiser4_file_open(fs, argv[2]))) {
	aal_exception_error("Can't open file %s.", argv[2]);
	goto error_free_fs;
    }
    
    while (1) {
	aal_memset(buff, 0, sizeof(buff));

	if (!reiser4_file_read(reg, buff, sizeof(buff) - 1))
	    break;

	printf("%s", buff);
    }
    
    reiser4_file_close(reg);
    reiser4_fs_close(fs);
    
    libreiser4_done();
    aal_file_close(device);
    
    return 0;

error_free_reg:
    reiser4_file_close(reg);
error_free_fs:
    reiser4_fs_close(fs);
error_free_device:
    aal_file_close(device);
error_free_libreiser4:
    libreiser4_done();
    
#endif
    
    return 0xff;
}

