includedir			= -I$(top_srcdir)/include

smart_sources	                = smart.c

STATIC_LIBS                     = libsmart-static.la
libsmart_static_la_SOURCES 	= $(smart_sources)
libsmart_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsmart-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsmart_minimal_la_SOURCES       = $(smart_sources)
libsmart_minimal_la_CFLAGS        = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

