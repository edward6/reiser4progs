/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * this file contains:
 * declarations of functions implementing file plugin for ordinary file
 */

#if !defined( __REISER4_FILE_H__ )
#define __REISER4_FILE_H__


void get_exclusive_access      (struct inode * inode);
void drop_exclusive_access     (struct inode * inode);
void get_nonexclusive_access   (struct inode * inode);
void drop_nonexclusive_access  (struct inode * inode);
int  tail2extent               (struct inode * inode);
int  extent2tail               (struct file * file);
int  unix_file_readpage_nolock (void * file, struct page * page);
int  find_next_item            (struct sealed_coord *, const reiser4_key *, coord_t *,
				lock_handle *, znode_lock_mode,
				__u32 cbk_flags);
void set_hint                  (struct sealed_coord *, const reiser4_key *,
				const coord_t *);
void unset_hint                (struct sealed_coord * hint);
int  hint_is_set               (const struct sealed_coord * hint);
int  hint_validate             (struct sealed_coord *,
				const reiser4_key *, coord_t *, lock_handle *);
int  coord_set_properly        (const reiser4_key * key, coord_t * coord);




/* plugin->file.*
   required by VFS */
int     unix_file_truncate     (struct inode *, loff_t size);
int     unix_file_readpage     (struct file *, struct page *);
int     unix_file_writepage    (struct page *);
ssize_t unix_file_read         (struct file *, char * buf, size_t size,
				loff_t * off);
ssize_t unix_file_write        (struct file *, const char * buf, size_t size,
				loff_t * off);
int     unix_file_release      (struct file *);
int     unix_file_mmap         (struct file *, struct vm_area_struct * vma);
int     unix_file_get_block    (struct inode *, sector_t block,
				struct buffer_head * bh_result, int create);


int     unix_file_key_by_inode (struct inode *, loff_t off, reiser4_key *);
int     unix_file_create       (struct inode * object, struct inode * parent,
			        reiser4_object_create_data * data);
int     unix_file_owns_item    (const struct inode *, const coord_t *);


/* __REISER4_FILE_H__ */
#endif

/* 
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * scroll-step: 1
 * End:
 */
