includedir			= -I$(top_srcdir)/include

fnv1_hash_sources		= fnv1_hash.c

STATIC_LIBS                     = libfnv1_hash-static.la
libfnv1_hash_static_la_SOURCES	= $(fnv1_hash_sources)
libfnv1_hash_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libfnv1_hash-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libfnv1_hash_minimal_la_SOURCES   = $(fnv1_hash_sources)
libfnv1_hash_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
