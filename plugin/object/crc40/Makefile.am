includedir		    = -I$(top_srcdir)/include

crc40_sources		    = crc40.c crc40.h crc40_repair.c crc40_repair.h

STATIC_LIBS                 = libcrc40-static.la
libcrc40_static_la_SOURCES  = $(crc40_sources)
libcrc40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libcrc40-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libcrc40_minimal_la_SOURCES    = $(crc40_sources)
libcrc40_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
