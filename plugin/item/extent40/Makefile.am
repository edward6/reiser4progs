includedir		      = -I$(top_srcdir)/include
libdir			      = $(plugindir)

extent40_sources	      = extent40.c extent40_repair.c extent40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                   = libextent40-static.la
libextent40_static_la_SOURCES = $(extent40_sources)
libextent40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libextent40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                   = 
lib_LTLIBRARIES		      = libextent40.la
libextent40_la_LIBADD	      = $(top_builddir)/plugin/item/common40/libcommon40-static.la \
				@AAL_LIBS@

libextent40_la_SOURCES	      = $(extent40_sources)
libextent40_la_LDFLAGS	      = @PLUGIN_LDFLAGS@
libextent40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libextent40-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libextent40_alone_la_SOURCES  = $(extent40_sources)
libextent40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

