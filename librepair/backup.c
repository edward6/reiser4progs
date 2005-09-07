/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   backup.c -- filesystem backup methods. */

#include <repair/librepair.h>

static errno_t cb_pack(blk_t blk, uint64_t count, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)data;
	aal_stream_t *stream = (aal_stream_t *)fs->data;

	count_t size;
	aal_block_t *block;
	
	/* Open the block to be packed. */
	size = reiser4_master_get_blksize(fs->master);
	
	if (!(block = aal_block_load(fs->device, size, blk))) {
		aal_error("Failed to load a block (%llu) of "
			  "the fs metadata backup.", blk);
		return -EIO;
	}

	aal_stream_write(stream, block->data, block->size);
	aal_block_free(block);

	return 0;
}

errno_t repair_backup_pack(reiser4_fs_t *fs, aal_stream_t *stream) {
	aal_assert("vpf-1411", fs != NULL);
	aal_assert("vpf-1412", stream != NULL);

	fs->data = stream;
	
	return reiser4_backup_layout(fs, cb_pack, fs);
}

static errno_t cb_unpack(blk_t blk, uint64_t count, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)data;
	aal_stream_t *stream = (aal_stream_t *)fs->data;
	
	count_t size;
	aal_block_t *block;

	/* Allocate the block to be unpacked. */
	size = reiser4_master_get_blksize(fs->master);
	
	if (!(block = aal_block_alloc(fs->device, size, blk))) {
		aal_error("Failed to allocate a block (%llu) "
			  "for the fs metadata backup.", blk);
		return -ENOMEM;
	}

	/* Read from the stream to the allocated block and write the block. */
	if (aal_stream_read(stream, block->data, size) != (int64_t)size)
		goto error_free_block;
	
	aal_block_write(block);
	aal_block_free(block);

	return 0;
	
 error_free_block:
	aal_error("Can't unpack the block (%llu). Stream is over?", blk);
	aal_block_free(block);
	return -EIO;
}

errno_t repair_backup_unpack(reiser4_fs_t *fs, aal_stream_t *stream) {
	aal_assert("vpf-1413", fs != NULL);
	aal_assert("vpf-1414", stream != NULL);

	fs->data = stream;
	return reiser4_backup_layout(fs, cb_unpack, fs);
}

static int repair_backup_hint_cmp(backup_hint_t *list,
				  backup_hint_t *hint)
{
	if (list->block.size < hint->block.size)
		return -1;
	
	if (list->block.size > hint->block.size)
		return 1;

	return aal_memcmp(list->block.data, 
			  hint->block.data, 
			  hint->block.size);
}

static backup_hint_t *repair_backup_hint_create(backup_hint_t *hint,
						aal_device_t *device,
						uint32_t size,
						int zero) 
{
	backup_hint_t *newbh;

	if (!hint && !size) 
		return NULL;

	/* Backup block looks correct. Save it. */
	if (!(newbh = aal_calloc(sizeof(*newbh), 0)))
		return NULL;

	if (hint) {
		aal_memcpy(newbh, hint, sizeof(*hint));

		if (aal_block_init(&newbh->block, newbh->block.device,
				   newbh->block.size, newbh->block.nr))
		{
			goto error;
		}

		aal_memcpy(newbh->block.data, 
			   hint->block.data, 
			   hint->block.size);
	} else {
		if (aal_block_init(&newbh->block, 
				   device, size, 0))
		{
			goto error;
		}
	}

	if (zero) {
		aal_block_fill(&newbh->block, 0);
	}
	
	return newbh;
	
 error:
	aal_free(newbh);
	return NULL;
}

