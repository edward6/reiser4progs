includedir		         = -I$(top_srcdir)/include

common40_sources                 = common40.c common40.h

libcommon40_static_la_SOURCES    = $(common40_sources)
libcommon40_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                       = libcommon40-alone.la
else
ALONE_LIBS                       =
endif

noinst_LTLIBRARIES	         = libcommon40-static.la $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libcommon40_alone_la_SOURCES     = $(common40_sources)
libcommon40_alone_la_CFLAGS      = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
