includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

format36_sources                = format36.c format36_repair.c format36.h

noinst_LTLIBRARIES		= libformat36-static.la
lib_LTLIBRARIES			= libformat36.la

libformat36_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libformat36_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libformat36_static_la_SOURCES	= $(format36_sources)
else
libformat36_la_LIBADD		= $(top_builddir)/libaal/libaal.la
libformat36_la_SOURCES		= $(format36_sources)
endif

INCLUDES = $(includedir)

