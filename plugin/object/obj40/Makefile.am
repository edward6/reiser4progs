includedir		      = -I$(top_srcdir)/include

obj40_sources                 = obj40.c obj40_repair.c obj40.h obj40_repair.h

libobj40_static_la_SOURCES    = $(obj40_sources)
libobj40_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                    = libobj40-minimal.la
else
MINIMAL_LIBS                    =
endif

noinst_LTLIBRARIES	      = libobj40-static.la $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libobj40_minimal_la_SOURCES     = $(obj40_sources)
libobj40_minimal_la_CFLAGS      = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

