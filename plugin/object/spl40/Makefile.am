includedir	 	    = -I$(top_srcdir)/include

spl40_sources	            = spl40.c spl40.h spl40_repair.c spl40_repair.h

STATIC_LIBS                 = libspl40-static.la
libspl40_static_la_SOURCES  = $(spl40_sources)
libspl40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libspl40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libspl40_alone_la_SOURCES   = $(spl40_sources)
libspl40_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
