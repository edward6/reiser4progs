includedir		   = -I$(top_srcdir)/include

cde40_sources              = cde40.c cde40_repair.c cde40.h cde40_repair.h

STATIC_LIBS                = libcde40-static.la
libcde40_static_la_SOURCES = $(cde40_sources)
libcde40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                 = libcde40-alone.la
else
ALONE_LIBS                 =
endif

noinst_LTLIBRARIES	   = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libcde40_alone_la_SOURCES  = $(cde40_sources)
libcde40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
