includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lt_sources		= sdext_lt.c sdext_lt_repair.c sdext_lt.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libsdext_lt-static.la
libsdext_lt_static_la_SOURCES 	= $(sdext_lt_sources)
libsdext_lt_static_la_LDFLAGS 	= @PLUGIN_LDFLAGS@
libsdext_lt_static_la_CFLAGS 	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libsdext_lt.la
libsdext_lt_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libsdext_lt_la_SOURCES		= $(sdext_lt_sources)
libsdext_lt_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libsdext_lt_la_CFLAGS           = @GENERIC_CFLAGS@
endif

if ENABLE_ALONE
ALONE_LIBS                      = libsdext_lt-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libsdext_lt_alone_la_SOURCES    = $(sdext_lt_sources)
libsdext_lt_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

