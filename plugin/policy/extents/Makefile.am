includedir			= -I$(top_srcdir)/include

extents_sources	                = extents.c

STATIC_LIBS                     = libextents-static.la
libextents_static_la_SOURCES 	= $(extents_sources)
libextents_static_la_CFLAGS 	= @GENERIC_CFLAGS@

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
