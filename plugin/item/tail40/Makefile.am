includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

tail40_sources		    = tail40.c

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libtail40-static.la
libtail40_static_la_SOURCES = $(tail40_sources)
libtail40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libtail40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
lib_LTLIBRARIES		    = libtail40.la
libtail40_la_LIBADD	    = $(top_builddir)/plugin/item/common40/libcommon40-static.la \
			      @AAL_LIBS@

libtail40_la_SOURCES	    = $(tail40_sources)
libtail40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libtail40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libtail40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libtail40_alone_la_SOURCES  = $(tail40_sources)
libtail40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

