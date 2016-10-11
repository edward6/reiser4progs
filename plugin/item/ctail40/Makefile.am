includedir		     = -I$(top_srcdir)/include

ctail40_sources		     = ctail40.c ctail40.h ctail40_repair.c ctail40_repair.h

STATIC_LIBS                  = libctail40-static.la
libctail40_static_la_SOURCES = $(ctail40_sources)
libctail40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                 = libctail40-minimal.la
else
MINIMAL_LIBS                 =
endif

noinst_LTLIBRARIES           = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libctail40_minimal_la_SOURCES = $(ctail40_sources)
libctail40_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

