includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

journal40_sources		= journal40.c journal40_repair.c journal40.h journal40_repair.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libjournal40-static.la
libjournal40_static_la_SOURCES	= $(journal40_sources)
libjournal40_static_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libjournal40_static_la_CFLAGS	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     =
lib_LTLIBRARIES			= libjournal40.la
libjournal40_la_LIBADD		= @AAL_LIBS@ \
				  $(top_builddir)/libaux/libaux-static.la

libjournal40_la_SOURCES		= $(journal40_sources)
libjournal40_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libjournal40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libjournal40-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libjournal40_alone_la_SOURCES  	= $(journal40_sources)
libjournal40_alone_la_CFLAGS   	= @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

