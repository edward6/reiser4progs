includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

dir40_sources		    = dir40.c dir40.h

noinst_LTLIBRARIES	    = libdir40-static.la
lib_LTLIBRARIES		    = libdir40.la

libdir40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
libdir40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libdir40_static_la_SOURCES  = $(dir40_sources)
else
libdir40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la
libdir40_la_SOURCES	    = $(dir40_sources)
endif

INCLUDES = $(includedir)

