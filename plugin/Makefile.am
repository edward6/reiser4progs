SUBDIRS                         = format journal alloc oid node key hash \
                                  sdext item object policy fibre

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libreiser4-plugin-alone.la
else
ALONE_LIBS                      = 
endif

noinst_LTLIBRARIES              = libreiser4-plugin.la $(ALONE_LIBS)

libreiser4_plugin_la_LIBADD       = format/format40/libformat40-static.la \
                                    journal/journal40/libjournal40-static.la \
                                    alloc/alloc40/liballoc40-static.la \
                                    oid/oid40/liboid40-static.la \
                                    node/node40/libnode40-static.la \
                                    key/key_common/libkey_common-static.la \
                                    key/key_short/libkey_short-static.la \
                                    key/key_large/libkey_large-static.la \
                                    item/body40/libbody40-static.la \
                                    item/nodeptr40/libnodeptr40-static.la \
                                    item/cde40/libcde40-static.la \
                                    item/tail40/libtail40-static.la \
                                    item/extent40/libextent40-static.la \
                                    item/stat40/libstat40-static.la \
                                    item/blackbox40/libblackbox40-static.la \
                                    object/obj40/libobj40-static.la \
                                    object/dir40/libdir40-static.la \
                                    object/reg40/libreg40-static.la \
                                    object/sym40/libsym40-static.la \
                                    object/spl40/libspl40-static.la \
                                    sdext/sdext_unix/libsdext_unix-static.la \
                                    sdext/sdext_lw/libsdext_lw-static.la \
                                    sdext/sdext_lt/libsdext_lt-static.la \
                                    sdext/sdext_symlink/libsdext_symlink-static.la \
                                    policy/tails/libtails-static.la \
                                    policy/extents/libextents-static.la \
                                    policy/smart/libsmart-static.la \
                                    hash/r5_hash/libr5_hash-static.la \
                                    hash/fnv1_hash/libfnv1_hash-static.la \
                                    hash/rupasov_hash/librupasov_hash-static.la \
                                    hash/tea_hash/libtea_hash-static.la \
                                    hash/deg_hash/libdeg_hash-static.la \
                                    fibre/lexic_fibre/liblexic_fibre-static.la \
                                    fibre/dot_o_fibre/libdot_o_fibre-static.la \
                                    fibre/ext_1_fibre/libext_1_fibre-static.la \
                                    fibre/ext_3_fibre/libext_3_fibre-static.la

if ENABLE_STAND_ALONE
libreiser4_plugin_alone_la_LIBADD = format/format40/libformat40-alone.la \
                                    oid/oid40/liboid40-alone.la \
                                    node/node40/libnode40-alone.la \
                                    key/key_common/libkey_common-alone.la \
                                    key/key_short/libkey_short-alone.la \
                                    key/key_large/libkey_large-alone.la \
                                    item/body40/libbody40-alone.la \
                                    item/nodeptr40/libnodeptr40-alone.la \
                                    item/cde40/libcde40-alone.la \
                                    item/tail40/libtail40-alone.la \
                                    item/extent40/libextent40-alone.la \
                                    item/stat40/libstat40-alone.la \
                                    item/blackbox40/libstat40-alone.la \
                                    object/obj40/libobj40-alone.la \
                                    object/dir40/libdir40-alone.la \
                                    object/reg40/libreg40-alone.la \
                                    object/sym40/libsym40-alone.la \
                                    object/spl40/libspl40-alone.la \
                                    sdext/sdext_unix/libsdext_unix-alone.la \
                                    sdext/sdext_lw/libsdext_lw-alone.la \
                                    sdext/sdext_lt/libsdext_lt-alone.la \
                                    sdext/sdext_symlink/libsdext_symlink-alone.la \
                                    policy/tails/libtails-alone.la \
                                    policy/extents/libextents-alone.la \
                                    policy/smart/libsmart-alone.la \
                                    hash/r5_hash/libr5_hash-alone.la \
                                    hash/fnv1_hash/libfnv1_hash-alone.la \
                                    hash/rupasov_hash/librupasov_hash-alone.la \
                                    hash/tea_hash/libtea_hash-alone.la \
                                    hash/deg_hash/libdeg_hash-alone.la \
                                    fibre/lexic_fibre/liblexic_fibre-alone.la \
                                    fibre/dot_o_fibre/libdot_o_fibre-alone.la \
                                    fibre/ext_1_fibre/libext_1_fibre-alone.la \
                                    fibre/ext_3_fibre/libext_3_fibre-alone.la


libreiser4_plugin_alone_la_SOURCES =

endif

libreiser4_plugin_la_SOURCES        =
