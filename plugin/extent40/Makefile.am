includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

extent40_sources	    = extent40.c extent40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	    = libextent40-static.la

libextent40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libextent40_static_la_SOURCES = $(extent40_sources)
else
lib_LTLIBRARIES		    = libextent40.la

libextent40_la_LIBADD	    = 
libextent40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
libextent40_la_SOURCES	    = $(extent40_sources)
endif

INCLUDES = $(includedir)

