includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

journal40_sources		= journal40.c journal40.h

noinst_LTLIBRARIES		= libjournal40-static.la
lib_LTLIBRARIES			= libjournal40.la

libjournal40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libjournal40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libjournal40_static_la_SOURCES	= $(journal40_sources)
else
libjournal40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libjournal40_la_SOURCES		= $(journal40_sources)
endif

INCLUDES = $(includedir)

