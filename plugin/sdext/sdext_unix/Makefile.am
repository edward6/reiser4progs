includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_unix_sources		= sdext_unix.c sdext_unix.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libsdext_unix-static.la
libsdext_unix_static_la_SOURCES = $(sdext_unix_sources)
libsdext_unix_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libsdext_unix_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                     =
lib_LTLIBRARIES			= libsdext_unix.la
libsdext_unix_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libsdext_unix_la_SOURCES	= $(sdext_unix_sources)
libsdext_unix_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libsdext_unix_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_ALONE
ALONE_LIBS                      = libsdext_unix-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES              = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libsdext_unix_alone_la_SOURCES  = $(sdext_unix_sources)
libsdext_unix_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

