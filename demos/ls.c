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
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_place_t place;

	reiser4_file_t *dir;
	reiser4_entry_hint_t entry;

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
		BLOCKSIZE, O_RDWR))) 
	{
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, progs_profile_find("smart40")))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    aal_device_name(device));
		goto error_free_device;
	}
    
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_fs;
    
	if (!(fs->root = reiser4_file_open(fs, "/"))) {
		aal_exception_error("Can't open root dir.");
		goto error_free_tree;
	}
    
	if (!(dir = reiser4_file_open(fs, argv[2]))) {
		aal_exception_error("Can't open dir %s.", argv[2]);
		goto error_free_root;
	}
    
	{
		reiser4_plugin_t *dir_plugin;
		reiser4_file_hint_t dir_hint;
	
		dir_hint.plugin = fs->root->entity->plugin;
		dir_hint.statdata = ITEM_STATDATA40_ID;
	
		dir_hint.body.dir.hash = HASH_R5_ID;
		dir_hint.body.dir.direntry = ITEM_CDE40_ID;
	
		{
			int i;
			char name[256];
			reiser4_file_t *file;
	    
			for (i = 0; i < 5000; i++) {
				aal_memset(name, 0, sizeof(name));

				aal_snprintf(name, 256, "%s/testdir%d",
					     dir->name, i);

				if (!(file = reiser4_file_create(fs, name, &dir_hint)))
					goto error_free_dir;

				place = file->place;
				reiser4_file_close(file);
			}
		}
	}
    
//	reiser4_file_remove(dir, "testdir0");
	
	if (reiser4_file_reset(dir)) {
		aal_exception_error("Can't rewind dir %s.", argv[2]);
		goto error_free_dir;
	}
    
	while (reiser4_file_read(dir, (void *)&entry, 1)) {
		aal_stream_t stream;
		aal_stream_init(&stream);
		
		reiser4_key_print(&entry.object, &stream);
		aal_stream_format(&stream, " %s\n", entry.name);
		printf((char *)stream.data);
		
		aal_stream_fini(&stream);
	}

	if (reiser4_file_reset(dir)) {
		aal_exception_error("Can't rewind dir %s.", argv[2]);
		goto error_free_dir;
	}

/*	place.pos.item = 0;

	{
		int i = 0;
		while (place.node && reiser4_node_items(place.node) > 0) {
			reiser4_tree_remove(fs->tree, &place, 1);
			i++;
		}
	}*/
	
//	reiser4_tree_detach(fs->tree, place.node);
	
	reiser4_file_close(dir);
//        reiser4_fs_sync(fs);

	reiser4_file_close(fs->root);
	reiser4_tree_close(fs->tree);
	
	reiser4_fs_close(fs);
    
	libreiser4_done();
	aal_device_close(device);
    
	return 0;

 error_free_dir:
	reiser4_file_close(dir);
 error_free_root:
	reiser4_file_close(fs->root);
 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_done();
	return 0xff;
}

