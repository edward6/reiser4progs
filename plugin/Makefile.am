SUBDIRS 			= format journal alloc oid node key hash sdext item file

noinst_LTLIBRARIES	     	= libheader.la libfooter.la libreiser4-plugin.la

if ENABLE_MONOLITHIC
libheader_la_SOURCES 		= header.c
libfooter_la_SOURCES 		= footer.c

libreiser4_plugin_la_LIBADD  	= libheader.la \
			          format/format40/libformat40-static.la \
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
			          file/file40/libfile40-static.la \
			          file/dir40/libdir40-static.la \
			          file/reg40/libreg40-static.la \
			          file/symlink40/libsymlink40-static.la \
			          hash/r5_hash/libr5_hash-static.la \
			          hash/tea_hash/libtea_hash-static.la \
			          hash/rupasov_hash/librupasov_hash-static.la \
			          hash/fnv1_hash/libfnv1_hash-static.la \
			          sdext/sdext_unix/libsdext_unix-static.la \
			          sdext/sdext_lw/libsdext_lw-static.la \
			          sdext/sdext_lt/libsdext_lt-static.la \
			          sdext/sdext_symlink/libsdext_symlink-static.la \
			          libfooter.la
else
libheader_la_SOURCES 		= 
libfooter_la_SOURCES 		= 

libreiser4_plugin_la_LIBADD	=
endif

libreiser4_plugin_la_SOURCES	=
