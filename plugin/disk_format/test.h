/*
 * Copyright 2002 by Hans Reiser, licensing governed by reiser4/README
 */

#ifndef __DISK_FORMAT_TEST_H__
#define __DISK_FORMAT_TEST_H__

#define TEST_MAGIC "TestLay"

#include "../../dformat.h"
#include "../../key.h"

#include <linux/fs.h> /* for struct super_block  */

/* ondisk super block for test layout */
typedef struct test_disk_super_block {
	char magic [8];
	d64 root_block;    /* root of tree */
	d16 tree_height;

	d16 root_dir_plugin; /* not used */
	d16 root_hash_plugin;/* not used */
	d16 node_plugin;
	d16 tail_policy;
	d16 not_used [3];

	d64 block_count;      /* number of blocks on device */
	d64 next_free_block;  /* smallest free block */

	d64 next_free_oid;    /* smallest oid to allocated */

	d64 root_locality;    /* key of root directory */
	d64 root_objectid;
	
} test_disk_super_block;


/* test layout specific part of reiser4_super_info_data */
typedef struct {
	reiser4_key root_dir_key;
	reiser4_block_nr new_blocknr;
} test_format_super_info;


/* declarations of functions implementing methods of format plugin for
 * test format. The functions theirself are in
 * plugin/disk_format/test.c */
int                 test_format_get_ready    (struct super_block *, void * data);
const reiser4_key * test_format_root_dir_key (const struct super_block *);
int                 test_format_release      (struct super_block *);
void                test_format_print_info   (const struct super_block *);

/* __DISK_FORMAT_TEST_H__ */
#endif

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
