includedir		    = -I$(top_srcdir)/include

oid40_sources		    = oid40.c oid40_repair.c oid40.h oid40_repair.h

STATIC_LIBS		    = liboid40-static.la
liboid40_static_la_SOURCES  = $(oid40_sources)
liboid40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = liboid40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
liboid40_minimal_la_SOURCES   = $(oid40_sources)
liboid40_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

