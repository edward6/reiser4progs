/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   master.c -- master super block functions. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
#include <unistd.h>

bool_t reiser4_master_isdirty(reiser4_master_t *master) {
	aal_assert("umka-2109", master != NULL);
	return master->dirty;
}

void reiser4_master_mkdirty(reiser4_master_t *master) {
	aal_assert("umka-2110", master != NULL);
	master->dirty = 1;
}

void reiser4_master_mkclean(reiser4_master_t *master) {
	aal_assert("umka-2111", master != NULL);
	master->dirty = 0;
}

/* This function checks master super block for validness */
errno_t reiser4_master_valid(reiser4_master_t *master) {
	aal_assert("umka-898", master != NULL);

	if (!aal_pow2(get_ms_blksize(SUPER(master))))
		return -EINVAL;

	return 0;
}

/* Forms master super block disk structure */
reiser4_master_t *reiser4_master_create(
	aal_device_t *device,	    /* device master will be created on */
	uint32_t blksize)	    /* blocksize to be used */
{
	reiser4_master_t *master;
    
	aal_assert("umka-981", device != NULL);
    
	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
    
	/* Setting up magic */
	aal_strncpy(SUPER(master)->ms_magic, REISER4_MASTER_MAGIC,
		    sizeof(SUPER(master)->ms_magic));
    
	/* Setting up block filesystem used */
	set_ms_blksize(SUPER(master), blksize);

	master->dirty = TRUE;
	master->device = device;
	
	return master;
}

#define MASTER_SIGN "MSTR"

errno_t reiser4_master_pack(reiser4_master_t *master,
			    aal_stream_t *stream)
{
	uint32_t size;
	
	aal_assert("umka-2608", master != NULL);
	aal_assert("umka-2609", stream != NULL);

	/* Write magic. */
	aal_stream_write(stream, MASTER_SIGN, 4);

	/* Write data size. */
	size = sizeof(master->ent);
	aal_stream_write(stream, &size, sizeof(size));

	/* Write master data to @stream. */
	aal_stream_write(stream, &master->ent, size);

	return 0;
}

reiser4_master_t *reiser4_master_unpack(aal_device_t *device,
					aal_stream_t *stream)
{
	uint32_t size;
	char sign[5] = {0};
	reiser4_master_t *master;
    
	aal_assert("umka-981", device != NULL);
	aal_assert("umka-2611", stream != NULL);

	/* Check magic first. */
	aal_stream_read(stream, &sign, 4);

	if (aal_strncmp(sign, MASTER_SIGN, 4)) {
		aal_exception_error("Invalid master magic %s is "
				    "detected in stream.",
				    sign);
		return NULL;
	}

	/* Read size and check for validness. */
	aal_stream_read(stream, &size, sizeof(size));

	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
	
	if (size != sizeof(master->ent)) {
		aal_exception_error("Invalid size %u is "
				    "detected in stream.",
				    size);
		goto error_free_master;
	}

	/* Read master data from @stream. */
	aal_stream_read(stream, &master->ent, size);

	master->dirty = TRUE;
	master->device = device;
	
	return master;
	
 error_free_master:
	aal_free(master);
	return NULL;
}

errno_t reiser4_master_print(reiser4_master_t *master,
			     aal_stream_t *stream)
{
	uint32_t blksize;
	
	aal_assert("umka-1568", master != NULL);
	aal_assert("umka-1569", stream != NULL);

	blksize = reiser4_master_get_blksize(master);
	
	aal_stream_format(stream, "Master super block (%lu):\n",
			  REISER4_MASTER_OFFSET / blksize);
	
	aal_stream_format(stream, "magic:\t\t%s\n",
			  reiser4_master_get_magic(master));
	
	aal_stream_format(stream, "blksize:\t%u\n",
			  reiser4_master_get_blksize(master));

	aal_stream_format(stream, "format plug id:\t%x\n",
			  reiser4_master_get_format(master));

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*master->ent.ms_uuid != '\0') {
		char uuid[256];
		
		uuid_unparse(reiser4_master_get_uuid(master), uuid);
		aal_stream_format(stream, "uuid:\t\t%s\n", uuid);
	} else {
		aal_stream_format(stream, "uuid:\t\t<none>\n");
	}
#endif
	
	if (*master->ent.ms_label != '\0') {
		aal_stream_format(stream, "label:\t\t%s\n",
				  reiser4_master_get_label(master));
	} else {
		aal_stream_format(stream, "label:\t\t<none>\n");
	}


	return 0;
}

errno_t reiser4_master_layout(reiser4_master_t *master, 
			      region_func_t region_func,
			      void *data)
{
	uint32_t blk;
	uint32_t blksize;
	
	aal_assert("vpf-1317", master != NULL);
	aal_assert("vpf-1317", region_func != NULL);

	blksize = reiser4_master_get_blksize(master);
	blk = REISER4_MASTER_OFFSET / blksize;
	return region_func(master, blk, 1, data);
}

/* Callback function for comparing plugins */
static errno_t callback_guess_format(
	reiser4_plug_t *plug,        /* plugin to be checked */
	void *data)		     /* needed plugin type */
{
	if (plug->id.type == FORMAT_PLUG_TYPE) {
		generic_entity_t *entity;
		
		if ((entity = plug_call(plug->o.format_ops,
					open, (aal_device_t *)data,
					sysconf(_SC_PAGESIZE))))
		{
			plug_call(plug->o.format_ops, close,
				  entity);
			
			return 1;
		}
	}
    
	return 0;
}

