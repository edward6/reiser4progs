includedir		      = -I$(top_srcdir)/include

deg_hash_sources	      = deg_hash.c

STATIC_LIBS	              = libdeg_hash-static.la
libdeg_hash_static_la_SOURCES = $(deg_hash_sources)
libdeg_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libdeg_hash-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdeg_hash_alone_la_SOURCES  = $(deg_hash_sources)
libdeg_hash_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

