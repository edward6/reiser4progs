SUBDIRS 			= format journal alloc oid node key hash sdext item object

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libreiser4-plugin-alone.la
else
ALONE_LIBS                      = 
endif

noinst_LTLIBRARIES	     	= libreiser4-plugin.la $(ALONE_LIBS)

if ENABLE_MONOLITHIC

if ENABLE_R5_HASH
monolithic_r5_hash_libs         = hash/r5_hash/libr5_hash-static.la
else
monolithic_r5_hash_libs         =
endif

if ENABLE_FNV1_HASH
monolithic_fnv1_hash_libs       = hash/fnv1_hash/libfnv1_hash-static.la
else
monolithic_fnv1_hash_libs       =
endif

if ENABLE_RUPASOV_HASH
monolithic_rupasov_hash_libs    = hash/rupasov_hash/librupasov_hash-static.la
else
monolithic_rupasov_hash_libs    =
endif

if ENABLE_TEA_HASH
monolithic_tea_hash_libs        = hash/tea_hash/libtea_hash-static.la
else
monolithic_tea_hash_libs        =
endif

if ENABLE_SYMLINKS_SUPPORT
monolithic_sym40_libs           = object/sym40/libsym40-static.la
monolithic_sdext_symlink_libs   = sdext/sdext_symlink/libsdext_symlink-static.la
else
monolithic_sym40_libs           = 
monolithic_sdext_symlink_libs   = 
endif

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
				  $(monolithic_sym40_libs) \
			          sdext/sdext_unix/libsdext_unix-static.la \
			          sdext/sdext_lw/libsdext_lw-static.la \
			          sdext/sdext_lt/libsdext_lt-static.la \
			          $(monolithic_sdext_symlink_libs) \
				  $(monolithic_r5_hash_libs) $(monolithic_fnv1_hash_libs) \
                                  $(monolithic_rupasov_hash_libs) $(monolithic_tea_hash_libs)
endif

if ENABLE_STAND_ALONE

if ENABLE_R5_HASH
alone_r5_hash_libs              = hash/r5_hash/libr5_hash-alone.la
else
alone_r5_hash_libs              =
endif

if ENABLE_FNV1_HASH
alone_fnv1_hash_libs            = hash/fnv1_hash/libfnv1_hash-alone.la
else
alone_fnv1_hash_libs            =
endif

if ENABLE_RUPASOV_HASH
alone_rupasov_hash_libs         = hash/rupasov_hash/librupasov_hash-alone.la
else
alone_rupasov_hash_libs         =
endif

if ENABLE_TEA_HASH
alone_tea_hash_libs             = hash/tea_hash/libtea_hash-alone.la
else
alone_tea_hash_libs            	=
endif

if ENABLE_SYMLINKS_SUPPORT
alone_sym40_libs                = object/sym40/libsym40-alone.la
alone_sdext_symlink_libs        = sdext/sdext_symlink/libsdext_symlink-alone.la
else
alone_sym40_libs                = 
alone_sdext_symlink_libs        = 
endif

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
			          $(alone_sym40_libs) \
			          sdext/sdext_unix/libsdext_unix-alone.la \
			          sdext/sdext_lw/libsdext_lw-alone.la \
			          sdext/sdext_lt/libsdext_lt-alone.la \
			          $(alone_sdext_symlink_libs) \
				  $(alone_r5_hash_libs) $(alone_fnv1_hash_libs) \
                                  $(alone_rupasov_hash_libs) $(alone_tea_hash_libs)

libreiser4_plugin_alone_la_SOURCES =

endif

libreiser4_plugin_la_SOURCES	=
