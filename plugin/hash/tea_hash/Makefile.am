includedir		      = -I$(top_srcdir)/include
libdir			      = $(plugindir)

tea_hash_sources	      = tea_hash.c

if ENABLE_MONOLITHIC
STATIC_LIBS	              = libtea_hash-static.la
libtea_hash_static_la_SOURCES = $(tea_hash_sources)
libtea_hash_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS	              =
lib_LTLIBRARIES		      = libtea_hash.la
libtea_hash_la_SOURCES	      = $(tea_hash_sources)
libtea_hash_la_LDFLAGS	      = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                    = libtea_hash-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libtea_hash_alone_la_SOURCES  = $(tea_hash_sources)
libtea_hash_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

