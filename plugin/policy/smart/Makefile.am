includedir			= -I$(top_srcdir)/include

smart_sources	                = smart.c

STATIC_LIBS                     = libsmart-static.la
libsmart_static_la_SOURCES 	= $(smart_sources)
libsmart_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsmart-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsmart_alone_la_SOURCES       = $(smart_sources)
libsmart_alone_la_CFLAGS        = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

