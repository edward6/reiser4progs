includedir			= -I$(top_srcdir)/include

sdext_unix_sources		= sdext_unix.c sdext_unix_repair.c sdext_unix.h

STATIC_LIBS                     = libsdext_unix-static.la
libsdext_unix_static_la_SOURCES = $(sdext_unix_sources)
libsdext_unix_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_unix-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES              = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_unix_minimal_la_SOURCES  = $(sdext_unix_sources)
libsdext_unix_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

