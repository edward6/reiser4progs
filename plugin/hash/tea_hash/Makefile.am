includedir		      = -I$(top_srcdir)/include

tea_hash_sources	      = tea_hash.c

STATIC_LIBS	              = libtea_hash-static.la
libtea_hash_static_la_SOURCES = $(tea_hash_sources)
libtea_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libtea_hash-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libtea_hash_alone_la_SOURCES  = $(tea_hash_sources)
libtea_hash_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

