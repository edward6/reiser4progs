includedir			 = -I$(top_srcdir)/include

sdext_plug_sources		 = sdext_plug.c sdext_plug_repair.c sdext_plug.h

STATIC_LIBS                      = libsdext_plug-static.la
libsdext_plug_static_la_SOURCES = $(sdext_plug_sources)
libsdext_plug_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_plug-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_plug_minimal_la_SOURCES = $(sdext_plug_sources)
libsdext_plug_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

