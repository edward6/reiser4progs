includedir		      = -I$(top_srcdir)/include
libdir			      = $(plugindir)

tea_hash_sources	      = tea_hash.c

noinst_LTLIBRARIES	      = libtea_hash-static.la
lib_LTLIBRARIES		      = libtea_hash.la

libtea_hash_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libtea_hash_la_LDFLAGS	      = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libtea_hash_static_la_SOURCES = $(tea_hash_sources)
else
libtea_hash_la_SOURCES	      = $(tea_hash_sources)
endif

INCLUDES = $(includedir)

