includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

r5_hash_sources		     = r5_hash.c

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	     = libr5_hash-static.la

libr5_hash_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libr5_hash_static_la_SOURCES = $(r5_hash_sources)
else
lib_LTLIBRARIES		     = libr5_hash.la

libr5_hash_la_LDFLAGS	     = $(PLUGIN_LDFLAGS)
libr5_hash_la_SOURCES	     = $(r5_hash_sources)
endif

INCLUDES = $(includedir)

