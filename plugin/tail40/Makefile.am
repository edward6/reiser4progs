includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

tail40_sources		    = tail40.c

noinst_LTLIBRARIES	    = libtail40-static.la
lib_LTLIBRARIES		    = libtail40.la

libtail40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libtail40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libtail40_static_la_SOURCES = $(tail40_sources)
else
libtail40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/libaux/libaux-static.la

libtail40_la_SOURCES	    = $(tail40_sources)
endif

INCLUDES = $(includedir)

