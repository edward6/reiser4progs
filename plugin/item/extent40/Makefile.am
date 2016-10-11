includedir		      = -I$(top_srcdir)/include

extent40_sources	      = extent40.c extent40_repair.c extent40.h extent40_repair.h

STATIC_LIBS                   = libextent40-static.la
libextent40_static_la_SOURCES = $(extent40_sources)
libextent40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                    = libextent40-minimal.la
else
MINIMAL_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libextent40_minimal_la_SOURCES  = $(extent40_sources)
libextent40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

