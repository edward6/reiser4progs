includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

extents_sources	                = extents.c

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libextents-static.la
libextents_static_la_SOURCES 	= $(extents_sources)
libextents_static_la_LDFLAGS 	= @PLUGIN_LDFLAGS@
libextents_static_la_CFLAGS 	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libextents.la

libextents_la_SOURCES		= $(extents_sources)
libextents_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libextents_la_CFLAGS              = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libextents-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libextents_alone_la_SOURCES     = $(extents_sources)
libextents_alone_la_CFLAGS      = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
