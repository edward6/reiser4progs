/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40.h -- reiser4 default journal plugin. */

#ifndef JOURNAL40_H
#define JOURNAL40_H

#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include <reiser4/plugin.h>

#define JOURNAL40_BLOCKNR(blksize) \
        (REISER4_MASTER_BLOCKNR(blksize) + 3)

extern reiser4_plug_t journal40_plug;

struct journal40_area {
	blk_t start;
	count_t len;
};

typedef struct journal40_area journal40_area_t;

struct journal40 {
	reiser4_plug_t *plug;

	/* Filesystem blocksize */
	uint32_t blksize;

	/* Journal state (dirty, etc). */
	uint32_t state;
	
	/* Joirnal device */
	aal_device_t *device;

	/* Format instance */
	generic_entity_t *format;

	/* Oid instance */
	generic_entity_t *oid;
	
	/* Area on device, journal may occupie it. */
	journal40_area_t area;
	
	/* Journal header and footer */
	aal_block_t *header;
	aal_block_t *footer;
};

typedef struct journal40 journal40_t;

struct journal40_header {
	d64_t jh_last_commited;
};

typedef struct journal40_header journal40_header_t;

#define get_jh_last_commited(jh)		aal_get_le64(jh, jh_last_commited)
#define set_jh_last_commited(jh, val)		aal_set_le64(jh, jh_last_commited, val)

struct journal40_footer {
	d64_t jf_last_flushed;

	d64_t jf_free_blocks;
	d64_t jf_used_oids;
	d64_t jf_next_oid;
};

typedef struct journal40_footer journal40_footer_t;

#define get_jf_last_flushed(jf)			aal_get_le64(jf, jf_last_flushed)
#define set_jf_last_flushed(jf, val)		aal_set_le64(jf, jf_last_flushed, val)

#define get_jf_free_blocks(jf)			aal_get_le64(jf, jf_free_blocks)
#define set_jf_free_blocks(jf, val)		aal_set_le64(jf, jf_free_blocks, val)

#define get_jf_used_oids(jf)			aal_get_le64(jf, jf_used_oids)
#define set_jf_used_oids(jf, val)		aal_set_le64(jf, jf_used_oids, val)

#define get_jf_next_oid(jf)			aal_get_le64(jf, jf_next_oid)
#define set_jf_next_oid(jf, val)		aal_set_le64(jf, jf_next_oid, val)

#define TXH_MAGIC "TxMagic4"
#define LGR_MAGIC "LogMagc4"

#define TXH_MAGIC_SIZE 8
#define LGR_MAGIC_SIZE 8

struct journal40_tx_header {
	char magic[TXH_MAGIC_SIZE];
    
	d64_t th_id;
	d32_t th_total;
	d32_t th_padding;
	d64_t th_prev_tx;
	d64_t th_next_block;

	d64_t th_free_blocks;
	d64_t th_used_oids;
	d64_t th_next_oid;
};

typedef struct journal40_tx_header journal40_tx_header_t;

#define get_th_id(th)				aal_get_le64(th, th_id)
#define set_th_id(th, val)			aal_set_le64(th, th_id, val)

#define get_th_total(th)			aal_get_le32(th, th_total)
#define set_th_total(th, val)			aal_set_le32(th, th_total, val)

#define get_th_prev_tx(th)			aal_get_le64(th, th_prev_tx)
#define set_th_prev_tx(th, val)			aal_set_le64(th, th_prev_tx, val)

#define get_th_next_block(th)			aal_get_le64(th, th_next_block)
#define set_th_next_block(th, val)		aal_set_le64(th, th_next_block, val)

#define get_th_free_blocks(th)			aal_get_le64(th, th_free_blocks)
#define set_th_free_blocks(th, val)		aal_set_le64(th, th_free_blocks, val)

#define get_th_used_oids(th)			aal_get_le64(th, th_used_oids)
#define set_th_used_oids(th, val)		aal_set_le64(th, th_used_oids, val)

#define get_th_next_oid(th)			aal_get_le64(th, th_next_oid)
#define set_th_next_oid(th, val)		aal_set_le64(th, th_next_oid, val)

struct journal40_lr_header {
	char magic[LGR_MAGIC_SIZE];
	d64_t lh_id;
	d32_t lh_total;
	d32_t lh_serial;
	d64_t lh_next_block;
};

typedef struct journal40_lr_header journal40_lr_header_t;

#define get_lh_id(lh)				aal_get_le64(lh, lh_id)
#define set_lh_id(lh, val)			aal_set_le64(lh, lh_id, val)

#define get_lh_total(lh)			aal_get_le32(lh, lh_total)
#define set_lh_total(lh, val)			aal_set_le32(lh, lh_total, val)

#define get_lh_serial(lh)			aal_get_le32(lh, lh_serial)
#define set_lh_serial(lh, val)			aal_set_le32(lh, lh_serial, val)

#define get_lh_next_block(lh)			aal_get_le64(lh, lh_next_block)
#define set_lh_next_block(lh, val)		aal_set_le64(lh, lh_next_block, val)

struct journal40_lr_entry {
	d64_t le_original;
	d64_t le_wandered;
};

typedef struct journal40_lr_entry journal40_lr_entry_t;

enum journal40_block {
	JB_INV = 0x0,
	JB_TXH = 0x1,
	JB_LGR = 0x2,
	JB_WAN = 0x3,
	JB_ORG = 0x4,
	JB_LST
};

typedef enum journal40_block journal40_block_t;

#define get_le_original(le)			aal_get_le64(le, le_original)
#define set_le_original(le, val)		aal_set_le64(le, le_original, val)

#define get_le_wandered(le)			aal_get_le64(le, le_wandered)
#define set_le_wandered(le, val)		aal_set_le64(le, le_wandered, val)

typedef errno_t (*journal40_txh_func_t)    \
        (generic_entity_t *, blk_t, void *);

typedef errno_t (*journal40_sec_func_t)    \
        (generic_entity_t *, aal_block_t *, \
	 blk_t, journal40_block_t, void *);

typedef errno_t (*journal40_han_func_t)    \
        (generic_entity_t *, aal_block_t *, \
	 blk_t, void *);

#define JFOOTER(block) \
        ((journal40_footer_t *)block->data)

#define JHEADER(block) \
        ((journal40_header_t *)block->data)
#endif

extern errno_t journal40_traverse(journal40_t *journal,
				  journal40_txh_func_t txh_func,
				  journal40_han_func_t han_func,
				  journal40_sec_func_t sec_func,
				  void *data);

extern errno_t journal40_traverse_trans(journal40_t *journal,
					aal_block_t *tx_block,
					journal40_han_func_t han_func,
					journal40_sec_func_t sec_func,
					void *data);

extern aal_device_t *journal40_device(generic_entity_t *entity);

#define journal40_mkdirty(journal) \
	((journal40_t *)journal)->state |= (1 << ENTITY_DIRTY);

#define journal40_mkclean(journal) \
	((journal40_t *)journal)->state &= ~(1 << ENTITY_DIRTY);

#endif
