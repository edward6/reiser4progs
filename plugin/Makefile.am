SUBDIRS 			= format journal alloc oid node key hash sdext item object

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libreiser4-plugin-alone.la
else
ALONE_LIBS                      = 
endif

noinst_LTLIBRARIES	     	= libreiser4-plugin.la $(ALONE_LIBS)

if ENABLE_MONOLITHIC
libreiser4_plugin_la_LIBADD  	= format/format40/libformat40-static.la \
			          journal/journal40/libjournal40-static.la \
			          alloc/alloc40/liballoc40-static.la \
			          oid/oid40/liboid40-static.la \
			          node/node40/libnode40-static.la \
			          key/key40/libkey40-static.la \
			          item/nodeptr40/libnodeptr40-static.la \
			          item/direntry40/libdirentry40-static.la \
			          item/tail40/libtail40-static.la \
			          item/extent40/libextent40-static.la \
			          item/stat40/libstat40-static.la \
			          object/obj40/libobj40-static.la \
			          object/dir40/libdir40-static.la \
			          object/reg40/libreg40-static.la \
				  object/sym40/libsym40-static.la \
			          sdext/sdext_unix/libsdext_unix-static.la \
			          sdext/sdext_lw/libsdext_lw-static.la \
			          sdext/sdext_lt/libsdext_lt-static.la \
			          sdext/sdext_symlink/libsdext_symlink-static.la \
				  hash/r5_hash/libr5_hash-static.la \
				  hash/fnv1_hash/libfnv1_hash-static.la \
                                  hash/rupasov_hash/librupasov_hash-static.la \
                                  hash/tea_hash/libtea_hash-static.la
endif

if ENABLE_STAND_ALONE
libreiser4_plugin_alone_la_LIBADD = format/format40/libformat40-alone.la \
			          oid/oid40/liboid40-alone.la \
			          node/node40/libnode40-alone.la \
			          key/key40/libkey40-alone.la \
			          item/nodeptr40/libnodeptr40-alone.la \
			          item/direntry40/libdirentry40-alone.la \
			          item/tail40/libtail40-alone.la \
			          item/extent40/libextent40-alone.la \
			          item/stat40/libstat40-alone.la \
			          object/obj40/libobj40-alone.la \
			          object/dir40/libdir40-alone.la \
			          object/reg40/libreg40-alone.la \
				  object/sym40/libsym40-alone.la \
			          sdext/sdext_unix/libsdext_unix-alone.la \
			          sdext/sdext_lw/libsdext_lw-alone.la \
			          sdext/sdext_lt/libsdext_lt-alone.la \
			          sdext/sdext_symlink/libsdext_symlink-alone.la \
				  hash/r5_hash/libr5_hash-alone.la \
				  hash/fnv1_hash/libfnv1_hash-alone.la \
                                  hash/rupasov_hash/librupasov_hash-alone.la \
                                  hash/tea_hash/libtea_hash-alone.la

libreiser4_plugin_alone_la_SOURCES =

endif

libreiser4_plugin_la_SOURCES	=
