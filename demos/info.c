/*
  info.c -- a demo program that demonstrates opening reiser4 and getting some information.
  Copyright (C) 1996-2002 Hans Reiser.
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

static void info_print_usage(void) {
	fprintf(stderr, "Usage: info FILE\n");
}

static void info_print_plugin(reiser4_plugin_t *plugin) {
	fprintf(stderr, "0x%x:0x%x:%s\n(%s)\n\n", 
		plugin->h.sign.type, plugin->h.sign.id, plugin->h.label, plugin->h.desc);
}

static void info_print_fs(reiser4_fs_t *fs) {
	reiser4_plugin_t *plugin;

	fprintf(stderr, "\nreiser4 %s, block size %u, blocks: %llu, used: %llu, free: %llu.\n\n", 
		reiser4_fs_name(fs), reiser4_fs_blocksize(fs), 
		reiser4_format_get_len(fs->format), reiser4_alloc_used(fs->alloc), 
		reiser4_alloc_free(fs->alloc));

	fprintf(stderr, "Used plugins:\n-------------\n");

	fprintf(stderr, "(1) ");
	info_print_plugin(fs->format->entity->plugin);
    
	if (fs->journal) {
		fprintf(stderr, "(2) ");
		info_print_plugin(fs->journal->entity->plugin);
	}

	fprintf(stderr, "(3) ");
	info_print_plugin(fs->alloc->entity->plugin);
    
	fprintf(stderr, "(4) ");
	info_print_plugin(fs->oid->entity->plugin);
    
	fprintf(stderr, "(5) ");
	info_print_plugin(fs->tree->key.plugin);
    
	fprintf(stderr, "(6) ");
	info_print_plugin(fs->tree->root->node->entity->plugin);
    
	fprintf(stderr, "(7) ");
	info_print_plugin(fs->root->entity->plugin);
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;

#ifndef ENABLE_COMPACT    
    
	if (argc < 2) {
		info_print_usage();
		return 0xfe;
	}
    
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		return 0xff;
	}
    
	{
		int i;
		for (i = 0; i < 5; i++)
			progs_exception_set_stream(i, stderr);
	}
    
	if (!(device = aal_device_open(&file_ops, argv[1], 
		BLOCKSIZE, O_RDONLY))) 
	{
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, device, 0))) {
		aal_exception_error(
			"Can't open filesystem on %s.", aal_device_name(device));
		goto error_free_device;
	}
    
	if (!(fs->root = reiser4_file_open(fs, "/"))) {
		aal_exception_error("Can't open root directory.");
		goto error_free_fs;
	}
    
	info_print_fs(fs);
    
	reiser4_file_close(fs->root);
    
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
 error:
    
#endif
    
	return 0xff;
}
