includedir		      = -I$(top_srcdir)/include

object40_sources              = object40.c object40.h

libobject40_static_la_SOURCES = $(object40_sources)
libobject40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_ALONE
ALONE_LIBS                    = libobject40-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = libobject40-static.la $(ALONE_LIBS)

if ENABLE_ALONE
libobject40_alone_la_SOURCES  = $(object40_sources)
libobject40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

