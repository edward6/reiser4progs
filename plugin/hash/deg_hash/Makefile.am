includedir		      = -I$(top_srcdir)/include

deg_hash_sources	      = deg_hash.c

STATIC_LIBS	              = libdeg_hash-static.la
libdeg_hash_static_la_SOURCES = $(deg_hash_sources)
libdeg_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                    = libdeg_hash-minimal.la
else
MINIMAL_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libdeg_hash_minimal_la_SOURCES  = $(deg_hash_sources)
libdeg_hash_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

