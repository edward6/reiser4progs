includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

tails_sources	                = tails.c

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libtails-static.la
libtails_static_la_SOURCES 	= $(tails_sources)
libtails_static_la_LDFLAGS 	= @PLUGIN_LDFLAGS@
libtails_static_la_CFLAGS 	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libtails.la

libtails_la_SOURCES		= $(tails_sources)
libtails_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libtails_la_CFLAGS              = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libtails-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libtails_alone_la_SOURCES       = $(tails_sources)
libtails_alone_la_CFLAGS        = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

