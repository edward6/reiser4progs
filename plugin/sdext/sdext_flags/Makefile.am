includedir			 = -I$(top_srcdir)/include

sdext_flags_sources		 = sdext_flags.c sdext_flags_repair.c sdext_flags.h

STATIC_LIBS                      = libsdext_flags-static.la
libsdext_flags_static_la_SOURCES = $(sdext_flags_sources)
libsdext_flags_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_flags-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_flags_minimal_la_SOURCES = $(sdext_flags_sources)
libsdext_flags_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

