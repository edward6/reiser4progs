includedir		      = -I$(top_srcdir)/include

tea_hash_sources	      = tea_hash.c

STATIC_LIBS	              = libtea_hash-static.la
libtea_hash_static_la_SOURCES = $(tea_hash_sources)
libtea_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                    = libtea_hash-minimal.la
else
MINIMAL_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libtea_hash_minimal_la_SOURCES  = $(tea_hash_sources)
libtea_hash_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

