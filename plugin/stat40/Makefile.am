includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

stat40_sources              = stat40.c stat40_repair.c stat40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	    = libstat40-static.la
libstat40_static_la_SOURCES = $(stat40_sources)
else
lib_LTLIBRARIES		    = libstat40.la
libstat40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/libaux/libaux-static.la

libstat40_la_SOURCES	    = $(stat40_sources)
endif

libstat40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libstat40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

INCLUDES = $(includedir)

