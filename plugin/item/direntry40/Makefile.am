includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

direntry40_sources              = direntry40.c direntry40_repair.c direntry40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libdirentry40-static.la
libdirentry40_static_la_SOURCES = $(direntry40_sources)
libdirentry40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libdirentry40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libdirentry40.la
libdirentry40_la_LIBADD		= @AAL_LIBS@ \
				  $(top_builddir)/libaux/libaux-static.la

libdirentry40_la_SOURCES	= $(direntry40_sources)
libdirentry40_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libdirentry40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libdirentry40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdirentry40_alone_la_SOURCES  = $(direntry40_sources)
libdirentry40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

