/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   master.c -- master super block functions. */

#include <aux/aux.h>
#include <reiser4/libreiser4.h>

#ifndef ENABLE_MINIMAL
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
reiser4_master_t *reiser4_master_create(aal_device_t *device, fs_hint_t *hint) {
	reiser4_master_t *master;
    
	aal_assert("umka-981", device != NULL);
    
	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
    
	/* Setting up magic. */
	aal_strncpy(SUPER(master)->ms_magic, REISER4_MASTER_MAGIC,
		    sizeof(REISER4_MASTER_MAGIC));
    
	/* set block size */
	set_ms_blksize(SUPER(master), hint->blksize);
	reiser4_master_set_volume_uuid(master, hint->volume_uuid);
	reiser4_master_set_subvol_uuid(master, hint->subvol_uuid);
	reiser4_master_set_label(master, hint->label);

	master->dirty = 1;
	master->device = device;
	
	return master;
}

errno_t reiser4_master_backup(reiser4_master_t *master,
			      backup_hint_t *hint)
{
	aal_assert("vpf-1388", master != NULL);
	aal_assert("vpf-1389", hint != NULL);
	
	aal_memcpy(hint->block.data + hint->off[BK_MASTER], 
		   &master->ent, sizeof(master->ent));
	
	hint->off[BK_MASTER + 1] = hint->off[BK_MASTER] + sizeof(master->ent);
	
	/* Reserve 8 bytes. */
	aal_memset(hint->block.data + hint->off[BK_MASTER + 1], 0, 8);
	hint->off[BK_MASTER + 1] += 8;
	
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

	blksize = get_ms_blksize(SUPER(master));
	blk = REISER4_MASTER_BLOCKNR(blksize);
	return region_func(blk, 1, data);
}

/* Callback function for comparing plugins */
static errno_t cb_guess_format(
	reiser4_plug_t *plug,		/* plugin to be checked */
	void *data)			/* needed plugin type */
{
	if (plug->id.type == FORMAT_PLUG_TYPE) {
		reiser4_format_ent_t *entity;
		aal_device_t *device;
		uint32_t blksize;

		device = (aal_device_t *)data;
		blksize = sysconf(_SC_PAGESIZE);
		
		if ((entity = plugcall((reiser4_format_plug_t *)plug, 
				       open, device, blksize))) 
		{
			plugcall((reiser4_format_plug_t *)plug, close, entity);
			return 1;
		}
	}
    
	return 0;
}

reiser4_plug_t *reiser4_master_guess(aal_device_t *device) {
	/* Calls factory_cfind() (custom find) method in order to find
	   convenient plugin with guess_format() callback function. */
	return reiser4_factory_cfind(cb_guess_format, device);
}
#endif

/* Reads master super block from disk */
reiser4_master_t *reiser4_master_open(aal_device_t *device) {
	aal_block_t *block;
	reiser4_master_t *master;
    
	aal_assert("umka-143", device != NULL);
    
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;

	master->dirty = 0;
	master->device = device;
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(device, device->blksize,
				     REISER4_MASTER_BLOCKNR(device->blksize))))
	{
		aal_fatal("Can't read master super block.");
		goto error_free_master;
	}

	/* Copying master super block */
	aal_memcpy(SUPER(master), block->data,
		   sizeof(*SUPER(master)));

	aal_block_free(block);
    
	/* Reiser4 master super block is not found on the device. */
	if (aal_strncmp(SUPER(master)->ms_magic, REISER4_MASTER_MAGIC,
			sizeof(REISER4_MASTER_MAGIC)) != 0)
	{
		aal_fatal("Wrong magic found in the master "
			  "super block.");
		goto error_free_master;
	}
    
	return master;
    
 error_free_master:
	aal_free(master);
	return NULL;
}

#ifndef ENABLE_MINIMAL
/* Rereads master super block from the device */
errno_t reiser4_master_reopen(reiser4_master_t *master) {
	blk_t offset;
	uint32_t blksize;
	aal_block_t *block;
	
	aal_assert("umka-1576", master != NULL);

	blksize = master->device->blksize;
	offset = (blk_t)(REISER4_MASTER_BLOCKNR(blksize));
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(master->device,
				     blksize, offset)))
	{
		aal_fatal("Can't read master super block "
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
errno_t reiser4_master_sync(reiser4_master_t *master) {
	errno_t res;
	blk_t offset;
	uint32_t blksize;
	aal_block_t *block;
	
	aal_assert("umka-145", master != NULL);
    
	if (!master->dirty)
		return 0;
	
	blksize = get_ms_blksize(SUPER(master));
	offset = REISER4_MASTER_BLOCKNR(blksize);

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
		aal_error("Can't write master super block "
			  "at %llu. %s.", block->nr,
			  block->device->error);
		goto error_free_block;
	}

	master->dirty = 0;

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

#ifndef ENABLE_MINIMAL
char *reiser4_master_get_magic(reiser4_master_t *master) {
	aal_assert("umka-982", master != NULL);
	return SUPER(master)->ms_magic;
}

char *reiser4_master_get_volume_uuid(reiser4_master_t *master) {
	aal_assert("umka-984", master != NULL);
	return SUPER(master)->ms_vol_uuid;
}

char *reiser4_master_get_subvol_uuid(reiser4_master_t *master) {
	aal_assert("edward-xxx", master != NULL);
	return SUPER(master)->ms_sub_uuid;
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
	master->dirty = 1;
}

void reiser4_master_set_blksize(reiser4_master_t *master,
				uint32_t blksize)
{
	aal_assert("umka-2497", master != NULL);
	set_ms_blksize(SUPER(master), blksize);
	master->dirty = 1;
}

void reiser4_master_set_volume_uuid(reiser4_master_t *master,
				    char *uuid)
{
	aal_assert("umka-2498", master != NULL);

	aal_memset(SUPER(master)->ms_vol_uuid, 0,
		   sizeof(SUPER(master)->ms_vol_uuid));
	
	if (uuid) {
		aal_strncpy(SUPER(master)->ms_vol_uuid, uuid,
			    sizeof(SUPER(master)->ms_vol_uuid));
	} 
	master->dirty = 1;
}

void reiser4_master_set_subvol_uuid(reiser4_master_t *master,
				    char *uuid)
{
	aal_assert("edward-xxx", master != NULL);

	aal_memset(SUPER(master)->ms_sub_uuid, 0,
		   sizeof(SUPER(master)->ms_sub_uuid));

	if (uuid) {
		aal_strncpy(SUPER(master)->ms_sub_uuid, uuid,
			    sizeof(SUPER(master)->ms_vol_uuid));
	}
	master->dirty = 1;
}

void reiser4_master_set_label(reiser4_master_t *master,
			      char *label)
{
	aal_assert("umka-2500", master != NULL);

	aal_memset(SUPER(master)->ms_label, 0,
		   sizeof(SUPER(master)->ms_label));
	
	if (label) {
		aal_strncpy(SUPER(master)->ms_label, label,
			    sizeof(SUPER(master)->ms_label));
	}
	master->dirty = 1;
}

#endif
