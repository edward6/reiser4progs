includedir			= -I$(top_srcdir)/include

extents_sources	                = extents.c

STATIC_LIBS                     = libextents-static.la
libextents_static_la_SOURCES 	= $(extents_sources)
libextents_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libextents-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libextents_minimal_la_SOURCES     = $(extents_sources)
libextents_minimal_la_CFLAGS      = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
