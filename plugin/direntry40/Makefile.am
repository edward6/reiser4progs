includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

if ENABLE_COMPACT
direntry40_sources		= direntry40.c direntry40.h
else
direntry40_sources              = direntry40.c direntry40_repair.c direntry40.h
endif

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libdirentry40-static.la

libdirentry40_la_LIBADD		= 

libdirentry40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libdirentry40_static_la_SOURCES = $(direntry40_sources)
else
lib_LTLIBRARIES			= libdirentry40.la

libdirentry40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la
	
libdirentry40_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libdirentry40_la_SOURCES	= $(direntry40_sources)
endif

INCLUDES = $(includedir)

