includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_unix_sources		= sdext_unix.c sdext_unix.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libsdext_unix-static.la

libsdext_unix_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libsdext_unix_static_la_SOURCES = $(sdext_unix_sources)
else
lib_LTLIBRARIES			= libsdext_unix.la

libsdext_unix_la_LIBADD		= 
libsdext_unix_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libsdext_unix_la_SOURCES	= $(sdext_unix_sources)
endif

INCLUDES = $(includedir)

