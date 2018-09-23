includedir		      = -I$(top_srcdir)/include

extent41_sources	      = extent41.c extent41.h

STATIC_LIBS                   = libextent41-static.la
libextent41_static_la_SOURCES = $(extent41_sources)
libextent41_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                    = libextent41-minimal.la
else
MINIMAL_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libextent41_minimal_la_SOURCES  = $(extent41_sources)
libextent41_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
