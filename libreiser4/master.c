/*
  master.c -- master super block functions.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

#define SUPER(master) ((reiser4_master_sb_t *)master->block->data)

#ifndef ENABLE_COMPACT

/* Forms master super block disk structure */
reiser4_master_t *reiser4_master_create(
	aal_device_t *device,	    /* device master will be created on */
	rpid_t format_pid,	    /* disk format plugin id to be used */
	uint32_t blocksize,	    /* blocksize to be used */
	const char *uuid,	    /* uuid to be used */
	const char *label)	    /* filesystem label to be used */
{
	blk_t offset;
	reiser4_master_t *master;
    
	aal_assert("umka-981", device != NULL);
    
	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
    
	offset = MASTER_OFFSET / blocksize;
    
	if (!(master->block = aal_block_create(device, offset, 0)))
		goto error_free_master;
    
	/* Setting up magic */
	aal_strncpy(SUPER(master)->ms_magic, MASTER_MAGIC,
		    aal_strlen(MASTER_MAGIC));
    
	/* Setting up uuid and label */
	if (uuid) {
		aal_strncpy(SUPER(master)->ms_uuid, uuid, 
			    sizeof(SUPER(master)->ms_uuid));
	}
    
	if (label) {
		aal_strncpy(SUPER(master)->ms_label, label, 
			    sizeof(SUPER(master)->ms_label));
	}
    
	/* Setting up plugin id for used disk format plugin */
	set_ms_format(SUPER(master), format_pid);

	/* Setting up block filesystem used */
	set_ms_blocksize(SUPER(master), blocksize);

	master->native = TRUE;
	return master;
    
 error_free_master:
	aal_free(master);
	return NULL;
}

/* This function checks master super block for validness */
errno_t reiser4_master_valid(reiser4_master_t *master) {
	aal_assert("umka-898", master != NULL);
	return -(!aal_pow_of_two(get_ms_blocksize(SUPER(master))));
}

/* Callback function for comparing plugins */
static errno_t callback_guess_format(
	reiser4_plugin_t *plugin,    /* plugin to be checked */
	void *data)		     /* needed plugin type */
{
	if (plugin->h.type == FORMAT_PLUGIN_TYPE) {
		aal_device_t *device = (aal_device_t *)data;
		return plugin_call(plugin->format_ops, confirm, device);
	}
    
	return 0;
}

reiser4_plugin_t *reiser4_master_guess(aal_device_t *device) {
	return libreiser4_factory_cfind(callback_guess_format, device);
}

errno_t reiser4_master_print(reiser4_master_t *master,
			     aal_stream_t *stream)
{
	aal_assert("umka-1568", master != NULL);
	aal_assert("umka-1569", stream != NULL);
	
	aal_stream_format(stream, "offset:\t\t%llu\n",
			  aal_block_number(master->block));
	
	aal_stream_format(stream, "blocksize:\t%u\n",
			  reiser4_master_blocksize(master));

	aal_stream_format(stream, "magic:\t\t%s\n",
			  reiser4_master_magic(master));
	
	aal_stream_format(stream, "format:\t\t%x\n",
			  reiser4_master_format(master));

	aal_stream_format(stream, "label:\t\t%s\n",
			  reiser4_master_label(master));

	return 0;
}

#endif

/* Makes probing of reiser4 master super block on given device */
int reiser4_master_confirm(aal_device_t *device) {
	blk_t offset;
	aal_block_t *block;
	reiser4_master_sb_t *super;
    
	aal_assert("umka-901", device != NULL);
    
	offset = (blk_t)(MASTER_OFFSET / BLOCKSIZE);

	/* Setting up default block size (4096) to used device */
	aal_device_set_bs(device, BLOCKSIZE);
    
	/* Reading the block where master super block lies */
	if (!(block = aal_block_open(device, offset))) {
		aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
				    "Can't read master super block "
				    "at %llu.", offset);
		return 0;
	}
    
	super = (reiser4_master_sb_t *)block->data;

	if (aal_strncmp(super->ms_magic, MASTER_MAGIC, 4) == 0) {
		uint32_t blocksize = get_ms_blocksize(super);
			
		if (aal_device_set_bs(device, blocksize)) {
			aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
					    "Invalid block size detected %u.",
					    blocksize);
			goto error_free_block;
		}
	
		aal_block_close(block);
		return 1;
	}
    
	aal_block_close(block);
	return 0;
    
 error_free_block:
	aal_block_close(block);
	return 0;
}

