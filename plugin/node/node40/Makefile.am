includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

node40_sources              = node40.c node40_repair.c node40.h

if ENABLE_MONOLITHIC
STATIC_LIBS		    = libnode40-static.la
libnode40_static_la_SOURCES = $(node40_sources)
libnode40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS		    =
lib_LTLIBRARIES		    = libnode40.la
libnode40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/libaux/libaux-static.la

libnode40_la_SOURCES	    = $(node40_sources)
libnode40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                  = libnode40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libnode40_alone_la_SOURCES  = $(node40_sources)
libnode40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

