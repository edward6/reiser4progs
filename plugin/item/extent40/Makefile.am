includedir		      = -I$(top_srcdir)/include

extent40_sources	      = extent40.c extent40_repair.c extent40.h extent40_repair.h

STATIC_LIBS                   = libextent40-static.la
libextent40_static_la_SOURCES = $(extent40_sources)
libextent40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libextent40-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libextent40_alone_la_SOURCES  = $(extent40_sources)
libextent40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

