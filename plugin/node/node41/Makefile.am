includedir		    = -I$(top_srcdir)/include

node41_sources              = node41.c node41_repair.c node41.h node41_repair.h

STATIC_LIBS		    = libnode41-static.la
libnode41_static_la_SOURCES = $(node41_sources)
libnode41_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libnode41-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libnode41_minimal_la_SOURCES  = $(node41_sources)
libnode41_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
