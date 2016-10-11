includedir	 	    = -I$(top_srcdir)/include

sym40_sources	            = sym40.c sym40_repair.c sym40_repair.h

STATIC_LIBS                 = libsym40-static.la
libsym40_static_la_SOURCES  = $(sym40_sources)
libsym40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libsym40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsym40_minimal_la_SOURCES   = $(sym40_sources)
libsym40_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
