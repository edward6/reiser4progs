includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

fnv1_hash_sources		= fnv1_hash.c

noinst_LTLIBRARIES		= libfnv1_hash-static.la
lib_LTLIBRARIES			= libfnv1_hash.la

libfnv1_hash_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libfnv1_hash_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libfnv1_hash_static_la_SOURCES	= $(fnv1_hash_sources)
else
libfnv1_hash_la_SOURCES		= $(fnv1_hash_sources)
endif

INCLUDES = $(includedir)

