/*
  ls.c -- a demo program which works like standard ls utility.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void ls_print_usage(void) {
	fprintf(stderr, "Usage: ls FILE DIR\n");
}

static void ls_init(void) {
	int i;
	for (i = 0; i < 5; i++)
		progs_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	char buff[4096];
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_place_t place;

	entry_hint_t entry;
	reiser4_object_t *dir;

	if (argc < 3) {
		ls_print_usage();
		return 0xfe;
	}
    
	ls_init();

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
    
	if (!(fs = reiser4_fs_open(device, progs_profile_find("smart40")))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    aal_device_name(device));
		goto error_free_device;
	}
    
	if (!(fs->tree = reiser4_tree_init(fs, progs_mpressure_detect)))
		goto error_free_fs;
    
	if (!(fs->root = reiser4_object_open(fs, "/", TRUE))) {
		aal_exception_error("Can't open root dir.");
		goto error_free_tree;
	}
    
	if (!(dir = reiser4_object_open(fs, argv[2], TRUE))) {
		aal_exception_error("Can't open dir %s.", argv[2]);
		goto error_free_root;
	}
    
	{
		object_hint_t dir_hint;
		reiser4_plugin_t *dir_plugin;
	
		dir_hint.plugin = fs->root->entity->plugin;
		dir_hint.statdata = ITEM_STATDATA40_ID;
	
		dir_hint.body.dir.hash = HASH_R5_ID;
		dir_hint.body.dir.direntry = ITEM_CDE40_ID;
	
		{
			int i;
			char name[256];
			reiser4_object_t *object;
	    
			for (i = 0; i < 5000; i++) {
				aal_memset(name, 0, sizeof(name));

				aal_snprintf(name, 256, "testdir%d", i);

				if (!(object = reiser4_object_create(fs, dir, &dir_hint)))
					goto error_free_dir;

				if (reiser4_object_link(dir, object, name)) {
					reiser4_object_close(object);
					goto error_free_dir;
				}

				place = object->place;
				reiser4_object_close(object);
			}
		}
	}
    
	if (reiser4_object_reset(dir)) {
		aal_exception_error("Can't rewind dir %s.", argv[2]);
		goto error_free_dir;
	}
    
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (reiser4_object_readdir(dir, &entry) != 0)
			break;

		reiser4_key_string(&entry.object, buff);

		aal_snprintf(buff + aal_strlen(buff), sizeof(buff),
			     " %s\n", entry.name);

		printf(buff);
	}

	reiser4_object_close(dir);
	reiser4_object_close(fs->root);
	reiser4_tree_fini(fs->tree);
	reiser4_fs_close(fs);
    
	libreiser4_fini();
	aal_device_close(device);
    
	return 0;

 error_free_dir:
	reiser4_object_close(dir);
 error_free_root:
	reiser4_object_close(fs->root);
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

