includedir		         = -I$(top_srcdir)/include

key_common_sources               = key_common.c key_common.h

libkey_common_static_la_SOURCES  = $(key_common_sources)
libkey_common_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                       = libkey_common-minimal.la
else
MINIMAL_LIBS                       =
endif

noinst_LTLIBRARIES	         = libkey_common-static.la $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libkey_common_minimal_la_SOURCES   = $(key_common_sources)
libkey_common_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
