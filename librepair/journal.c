/* Copyright 2001-2005 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/journal.c - methods are needed for the work with broken reiser4
   journals. */

#include <repair/librepair.h>
#include <fcntl.h>

/* Callback for journal check method - check if a block, pointed from the 
   journal, is of the special filesystem areas - skipped, block allocator,
   oid alocator, etc. */

static errno_t cb_journal_check(void *object, region_func_t func, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)object;
	
	aal_assert("vpf-737", fs != NULL);
	return reiser4_fs_layout(fs, func, data);
}

/* Checks the opened journal. */
static errno_t repair_journal_check_struct(reiser4_journal_t *journal) {
	aal_assert("vpf-460", journal != NULL);
	aal_assert("vpf-736", journal->fs != NULL);
	
	return reiser4call(journal, check_struct, 
			   cb_journal_check, journal->fs);
}

/* Open the journal and check it. */
errno_t repair_journal_open(reiser4_fs_t *fs, 
			    aal_device_t *journal_device,
			    uint8_t mode, uint32_t options)
{
	reiser4_plug_t *plug;
	errno_t res = 0;
	rid_t pid;
	
	aal_assert("vpf-445", fs != NULL);
	aal_assert("vpf-446", fs->format != NULL);
	aal_assert("vpf-476", journal_device != NULL);
	
	/* Try to open the journal. */
	if ((fs->journal = reiser4_journal_open(fs, journal_device)) == NULL) {
		/* failed to open a journal. Build a new one. */
		aal_error("Failed to open a journal by its id (0x%x).",
			  reiser4_format_journal_pid(fs->format));
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
			aal_error("Invalid journal plugin id has been found.");
			return -EINVAL;
		}
		
		if (!(plug = reiser4_factory_ifind(JOURNAL_PLUG_TYPE, pid)))  {
			aal_error("Cannot find journal plugin by its id 0x%x.",
				  pid);
			return -EINVAL;
		}
		
		if (!(options & (1 << REPAIR_YES))) {
			if (aal_yesno("Do you want to create a new journal "
				      "(%s)?", plug->label) == EXCEPTION_OPT_NO)
			{
				return -EINVAL;
			}
		}
	    
		if (!(fs->journal = reiser4_journal_create(fs, journal_device))) {
			aal_error("Cannot create a journal by its id (0x%x).",
				  reiser4_format_journal_pid(fs->format));
			return -EINVAL;
		}
	} else {    
		/* Check the structure of the opened journal or rebuild it if needed. */
		if ((res = repair_journal_check_struct(fs->journal)))
			goto error_journal_close;
	}
	
	return 0;
	
 error_journal_close:
	reiser4_journal_close(fs->journal);
	fs->journal = NULL;
	
	return res;
}

void repair_journal_invalidate(reiser4_journal_t *journal) {
	aal_assert("vpf-1555", journal != NULL);

	reiser4call(journal, invalidate);
}

void repair_journal_print(reiser4_journal_t *journal, aal_stream_t *stream) {
	aal_assert("umka-1564", journal != NULL);

	reiser4call(journal, print, stream, 0);
}

errno_t repair_journal_pack(reiser4_journal_t *journal, aal_stream_t *stream) {
	rid_t pid;
	
	aal_assert("vpf-1747", journal != NULL);
	aal_assert("vpf-1748", stream != NULL);

	pid = journal->ent->plug->p.id.id;
	aal_stream_write(stream, &pid, sizeof(pid));
	
	return reiser4call(journal, pack, stream);
}

reiser4_journal_t *repair_journal_unpack(reiser4_fs_t *fs, 
					 aal_stream_t *stream) 
{
	reiser4_journal_t *journal;
	reiser4_plug_t *plug;
	uint32_t blksize;
	count_t blocks;
	uint32_t read;
	blk_t start;
	rid_t pid;
	
	aal_assert("vpf-1753", fs != NULL);
	aal_assert("vpf-1754", stream != NULL);

	read = aal_stream_read(stream, &pid, sizeof(pid));
	if (read != sizeof(pid)) {
		aal_error("Can't unpack the journal. Stream is over?");
		return NULL;
	}
	
	/* Getting needed plugin from plugin factory by its id */
	if (!(plug = reiser4_factory_ifind(JOURNAL_PLUG_TYPE, pid))) {
		aal_error("Can't find journal plugin "
			  "by its id 0x%x.", pid);
		return NULL;
	}

	/* Allocating memory and finding plugin */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->fs = fs;
	journal->device = fs->device;
	
	start = reiser4_format_start(fs->format);
	blocks = reiser4_format_get_len(fs->format);
	blksize = reiser4_master_get_blksize(fs->master);

	/* Creating journal entity. */
	if (!(journal->ent = plugcall((reiser4_journal_plug_t *)plug, unpack,
				      fs->device, blksize, fs->format->ent,
				      fs->oid->ent, start, blocks, stream)))
	{
		aal_error("Can't unpack journal %s on %s.",
			  plug->label, fs->device->name);
		goto error;
	}

	return journal;
	
 error:
	aal_free(journal);
	return NULL;
}
