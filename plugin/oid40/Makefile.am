includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

oid40_sources		    = oid40.c oid40.h

noinst_LTLIBRARIES	    = liboid40-static.la
lib_LTLIBRARIES		    = liboid40.la

liboid40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
liboid40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
liboid40_static_la_SOURCES  = $(oid40_sources)
else
liboid40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
	  		      $(top_builddir)/libaux/libaux-static.la

liboid40_la_SOURCES	    = $(oid40_sources)
endif

INCLUDES = $(includedir)

