includedir			 = -I$(top_srcdir)/include

sdext_crc_sources		 = sdext_crc.c sdext_crc.h sdext_crc_repair.c

STATIC_LIBS                      = libsdext_crc-static.la
libsdext_crc_static_la_SOURCES = $(sdext_crc_sources)
libsdext_crc_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_crc-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_crc_minimal_la_SOURCES = $(sdext_crc_sources)
libsdext_crc_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
