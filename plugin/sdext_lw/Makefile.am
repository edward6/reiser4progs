includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lw_sources		= sdext_lw.c sdext_lw.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libsdext_lw-static.la

libsdext_lw_static_la_LDFLAGS 	= $(PLUGIN_LDFLAGS)
libsdext_lw_static_la_SOURCES 	= $(sdext_lw_sources)
else
lib_LTLIBRARIES			= libsdext_lw.la

libsdext_lw_la_LIBADD		= 
libsdext_lw_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libsdext_lw_la_SOURCES		= $(sdext_lw_sources)
endif

INCLUDES = $(includedir)

