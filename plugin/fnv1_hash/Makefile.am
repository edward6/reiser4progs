includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

fnv1_hash_sources		= fnv1_hash.c

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libfnv1_hash-static.la

libfnv1_hash_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libfnv1_hash_static_la_SOURCES	= $(fnv1_hash_sources)
else
lib_LTLIBRARIES			= libfnv1_hash.la

libfnv1_hash_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libfnv1_hash_la_SOURCES		= $(fnv1_hash_sources)
endif

INCLUDES = $(includedir)

