includedir			 = -I$(top_srcdir)/include

sdext_plug_sources		 = sdext_plug.c sdext_plug_repair.c sdext_plug.h

STATIC_LIBS                      = libsdext_plug-static.la
libsdext_plug_static_la_SOURCES = $(sdext_plug_sources)
libsdext_plug_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_plug-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_plug_alone_la_SOURCES = $(sdext_plug_sources)
libsdext_plug_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

