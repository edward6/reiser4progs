includedir		    = -I$(top_srcdir)/include

node40_sources              = node40.c node40_repair.c node40.h node40_repair.h

STATIC_LIBS		    = libnode40-static.la
libnode40_static_la_SOURCES = $(node40_sources)
libnode40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libnode40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libnode40_minimal_la_SOURCES  = $(node40_sources)
libnode40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
