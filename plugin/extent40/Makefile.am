includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

extent40_sources	    = extent40.c extent40.h

lib_LTLIBRARIES		    = libextent40.la
noinst_LTLIBRARIES	    = libextent40-static.la

libextent40_la_LIBADD	    = 
libextent40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
libextent40_la_SOURCES	    = $(extent40_sources)

libextent40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libextent40_static_la_SOURCES = $(extent40_sources)

INCLUDES = $(includedir)

