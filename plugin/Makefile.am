SUBDIRS 			= format40 journal40 alloc40 node40 stat40 direntry40 nodeptr40 \
				  oid40 key40 r5_hash tea_hash rupasov_hash fnv1_hash sdext_unix \
			 	  tail40 sdext_lw extent40 sdext_lt sdext_symlink file format36 \
				  journal36 alloc36 

noinst_LTLIBRARIES	     	= libheader.la libfooter.la libreiser4-plugin.la

if ENABLE_MONOLITHIC
libheader_la_SOURCES 		= header.c
libfooter_la_SOURCES 		= footer.c

libreiser4_plugin_la_LIBADD  	= libheader.la \
			          format40/libformat40-static.la \
			          journal40/libjournal40-static.la \
			          alloc40/liballoc40-static.la \
			          oid40/liboid40-static.la \
			          node40/libnode40-static.la \
			          nodeptr40/libnodeptr40-static.la \
			          direntry40/libdirentry40-static.la \
			          tail40/libtail40-static.la \
			          extent40/libextent40-static.la \
			          stat40/libstat40-static.la \
			          key40/libkey40-static.la \
			          file/file40/libfile40-static.la \
			          file/dir40/libdir40-static.la \
			          file/reg40/libreg40-static.la \
			          file/symlink40/libsymlink40-static.la \
			          r5_hash/libr5_hash-static.la \
			          tea_hash/libtea_hash-static.la \
			          rupasov_hash/librupasov_hash-static.la \
			          fnv1_hash/libfnv1_hash-static.la \
			          sdext_unix/libsdext_unix-static.la \
			          sdext_lw/libsdext_lw-static.la \
			          sdext_lt/libsdext_lt-static.la \
			          sdext_symlink/libsdext_symlink-static.la \
			          libfooter.la
else
libheader_la_SOURCES 		= 
libfooter_la_SOURCES 		= 

libreiser4_plugin_la_LIBADD	=
endif

libreiser4_plugin_la_SOURCES	=
