includedir			= -I$(top_srcdir)/include

key_large_sources               = key_large.c key_large.h key_large_repair.c

STATIC_LIBS                     = libkey_large-static.la
libkey_large_static_la_SOURCES	= $(key_large_sources)
libkey_large_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libkey_large-minimal.la
else
MINIMAL_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libkey_large_minimal_la_SOURCES  	= $(key_large_sources)
libkey_large_minimal_la_CFLAGS   	= @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
