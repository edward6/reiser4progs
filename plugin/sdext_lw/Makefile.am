includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lw_sources		= sdext_lw.c sdext_lw.h

noinst_LTLIBRARIES		= libsdext_lw-static.la
lib_LTLIBRARIES			= libsdext_lw.la

libsdext_lw_static_la_LDFLAGS 	= $(PLUGIN_LDFLAGS)
libsdext_lw_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libsdext_lw_static_la_SOURCES 	= $(sdext_lw_sources)
else
libsdext_lw_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la
libsdext_lw_la_SOURCES		= $(sdext_lw_sources)
endif

INCLUDES = $(includedir)

