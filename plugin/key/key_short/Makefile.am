includedir			= -I$(top_srcdir)/include

key_short_sources               = key_short.c key_short.h key_short_repair.c

STATIC_LIBS                     = libkey_short-static.la
libkey_short_static_la_SOURCES	= $(key_short_sources)
libkey_short_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libkey_short-minimal.la
else
MINIMAL_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libkey_short_minimal_la_SOURCES  	= $(key_short_sources)
libkey_short_minimal_la_CFLAGS   	= @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