static errno_t cb_save_backup(uint64_t blk, uint64_t count, void *data) {
	reiser4_backup_t *backup;
	backup_hint_t *hint;
	backup_hint_t *cur;
	aal_list_t *blocks;
	aal_list_t *at;
	errno_t res;
	int cmp = 1;

	backup = (reiser4_backup_t *)data;
	blocks = (aal_list_t *)backup->data;
	
	backup->hint.block.nr = blk;
	backup->hint.blocks = backup->hint.count = 
		backup->hint.total = 0;
	
	/* Read the pointed block. */
	if (aal_block_read(&backup->hint.block))
		return -EIO;

	/* Check if this is a consistent backup block. */
	if ((res = repair_fs_check_backup(backup->fs->device, &backup->hint)))
		return res < 0 ? res : 0;

	/* Backup block looks correct. Save it. */
	aal_list_foreach_forward(blocks, at) {
		cur = (backup_hint_t *)at->data;
		
		cmp = repair_backup_hint_cmp(cur, &backup->hint);

		/* Such a backup is found already, increment the counter. */
		if (!cmp)
			cur->count++;

		if (cmp >= 0 || !at->next)
			break;
	}

	if (cmp) {
		if (!(hint = repair_backup_hint_create(&backup->hint, 
						       NULL, 0, 0)))
		{
			return -ENOMEM;
		}

		hint->count = 1;

		at = (cmp > 0) ? 
			aal_list_prepend(at, hint) : 
			aal_list_append(at, hint);

		if (!blocks || blocks == at->next)
			blocks = at;
	}
	
	
	backup->data = blocks;
	return 0;
}

static errno_t cb_backup_count(uint64_t blk, uint64_t count, void *data) {
	backup_hint_t *hint = (backup_hint_t *)data;

	hint->total++;
	return 0;
}

static void repair_backup_hint_close(backup_hint_t *hint) {
	aal_block_fini(&hint->block);
	aal_free(hint);
}

static errno_t cb_blocks_free(void *h, void *data) {
	backup_hint_t *hint = (backup_hint_t *)h;
	
	aal_block_fini(&hint->block);
	aal_free(hint);
	return 0;
}

extern errno_t reiser4_backup_layout_body(reiser4_alloc_t *alloc, 
					  uint32_t blksize, uint64_t len, 
					  region_func_t func, void *data);

/* This method reads all possible locations of backup blocks and make a decision
   what block is a correct one and what is a corrupted one. It must work even if
   master or format-specific super block cannot be opened.

   Algorithm. 
   (1) Read all possible locations of backup block for all block allocator 
       plugins and for all block sizes. 
   (2) For every read block, check the consistency: check magics, valid plugin 
       ids, block size, etc.
   (3) Make a list of backups that could be correct backups with counters for 
       every backup version.
   (4) Get the backup version that matches the fs metadata (master sb, etc).
   (5) Make a decision what version is the correct as the maximum of function:
       
       f = [ 66 % * MATCHES / COUNT ] + [ 33 % * META ]
       
       MATCHES - count of matched block contents;
       TOTAL - total block count for this backup type (it depends on allocator 
       plugin, block size);
       META - 1 if matches the fs metadata, 0 if not;

   When the most valuable backup block is choosed and master + format are opened,
   but they do not match each other, another decision if the backup or the 
   current fs metadata is correct is taken:

   +---+---+---+---+
   | 1 | 2 | 3 | 4 |
   +---+---+---+---+
   | + | + | + | - |
   | + | + | - | + |
   | + | - | + | - |
   | + | - | - | - |
   | - | + | + | + |
   | - | + | - | + |
   | - | - | + | + |
   | - | - | - | + |
   +---+---+---+---+

   where columns are:
   (1) Matched backup block count == 1 (fs len is taken from backup).
   (2) Total backup block count == Matched backup block count (--||--).
   (3) Total backup block count == 1 (fs len is taken from the __FORMAT__).
   (4) If backup gets opened.
 */
