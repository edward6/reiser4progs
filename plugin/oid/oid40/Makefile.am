includedir		    = -I$(top_srcdir)/include

oid40_sources		    = oid40.c oid40.h

STATIC_LIBS		    = liboid40-static.la
liboid40_static_la_SOURCES  = $(oid40_sources)
liboid40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                  = liboid40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
liboid40_alone_la_SOURCES   = $(oid40_sources)
liboid40_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

