includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

reg40_sources		    = reg40.c reg40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	    = libreg40-static.la
libreg40_static_la_SOURCES  = $(reg40_sources)
else
lib_LTLIBRARIES		    = libreg40.la
libreg40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la
libreg40_la_SOURCES	    = $(reg40_sources)
endif

libreg40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
libreg40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

INCLUDES = $(includedir)

