includedir			= -I$(top_srcdir)/include

tails_sources	                = tails.c

STATIC_LIBS                     = libtails-static.la
libtails_static_la_SOURCES 	= $(tails_sources)
libtails_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libtails-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libtails_minimal_la_SOURCES       = $(tails_sources)
libtails_minimal_la_CFLAGS        = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