reiser4_plug_t *reiser4_master_guess(aal_device_t *device) {
	/* Calls factory_cfind() (custom find) method in order to find
	   convenient plugin with guess_format() callback function. */
	return reiser4_factory_cfind(callback_guess_format, device);
}
#endif

/* Reads master super block from disk */
reiser4_master_t *reiser4_master_open(aal_device_t *device) {
	aal_block_t *block;
	reiser4_master_t *master;
    
	aal_assert("umka-143", device != NULL);
    
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;

	master->dirty = FALSE;
	master->device = device;
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(device, device->blksize,
				     REISER4_MASTER_OFFSET /
				     device->blksize)))
	{
		aal_exception_fatal("Can't read master super block.");
		goto error_free_master;
	}

	/* Copying master super block */
	aal_memcpy(SUPER(master), block->data, sizeof(*SUPER(master)));

	aal_block_free(block);
    
	/* Reiser4 master super block is not found on the device. */
	if (aal_strncmp(SUPER(master)->ms_magic, REISER4_MASTER_MAGIC,
			aal_strlen(REISER4_MASTER_MAGIC)) != 0)
	{
		aal_exception_fatal("Wrong magic is found in the master "
				    "super block.");
		goto error_free_master;
	}
    
	return master;
    
 error_free_master:
	aal_free(master);
	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Rereads master super block from the device */
errno_t reiser4_master_reopen(reiser4_master_t *master) {
	blk_t offset;
	uint32_t blksize;
	aal_block_t *block;
	
	aal_assert("umka-1576", master != NULL);

	blksize = master->device->blksize;
	offset = (blk_t)(REISER4_MASTER_OFFSET / blksize);
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(master->device,
				     blksize, offset)))
	{
		aal_exception_fatal("Can't read master super block "
				    "at %llu.", offset);
		return -EIO;
	}

	/* Copying master super block */
	aal_memcpy(SUPER(master), block->data,
		   sizeof(*SUPER(master)));
	
	aal_block_free(block);
	
	return 0;
}

/* Saves master super block to device. */
errno_t reiser4_master_sync(
	reiser4_master_t *master)	    /* master to be saved */
{
	errno_t res;
	blk_t offset;
	uint32_t blksize;
	aal_block_t *block;
	
	aal_assert("umka-145", master != NULL);
    
	if (!master->dirty)
		return 0;
	
	blksize = get_ms_blksize(SUPER(master));
	offset = REISER4_MASTER_OFFSET / blksize;

	if (!(block = aal_block_alloc(master->device,
				      blksize, offset)))
	{
		return -ENOMEM;
	}

	aal_block_fill(block, 0);

	aal_memcpy(block->data, SUPER(master),
		   sizeof(*SUPER(master)));
	
	/* Writing master super block to its device */
	if ((res = aal_block_write(block))) {
		aal_exception_error("Can't write master super block "
				    "at %llu. %s.", block->nr,
				    block->device->error);
		goto error_free_block;
	}

	master->dirty = FALSE;

 error_free_block:
	aal_block_free(block);
	return res;
}
#endif

/* Frees master super block occupied memory */
void reiser4_master_close(reiser4_master_t *master) {
	aal_assert("umka-1506", master != NULL);
	aal_free(master);
}

rid_t reiser4_master_get_format(reiser4_master_t *master) {
	aal_assert("umka-982", master != NULL);
	return get_ms_format(SUPER(master));
}

uint32_t reiser4_master_get_blksize(reiser4_master_t *master) {
	aal_assert("umka-983", master != NULL);
	return get_ms_blksize(SUPER(master));
}

#ifndef ENABLE_STAND_ALONE
char *reiser4_master_get_magic(reiser4_master_t *master) {
	aal_assert("umka-982", master != NULL);
	return SUPER(master)->ms_magic;
}

char *reiser4_master_get_uuid(reiser4_master_t *master) {
	aal_assert("umka-984", master != NULL);
	return SUPER(master)->ms_uuid;
}

char *reiser4_master_get_label(reiser4_master_t *master) {
	aal_assert("umka-985", master != NULL);
	return SUPER(master)->ms_label;
}

void reiser4_master_set_format(reiser4_master_t *master,
			       rid_t format)
{
	aal_assert("umka-2496", master != NULL);
	set_ms_format(SUPER(master), format);
}

void reiser4_master_set_blksize(reiser4_master_t *master,
				uint32_t blksize)
{
	aal_assert("umka-2497", master != NULL);
	set_ms_blksize(SUPER(master), blksize);
}

void reiser4_master_set_uuid(reiser4_master_t *master,
			     char *uuid)
{
	aal_assert("umka-2498", master != NULL);

	if (uuid) {
		aal_strncpy(SUPER(master)->ms_uuid, uuid,
			    sizeof(SUPER(master)->ms_uuid));
	} else {
		aal_memset(SUPER(master)->ms_uuid, 0,
			   sizeof(SUPER(master)->ms_uuid));
	}
}

void reiser4_master_set_label(reiser4_master_t *master,
			      char *label)
{
	aal_assert("umka-2500", master != NULL);

	if (label) {
		aal_strncpy(SUPER(master)->ms_label, label,
			    sizeof(SUPER(master)->ms_label));
	} else {
		aal_memset(SUPER(master)->ms_label, 0,
			   sizeof(SUPER(master)->ms_label));
	}
}
#endif
