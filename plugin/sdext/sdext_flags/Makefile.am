includedir			 = -I$(top_srcdir)/include

sdext_flags_sources		 = sdext_flags.c sdext_flags_repair.c sdext_flags.h

STATIC_LIBS                      = libsdext_flags-static.la
libsdext_flags_static_la_SOURCES = $(sdext_flags_sources)
libsdext_flags_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_flags-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_flags_alone_la_SOURCES = $(sdext_flags_sources)
libsdext_flags_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

