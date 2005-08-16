/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.c -- backup and rollback fsck changes code. */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <backup.h>

backup_t backup;

static errno_t backup_write(
	aal_device_t *device,	    /* file device, data will be wrote onto */
	void *buff,		    /* buffer, data stored in */
	blk_t block,		    /* start position for writing */
	count_t count)
{
	blk_t blk, end;
	
	end = block + count;
	for (blk = block; blk < end; blk++) {
		/* Check if the block is written already. */
		if (reiser4_bitmap_test(backup.bitmap, blk))
			continue;
		
		/* Mark it in the bitmap. */
		reiser4_bitmap_mark(backup.bitmap, blk);
		
		/* Write block number and data into the backup stream. */
		fwrite(&blk, sizeof(blk), 1, backup.file);
		fwrite(buff, device->blksize, 1, backup.file);
	}
	
	return backup.write(device, buff, block, count);
}

errno_t backup_init(FILE *file, aal_device_t *device, count_t len) {
	aal_memset(&backup, 0, sizeof(backup));
	
	if (!file) return 0;

	aal_assert("vpf-1511", device != NULL);
	
	backup.file = file;
	
	if (!(backup.bitmap = reiser4_bitmap_create(len))) {
		aal_error("Failed to allocate a bitmap for the backup.");
		return -ENOMEM;
	}

	backup.write = device->ops->write;
	device->ops->write = backup_write;

	/* Write header to the file. */
	fwrite(BACKUP_MAGIC, sizeof(BACKUP_MAGIC), 1, backup.file);
	fwrite(&device->blksize, sizeof(device->blksize), 1, backup.file);
	
	return 0;
}

void backup_fini() {
	if (!backup.file) return;

	fclose(backup.file);
	reiser4_bitmap_close(backup.bitmap);
}

errno_t backup_rollback(FILE *file, aal_device_t *device) {
	char buf[sizeof(BACKUP_MAGIC)];
	uint32_t size;
	count_t count;
	void *data;
	blk_t blk;
	
	aal_assert("vpf-1512", file != NULL);
	aal_assert("vpf-1513", device != NULL);

	if ((count = fread(buf, sizeof(BACKUP_MAGIC), 1, file)) != 1) {
		aal_error("Failed to read from the backup file.");
		return -EIO;
	}

	if (aal_strncmp(buf, BACKUP_MAGIC, sizeof(BACKUP_MAGIC))) {
		aal_error("Specified file does not look like a backup one.");
		return -EIO;
	}

	if ((count = fread(&size, sizeof(size), 1, file)) != 1) {
		aal_error("Failed to read from the backup file.");
		return -EIO;
	}

	if (!(data = aal_malloc(size))) {
		aal_error("Failed to alloc the buffer for rollback.");
		return -ENOMEM;
	}
	
	while (1) {
		/* Read from the backup file. */
		count = fread(&blk, sizeof(blk), 1, file);

		if (count != 1) {
			if (feof(file))
				break;
			
			goto error_free_data;
		}
		
		if ((count = fread(data, size, 1, file)) != 1) {
			aal_error("Failed to read from the backup file.");
			goto error_free_data;
		}
		
		/* Write to the device. */
		aal_device_write(device, data, blk, 1);
	}
	
	return 0;
	
 error_free_data:
	aal_error("Failed to read from the backup file.");
	aal_free(data);
	return -EIO;
}

