includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

journal40_sources		= journal40.c journal40_repair.c journal40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libjournal40-static.la
libjournal40_static_la_SOURCES	= $(journal40_sources)
libjournal40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     =
lib_LTLIBRARIES			= libjournal40.la
libjournal40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libjournal40_la_SOURCES		= $(journal40_sources)
libjournal40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                      = libjournal40-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libjournal40_alone_la_SOURCES  	= $(journal40_sources)
libjournal40_alone_la_CFLAGS   	= -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

