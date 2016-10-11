includedir		     = -I$(top_srcdir)/include

r5_hash_sources		     = r5_hash.c

STATIC_LIBS                  = libr5_hash-static.la
libr5_hash_static_la_SOURCES = $(r5_hash_sources)
libr5_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libr5_hash-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libr5_hash_minimal_la_SOURCES  = $(r5_hash_sources)
libr5_hash_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

