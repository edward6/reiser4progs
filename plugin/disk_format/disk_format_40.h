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

/* magic for default reiser4 layout */
#define FORMAT_40_MAGIC "R4Sb-Default"
#define FORMAT_40_OFFSET (65536 + 4096)

/* ondisk super block for format 40. It is 512 bytes long */
typedef struct format_40_disk_super_block {	
	/*   0 */ d64 block_count; /* number of block in a filesystem */
	/*   8 */ d64 free_blocks; /* number of free blocks */
	/*  16 */ d64 root_block;  /* filesystem tree root block */
	/*  32 */ d64 oid;	   /* smallest free objectid */
	/*  40 */ d64 file_count;  /* number of files in a filesystem */
	/*  48 */ d64 flushes;	   /* number of times super block was
				    * flushed. Needed if format 40
				    * will have few super blocks */
	/*  56 */ char magic[16];  /* magic string R4Sb-Default */
	/*  72 */ d16 tree_height; /* height of filesystem tree */
	
	/*  74 */ d16 journal_plugin_id; /* journal plugin identifier */
	/*  76 */ d16 alloc_plugin_id;	 /* space allocator plugin identifier */
	/*  78 */ d16 oid_plugin_id;	 /* oid allocator plugin identifier */
	
	/*  80 */ d16 padd [3];
	/*  86 */ char not_used [426];
} format_40_disk_super_block;


/* format 40 specific part of reiser4_super_info_data */
typedef struct format_40_super_info {
	format_40_disk_super_block actual_sb;
} format_40_super_info;


/* declarations of functions implementing methods of layout plugin for
 * format 40. The functions theirself are in disk_format_40.c */
int                 format_40_get_ready    (struct super_block *, void * data);
const reiser4_key * format_40_root_dir_key (const struct super_block *);

