includedir			= -I$(top_srcdir)/include

fnv1_hash_sources		= fnv1_hash.c

STATIC_LIBS                     = libfnv1_hash-static.la
libfnv1_hash_static_la_SOURCES	= $(fnv1_hash_sources)
libfnv1_hash_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libfnv1_hash-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libfnv1_hash_alone_la_SOURCES   = $(fnv1_hash_sources)
libfnv1_hash_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
