includedir		      = -I$(top_srcdir)/include

obj40_sources                 = obj40.c obj40_repair.c obj40.h

libobj40_static_la_SOURCES    = $(obj40_sources)
libobj40_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libobj40-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = libobj40-static.la $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libobj40_alone_la_SOURCES     = $(obj40_sources)
libobj40_alone_la_CFLAGS      = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

