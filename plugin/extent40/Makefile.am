includedir		      = -I$(top_srcdir)/include
libdir			      = $(plugindir)

extent40_sources	      = extent40.c extent40.h

noinst_LTLIBRARIES	      = libextent40-static.la
lib_LTLIBRARIES		      = libextent40.la

libextent40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libextent40_la_LDFLAGS	      = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libextent40_static_la_SOURCES = $(extent40_sources)
else
libextent40_la_LIBADD	      = $(top_builddir)/libaal/libaal.la \
			        $(top_builddir)/libaux/libaux-static.la

libextent40_la_SOURCES	      = $(extent40_sources)
endif

INCLUDES = $(includedir)