reiser4_backup_t *repair_backup_open(reiser4_fs_t *fs, uint8_t mode) {
	reiser4_backup_t *backup;
	reiser4_alloc_t alloc;
	reiser4_plug_t *plug;
	
	backup_hint_t *ondisk;
	backup_hint_t *hint;
	backup_hint_t *app;
	aal_list_t *list;
	aal_list_t *at;

	uint32_t blksize;
	uint64_t blocks;
	uint8_t max;
	errno_t res;
	int found;
	rid_t id;
	
	aal_assert("vpf-1728", fs != NULL);

	/* Allocating and initializing the backup. */
	if (!(backup = aal_calloc(sizeof(*backup), 0)))
		return NULL;

	backup->fs = fs;
	backup->data = list = NULL;

	ondisk = NULL;

	for (blksize = REISER4_MIN_BLKSIZE; 
	     blksize <= REISER4_MAX_BLKSIZE;
	     blksize *= 2)
	{
		if (aal_block_init(&backup->hint.block, 
				   fs->device, blksize, 0))
		{
			aal_error("Can't initialize the block.");
			goto error_free_backup;
		}
		
		aal_block_fill(&backup->hint.block, 0);
		
		blocks = repair_format_len_old(fs->device, blksize);
		
		for (id = 0; id < ALLOC_LAST_ID; id++) {
			plug = reiser4_factory_ifind(ALLOC_PLUG_TYPE, id);

			/* Plug must be found. */
			if (!plug) {
				aal_error("Can't find the allocator plugin "
					  "by its id 0x%x.", id);
				goto error_fini_backup;
			}
			
			/* Allocate a block alloc entity. */
			if (!(alloc.ent = plug_call(plug->pl.alloc, 
						    create, fs->device, 
						    blksize, blocks)))
			{
				aal_error("Can't create the allocator '%s' on "
					  "%s.", plug->label, fs->device->name);
				goto error_fini_backup;
			}

			/* Walk backup layout. */
			if (reiser4_backup_layout_body(&alloc, blksize, blocks, 
						       cb_save_backup, backup))
			{
				goto error_free_alloc;
			}

			/* For every new found backup block calculate 
			   the total amount of backup blocks on the device
			   of the lengths kept in the backup. */
			list = backup->data;
			aal_list_foreach_forward(list, at) {
				hint = (backup_hint_t *)at->data;

				/* Has been calculated already. */
				if (hint->total)
					continue;

				/* Walk backup layout. */
				if (reiser4_backup_layout_body(&alloc, blksize,
							       hint->blocks,
							       cb_backup_count,
							       hint))
				{
					goto error_free_alloc;
				}

				/* If we found more backup blocks than the max 
				   possible count of such blocks, descrease 
				   @count to the max value. */
				if (hint->count > hint->total)
					hint->count = hint->total;
			}
			
			/* Close alloc entity. */
			plug_call(plug->pl.alloc, close, alloc.ent);
		}

		aal_block_fini(&backup->hint.block);
	}

	/* If no one valid backup block is found. */
	if (list == NULL)
		goto error_free_backup;

	/* All valid backup blocks are gathered into the list. 
	   Make a decision if possible what is the correct backup block. */
	if (fs->master && fs->format) {
		/* If master & format are opened, create a fresh backup and 
		   compare found backups with the currect fs metadata. */
		
		blocks = reiser4_master_get_blksize(fs->master);
		if (!(ondisk = repair_backup_hint_create(NULL, fs->device, 
							 blocks, 1)))
		{
			aal_error("Can't create a backup hint.");
			goto error_free_backup;
		}

		if (reiser4_fs_backup(fs, ondisk)) {
			aal_error("Can't backup the fs on %s.", 
				  fs->device->name);
			repair_backup_hint_close(ondisk);
			goto error_free_backup;
		}

		/* Check if this is a consistent backup block. */
		if ((res = repair_fs_check_backup(fs->device, ondisk))) {
			if (res < 0) {
				aal_error("Failed to check the backup.");
				goto error_free_ondisk;
			}
			
			repair_backup_hint_close(ondisk);
			ondisk = NULL;
		}

		if (ondisk) {
			blksize = reiser4_master_get_blksize(fs->master);
			id = reiser4_format_alloc_pid(fs->format);
			
			plug = reiser4_factory_ifind(ALLOC_PLUG_TYPE, id);

			/* Plug must be found. */
			if (!plug) {
				aal_error("Can't find the allocator plugin "
					  "by its id 0x%x.", id);
				goto error_free_ondisk;
			}

			/* Allocathe a block alloc entity. */
			if (!(alloc.ent = plug_call(plug->pl.alloc, create,
						    fs->device, blksize,
						    ondisk->blocks)))
			{
				aal_error("Can't create the allocator '%s' on "
					  "%s.", plug->label, fs->device->name);
				goto error_free_ondisk;
			}

			if (reiser4_backup_layout_body(&alloc, blksize, 
						       ondisk->blocks,
						       cb_backup_count, 
						       ondisk))
			{
				goto error_free_alloc;
			}

			plug_call(plug->pl.alloc, close, alloc.ent);
		}
	} 
	
	max = 0;
	app = NULL;
	found = 0;
	aal_list_foreach_forward(list, at) {
		int matched;
		int weight;
		
		hint = (backup_hint_t *)at->data;
		matched = 0;
		
		/* Compare with the ondisk backup if needed. */
		if (ondisk && !found) {
			matched = (repair_backup_hint_cmp(hint, ondisk) ? 
				   0 : 1);
		
			found = matched;
		}
		
		weight = hint->count * 66 / hint->total + matched * 33;

		if (weight > max || (weight == max && matched)) {
			max = weight;
			app = hint;

			/* Set @found to 2 if some previous, not the current 
			   hint, matches @ondisk hint. */
			if (found && !matched)
				found = 2;
		}
	}

	if (mode != RM_BUILD) {
		/* All backup blocks must be found and must match 
		   the ondisk one. */
		if (found != 1) {
			fsck_mess("Found backup does not match to the on-disk "
				  "filesystem metadata.");
			goto error_free_ondisk;
		}
		
		if (app->total != app->count) {
			fsck_mess("Only %llu of %llu backup blocks found.",
				  app->count, app->total);
			goto error_free_ondisk;
		}
	} else {
		/* No backup block is found. */
		if (!app) goto error_free_ondisk;
		
		/* The only 1 backup block is found and it does not match 
		   the @ondisk data */
		if (ondisk && found != 1 && app->count == 1) {
			/* Some more backup blocks must present. */
			if (app->count != app->total)
				goto error_free_ondisk;

			/* The only 1 backup block must present according to 
			   both this backup block and the @ondisk data, take
			   @ondisk as the correct version. */
			if (ondisk->total == 1)
				goto error_free_ondisk;
		}
	}
	
	if (ondisk) {
		repair_backup_hint_close(ondisk);
		ondisk = NULL;
	}

	/* Some backup version is chosen, create a backup on it. */
	aal_memcpy(&backup->hint, app, sizeof(*app));

	if (aal_block_init(&backup->hint.block, fs->device, 
			   backup->hint.block.size, 0))
	{
		aal_error("Can't initialize the backup block.");
		goto error_free_backup;
	}

	aal_memcpy(backup->hint.block.data, app->block.data, 
		   backup->hint.block.size);

	if (app->total != app->count)
		reiser4_backup_mkdirty(backup);

	aal_list_free(list, cb_blocks_free, NULL);
	return backup;

 error_free_alloc:
	plug_call(plug->pl.alloc, close, alloc.ent);
 error_fini_backup:
	if (backup->hint.block.data) 
		aal_block_fini(&backup->hint.block);
 error_free_ondisk:
	if (ondisk) repair_backup_hint_close(ondisk);
 error_free_backup:
	aal_list_free(list, cb_blocks_free, NULL);
	aal_free(backup);
	return NULL;
}

reiser4_backup_t *repair_backup_reopen(reiser4_fs_t *fs) {
	reiser4_backup_t *backup;

	if (!(backup = reiser4_backup_create(fs)))
		return NULL;

	if (!fs->backup)
		return backup;

	if (!repair_backup_hint_cmp(&backup->hint, &fs->backup->hint)) {
		/* Backup matches, nothing has been fixed. */
		reiser4_backup_close(backup);
		return fs->backup;
	}

	/* Backup has been changed. */
	reiser4_backup_close(fs->backup);
	fs->backup = NULL;
	return backup;
}
