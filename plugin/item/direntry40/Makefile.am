includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

direntry40_sources              = direntry40.c direntry40_repair.c direntry40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libdirentry40-static.la
libdirentry40_static_la_SOURCES = $(direntry40_sources)
libdirentry40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libdirentry40.la
libdirentry40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libdirentry40_la_SOURCES	= $(direntry40_sources)
libdirentry40_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                      = libdirentry40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libdirentry40_alone_la_SOURCES  = $(direntry40_sources)
libdirentry40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

