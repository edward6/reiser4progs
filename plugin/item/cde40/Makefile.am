includedir		   = -I$(top_srcdir)/include

cde40_sources              = cde40.c cde40_repair.c cde40.h cde40_repair.h

STATIC_LIBS                = libcde40-static.la
libcde40_static_la_SOURCES = $(cde40_sources)
libcde40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                 = libcde40-minimal.la
else
MINIMAL_LIBS                 =
endif

noinst_LTLIBRARIES	   = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libcde40_minimal_la_SOURCES  = $(cde40_sources)
libcde40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
