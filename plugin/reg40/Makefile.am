includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

file40_sources		    = reg40.c reg40.h

noinst_LTLIBRARIES	    = libreg40-static.la
lib_LTLIBRARIES		    = libreg40.la

libreg40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
libreg40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libreg40_static_la_SOURCES  = $(file40_sources)
else
libreg40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la
libreg40_la_SOURCES	    = $(file40_sources)
endif

INCLUDES = $(includedir)

