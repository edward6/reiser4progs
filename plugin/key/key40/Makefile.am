includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

key40_sources                   = key40.c key40_repair.c key40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libkey40-static.la
libkey40_static_la_SOURCES	= $(key40_sources)
libkey40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libkey40.la
libkey40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libkey40_la_SOURCES		= $(key40_sources)
libkey40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                      = libkey40-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libkey40_alone_la_SOURCES  	= $(key40_sources)
libkey40_alone_la_CFLAGS   	= -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)
