/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ls.c -- a demo program which works like standard ls utility. */

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
		aal_exception_error("Can't initialize libreiser4.");
		return 0xff;
	}
    
//	misc_param_override("hash=deg_hash");
		
	if (!(device = aal_device_open(&file_ops, argv[1], 
				       512, O_RDWR))) 
	{
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    device->name);
		goto error_free_device;
	}
    
	if (!(fs->tree = reiser4_tree_init(fs, misc_mpressure_detect)))
		goto error_free_fs;
    
	if (!(fs->root = reiser4_object_open(fs->tree, "/", TRUE))) {
		aal_exception_error("Can't open root dir.");
		goto error_free_tree;
	}
    
	if (!(dir = reiser4_object_open(fs->tree, argv[2], TRUE))) {
		aal_exception_error("Can't open dir %s.", argv[2]);
		goto error_free_root;
	}
    
/*	{
		object_hint_t dir_hint;
	
		dir_hint.plug = fs->root->entity->plug;
		dir_hint.statdata = ITEM_STATDATA40_ID;
	
		dir_hint.body.dir.hash = HASH_R5_ID;
		dir_hint.body.dir.direntry = ITEM_CDE40_ID;
	
		{
			int i;
			char name[256];
			reiser4_object_t *object;
	    
			for (i = 0; i < 5000; i++) {
				aal_snprintf(name, 256, "dir%d", i);

				if (!(object = reiser4_object_create(fs->tree,
				                                     dir, &dir_hint)))
				{
				       goto error_free_dir;
				}

				if (reiser4_object_link(dir, object, name)) {
					reiser4_object_close(object);
					goto error_free_dir;
				}

				reiser4_object_close(object);
			}
		}
	}*/
    
	{
		object_hint_t reg_hint;
	
		reg_hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE,
						      OBJECT_REG40_ID);
		
		reg_hint.statdata = ITEM_STATDATA40_ID;

		reg_hint.body.reg.tail = ITEM_TAIL40_ID;
		reg_hint.body.reg.extent = ITEM_EXTENT40_ID;
//		reg_hint.body.reg.policy = TAIL_NEVER_ID;
		reg_hint.body.reg.policy = TAIL_ALWAYS_ID;

		{
			int i, j;
			char name[256];
			reiser4_object_t *object;
	    
			for (i = 0; i < 5000; i++) {
				aal_snprintf(name, 256, "file name%d", i);

				if (!(object = reiser4_object_create(fs->tree, dir,
								     &reg_hint)))
				{
					goto error_free_dir;
				}

				if (reiser4_object_link(dir, object, name))
					continue;

				for (j = 0; j < 1/*2049*/; j++) {
					reiser4_object_write(object, name,
							     aal_strlen(name));
				}
				
				reiser4_object_close(object);
			}
		}
	}
    
	if (reiser4_object_reset(dir)) {
		aal_exception_error("Can't reset directory %s.", argv[2]);
		goto error_free_dir;
	}
    
	while (reiser4_object_readdir(dir, &entry) > 0) {
		aal_snprintf(buff, sizeof(buff), "[%s] %s\n",
			     reiser4_print_key(&entry.object, PO_DEF),
			     entry.name);

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

