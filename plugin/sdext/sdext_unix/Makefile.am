includedir			= -I$(top_srcdir)/include

sdext_unix_sources		= sdext_unix.c sdext_unix_repair.c sdext_unix.h

STATIC_LIBS                     = libsdext_unix-static.la
libsdext_unix_static_la_SOURCES = $(sdext_unix_sources)
libsdext_unix_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_unix-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES              = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_unix_alone_la_SOURCES  = $(sdext_unix_sources)
libsdext_unix_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

