includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lt_sources		= sdext_lt.c sdext_lt.h

lib_LTLIBRARIES			= libsdext_lt.la
noinst_LTLIBRARIES		= libsdext_lt-static.la

libsdext_lt_la_LIBADD		= 
libsdext_lt_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libsdext_lt_la_SOURCES		= $(sdext_lt_sources)

libsdext_lt_static_la_LDFLAGS 	= $(PLUGIN_LDFLAGS)
libsdext_lt_static_la_SOURCES 	= $(sdext_lt_sources)

INCLUDES = $(includedir)

