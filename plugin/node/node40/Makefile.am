includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

node40_sources              = node40.c node40_repair.c node40.h

if ENABLE_MONOLITHIC
STATIC_LIBS		    = libnode40-static.la
libnode40_static_la_SOURCES = $(node40_sources)
libnode40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libnode40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS		    =
lib_LTLIBRARIES		    = libnode40.la
libnode40_la_LIBADD	    = @AAL_LIBS@ $(top_builddir)/libaux/libaux-static.la

libnode40_la_SOURCES	    = $(node40_sources)
libnode40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libnode40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libnode40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnode40_alone_la_SOURCES  = $(node40_sources)
libnode40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
