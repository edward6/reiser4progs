includedir		    = -I$(top_srcdir)/include

dir40_sources		    = dir40.c dir40_repair.c dir40.h dir40_repair.h

STATIC_LIBS                 = libdir40-static.la
libdir40_static_la_SOURCES  = $(dir40_sources)
libdir40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libdir40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdir40_alone_la_SOURCES    = $(dir40_sources)
libdir40_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
