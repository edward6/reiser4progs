includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

fnv1_hash_sources		= fnv1_hash.c

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libfnv1_hash-static.la
libfnv1_hash_static_la_SOURCES	= $(fnv1_hash_sources)
libfnv1_hash_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libfnv1_hash.la
libfnv1_hash_la_SOURCES		= $(fnv1_hash_sources)
libfnv1_hash_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                      = libfnv1_hash-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libfnv1_hash_alone_la_SOURCES   = $(fnv1_hash_sources)
libfnv1_hash_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)