/* Reads master super block from disk */
reiser4_master_t *reiser4_master_open(aal_device_t *device) {
	blk_t offset;
	reiser4_master_t *master;
    
	aal_assert("umka-143", device != NULL);
    
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
    
	offset = (blk_t)(MASTER_OFFSET / BLOCKSIZE);

	/* Setting up default block size (4096) to used device */
	aal_device_set_bs(device, BLOCKSIZE);
    
	/* Reading the block where master super block lies */
	if (!(master->block = aal_block_open(device, offset))) {
		aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
				    "Can't read master super block "
				    "at %llu.", offset);
		goto error_free_master;
	}
    
	/*
	  Checking if master super block can be counted as the reiser4 super
	  block by mean of checking its magic. If it is not reiser4 master super
	  block, then we trying guess format in use by means of traversing all
	  format plugins and call its confirm method.
	*/
	if (aal_strncmp(SUPER(master)->ms_magic, MASTER_MAGIC, 4) != 0) {
		/* 
		   Reiser4 doesn't found on passed device. In this point we
		   should call the function which detectes used format on the
		   device.
		*/
#ifndef ENABLE_COMPACT
		{
			reiser4_plugin_t *plugin;
	    
			if (!(plugin = reiser4_master_guess(device)))
				goto error_free_block;
	    
			/* Creating in-memory master super block */
			if (!(master = reiser4_master_create(device, plugin->h.id, 
							     BLOCKSIZE, NULL, NULL)))
			{
				aal_exception_error("Can't find format in use after "
						    "probe the all registered format "
						    "plugins.");
				goto error_free_block;
			}
	    
			master->native = FALSE;
			return master;
		}
#endif
		aal_exception_error("Can't find reiser4 filesystem on %s.",
				    device->name);
		goto error_free_block;
	}
    
	return master;
    
 error_free_block:
	aal_block_close(master->block);
 error_free_master:
	aal_free(master);
	return NULL;
}

reiser4_master_t *reiser4_master_reopen(reiser4_master_t *master) {
	aal_device_t *device;
	
	aal_assert("umka-1576", master != NULL);

	device = master->block->device;
	reiser4_master_close(master);

	return reiser4_master_open(device);
}

#ifndef ENABLE_COMPACT

/* Saves master super block to device. */
errno_t reiser4_master_sync(
	reiser4_master_t *master)	    /* master to be saved */
{
	aal_assert("umka-145", master != NULL);
    
	/* The check if opened filesystem is native reiser4 one */
	if (!master->native)
		return 0;
    
	/* Writing master super block to its device */
	if (aal_block_sync(master->block)) {
		aal_exception_error("Can't synchronize master super "
				    "block at %llu. %s.", aal_block_number(master->block), 
				    aal_device_error(master->block->device));
		return -1;
	}

	return 0;
}

#endif

/* Frees master super block occupied memory */
void reiser4_master_close(reiser4_master_t *master) {
	aal_assert("umka-1506", master != NULL);
	
	aal_block_close(master->block);
	aal_free(master);
}

char *reiser4_master_magic(reiser4_master_t *master) {
	aal_assert("umka-982", master != NULL);
	return SUPER(master)->ms_magic;
}

rpid_t reiser4_master_format(reiser4_master_t *master) {
	aal_assert("umka-982", master != NULL);
	return get_ms_format(SUPER(master));
}

uint32_t reiser4_master_blocksize(reiser4_master_t *master) {
	aal_assert("umka-983", master != NULL);
	return get_ms_blocksize(SUPER(master));
}

char *reiser4_master_uuid(reiser4_master_t *master) {
	aal_assert("umka-984", master != NULL);
	return SUPER(master)->ms_uuid;
}

char *reiser4_master_label(reiser4_master_t *master) {
	aal_assert("umka-985", master != NULL);
	return SUPER(master)->ms_label;
}
