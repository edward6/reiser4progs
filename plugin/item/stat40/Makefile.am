includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

stat40_sources              = stat40.c stat40_repair.c stat40.h stat40_repair.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libstat40-static.la
libstat40_static_la_SOURCES = $(stat40_sources)
libstat40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libstat40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
lib_LTLIBRARIES		    = libstat40.la
libstat40_la_LIBADD	    = @AAL_LIBS@

libstat40_la_SOURCES	    = $(stat40_sources)
libstat40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libstat40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libstat40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libstat40_alone_la_SOURCES  = $(stat40_sources)
libstat40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

