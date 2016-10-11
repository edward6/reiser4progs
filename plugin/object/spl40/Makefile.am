includedir	 	    = -I$(top_srcdir)/include

spl40_sources	            = spl40.c spl40.h spl40_repair.c spl40_repair.h

STATIC_LIBS                 = libspl40-static.la
libspl40_static_la_SOURCES  = $(spl40_sources)
libspl40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libspl40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libspl40_minimal_la_SOURCES   = $(spl40_sources)
libspl40_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
