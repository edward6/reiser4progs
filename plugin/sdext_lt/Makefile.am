includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lt_sources		= sdext_lt.c sdext_lt.h

noinst_LTLIBRARIES		= libsdext_lt-static.la
lib_LTLIBRARIES			= libsdext_lt.la

libsdext_lt_static_la_LDFLAGS 	= $(PLUGIN_LDFLAGS)
libsdext_lt_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libsdext_lt_static_la_SOURCES 	= $(sdext_lt_sources)
else
libsdext_lt_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libsdext_lt_la_SOURCES		= $(sdext_lt_sources)
endif

INCLUDES = $(includedir)

