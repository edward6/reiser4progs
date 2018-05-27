SUBDIRS                         = format journal alloc oid node key hash \
                                  sdext item object policy fibre compress \
                                  dst vol

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libreiser4-plugin-minimal.la
else
MINIMAL_LIBS                      = 
endif

noinst_LTLIBRARIES              = libreiser4-plugin.la $(MINIMAL_LIBS)

libreiser4_plugin_la_LIBADD       = format/format40/libformat40-static.la \
                                    format/format41/libformat41-static.la \
                                    journal/journal40/libjournal40-static.la \
                                    alloc/alloc40/liballoc40-static.la \
                                    oid/oid40/liboid40-static.la \
                                    node/node40/libnode40-static.la \
                                    node/node41/libnode41-static.la \
                                    key/key_common/libkey_common-static.la \
                                    key/key_short/libkey_short-static.la \
                                    key/key_large/libkey_large-static.la \
                                    item/nodeptr40/libnodeptr40-static.la \
                                    item/cde40/libcde40-static.la \
                                    item/body40/libbody40-static.la \
                                    item/tail40/libtail40-static.la \
                                    item/plain40/libplain40-static.la \
                                    item/ctail40/libctail40-static.la \
                                    item/extent40/libextent40-static.la \
                                    item/stat40/libstat40-static.la \
                                    item/bbox40/libbbox40-static.la \
                                    object/obj40/libobj40-static.la \
                                    object/dir40/libdir40-static.la \
                                    object/reg40/libreg40-static.la \
                                    object/reg42/libreg42-static.la \
                                    object/sym40/libsym40-static.la \
                                    object/spl40/libspl40-static.la \
                                    object/ccreg40/libccreg40-static.la \
                                    sdext/sdext_unix/libsdext_unix-static.la \
                                    sdext/sdext_lw/libsdext_lw-static.la \
                                    sdext/sdext_lt/libsdext_lt-static.la \
                                    sdext/sdext_symlink/libsdext_symlink-static.la \
                                    sdext/sdext_plug/libsdext_plug-static.la \
                                    sdext/sdext_crypto/libsdext_crypto-static.la \
                                    sdext/sdext_flags/libsdext_flags-static.la \
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
                                    fibre/ext_3_fibre/libext_3_fibre-static.la \
				    compress/libcompress-static.la \
				    vol/libvolume-static.la \
				    dst/libdst-static.la

if ENABLE_MINIMAL
libreiser4_plugin_minimal_la_LIBADD = format/format40/libformat40-minimal.la \
                                    format/format41/libformat41-minimal.la \
                                    oid/oid40/liboid40-minimal.la \
                                    node/node40/libnode40-minimal.la \
                                    node/node41/libnode41-minimal.la \
                                    key/key_common/libkey_common-minimal.la \
                                    key/key_short/libkey_short-minimal.la \
                                    key/key_large/libkey_large-minimal.la \
                                    item/body40/libbody40-minimal.la \
                                    item/nodeptr40/libnodeptr40-minimal.la \
                                    item/cde40/libcde40-minimal.la \
                                    item/tail40/libtail40-minimal.la \
                                    item/plain40/libplain40-minimal.la \
                                    item/ctail40/libctail40-minimal.la \
                                    item/extent40/libextent40-minimal.la \
                                    item/stat40/libstat40-minimal.la \
                                    item/bbox40/libbbox40-minimal.la \
                                    object/obj40/libobj40-minimal.la \
                                    object/dir40/libdir40-minimal.la \
                                    object/reg40/libreg40-minimal.la \
                                    object/sym40/libsym40-minimal.la \
                                    object/spl40/libspl40-minimal.la \
                                    object/ccreg40/libccreg40-minimal.la \
                                    sdext/sdext_unix/libsdext_unix-minimal.la \
                                    sdext/sdext_lw/libsdext_lw-minimal.la \
                                    sdext/sdext_lt/libsdext_lt-minimal.la \
                                    sdext/sdext_symlink/libsdext_symlink-minimal.la \
                                    sdext/sdext_plug/libsdext_plug-minimal.la \
                                    sdext/sdext_flags/libsdext_flags-minimal.la \
                                    policy/tails/libtails-minimal.la \
                                    policy/extents/libextents-minimal.la \
                                    policy/smart/libsmart-minimal.la \
                                    hash/r5_hash/libr5_hash-minimal.la \
                                    hash/fnv1_hash/libfnv1_hash-minimal.la \
                                    hash/rupasov_hash/librupasov_hash-minimal.la \
                                    hash/tea_hash/libtea_hash-minimal.la \
                                    hash/deg_hash/libdeg_hash-minimal.la \
                                    fibre/lexic_fibre/liblexic_fibre-minimal.la \
                                    fibre/dot_o_fibre/libdot_o_fibre-minimal.la \
                                    fibre/ext_1_fibre/libext_1_fibre-minimal.la \
                                    fibre/ext_3_fibre/libext_3_fibre-minimal.la \
				    compress/libcompress-minimal.la \
				    vol/libvolume-minimal.la \
				    dst/libdst-minimal.la

libreiser4_plugin_minimal_la_SOURCES =

endif

libreiser4_plugin_la_SOURCES        =
