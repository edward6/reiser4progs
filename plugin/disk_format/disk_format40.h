/*
 * Copyright 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * this file contains:
 * - definition of ondisk super block of standart disk layout for
 *   reiser 4.0 (layout 40)
 * - definition of layout 40 specific portion of in-core super block
 * - declarations of functions implementing methods of layout plugin
 *   for layout 40
 * - declarations of functions used to get/set fields in layout 40 super block
 */

#ifndef __DISK_FORMAT40_H__
#define __DISK_FORMAT40_H__

/* magic for default reiser4 layout */
#define FORMAT40_MAGIC "R4Sb-Default"
#define FORMAT40_OFFSET (65536 + 4096)

#include "../../dformat.h"

#include <linux/fs.h> /* for struct super_block  */

/* ondisk super block for format 40. It is 512 bytes long */
typedef struct format40_disk_super_block {
	/*   0 */ d32 mkfs_id;     /* unique identifier of fs */
	/*   4 */ d64 block_count; /* number of block in a filesystem */
	/*  12 */ d64 free_blocks; /* number of free blocks */
	/*  20 */ d64 root_block;  /* filesystem tree root block */
	/*  28 */ d64 oid;	   /* smallest free objectid */
	/*  36 */ d64 file_count;  /* number of files in a filesystem */
	/*  44 */ d64 flushes;	   /* number of times super block was
				    * flushed. Needed if format 40
				    * will have few super blocks */
	/*  52 */ char magic[16];  /* magic string R4Sb-Default */
	/*  68 */ d16 tree_height; /* height of filesystem tree */
	/*  70 */ d16 tail_policy;
	/*  72 */ char not_used [440];
} format40_disk_super_block;


/* format 40 specific part of reiser4_super_info_data */
typedef struct format40_super_info {
	format40_disk_super_block actual_sb;
	jnode * sb_jnode;
} format40_super_info;

#define FORMAT40_JOURNAL_HEADER_BLOCKNR 19
#define FORMAT40_JOURNAL_FOOTER_BLOCKNR 20

/* declarations of functions implementing methods of layout plugin for
 * format 40. The functions theirself are in disk_format40.c */
int                 format40_get_ready    (struct super_block *, void * data);
const reiser4_key * format40_root_dir_key (const struct super_block *);
int                 format40_release      (struct super_block * s);
jnode             * format40_log_super    (struct super_block * s);
void                format40_print_info   (const struct super_block * s);

/* __DISK_FORMAT40_H__ */
#endif


