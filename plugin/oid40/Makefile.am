includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

oid40_sources		    = oid40.c oid40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	    = liboid40-static.la

liboid40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
liboid40_static_la_SOURCES  = $(oid40_sources)
else
lib_LTLIBRARIES		    = liboid40.la

liboid40_la_LIBADD	    = 
liboid40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
liboid40_la_SOURCES	    = $(oid40_sources)
endif

INCLUDES = $(includedir)

