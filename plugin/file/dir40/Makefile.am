includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

dir40_sources		    = dir40.c dir40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	    = libdir40-static.la
libdir40_static_la_SOURCES  = $(dir40_sources)
else
lib_LTLIBRARIES		    = libdir40.la
libdir40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
	     		      $(top_builddir)/plugin/file/file40/libfile40-static.la
libdir40_la_SOURCES	    = $(dir40_sources)
endif

libdir40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
libdir40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

INCLUDES = $(includedir)

