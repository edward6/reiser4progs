/*
    journal40.h -- reiser4 default journal plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef JOURNAL40_H
#define JOURNAL40_h

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct journal40 {
    reiser4_plugin_t *plugin;
    reiser4_entity_t *format;
 
    aal_device_t *device;

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
    d64_t jf_nr_files;
    d64_t jf_next_oid;
};

typedef struct journal40_footer journal40_footer_t;

#define get_jf_last_flushed(jf)			aal_get_le64(jf, jf_last_flushed)
#define set_jf_last_flushed(jf, val)		aal_set_le64(jf, jf_last_flushed, val)

#define get_jf_free_blocks(jf)			aal_get_le64(jf, jf_free_blocks)
#define set_jf_free_blocks(jf, val)		aal_set_le64(jf, jf_free_blocks, val)

#define get_jf_nr_files(jf)			aal_get_le64(jf, jf_nr_files)
#define set_jf_nr_files(jf, val)		aal_set_le64(jf, jf_nr_files, val)

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
    d64_t th_nr_files;
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

#define get_th_nr_files(th)			aal_get_le64(th, th_nr_files)
#define set_th_nr_files(th, val)		aal_set_le64(th, th_nr_files, val)

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

#define get_le_original(le)			aal_get_le64(le, le_original)
#define set_le_original(le, val)		aal_set_le64(le, le_original, val)

#define get_le_wandered(le)			aal_get_le64(le, le_wandered)
#define set_le_wandered(le, val)		aal_set_le64(le, le_wandered, val)

#endif

