includedir		    = -I$(top_srcdir)/include

ccreg40_sources		    = ccreg40.c ccreg40.h ccreg40_repair.c

STATIC_LIBS                 = libccreg40-static.la
libccreg40_static_la_SOURCES  = $(ccreg40_sources)
libccreg40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libccreg40-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libccreg40_minimal_la_SOURCES    = $(ccreg40_sources)
libccreg40_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
