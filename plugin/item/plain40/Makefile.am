includedir		      = -I$(top_srcdir)/include

plain40_sources		      = plain40.c plain40_repair.c plain40.h plain40_repair.h

STATIC_LIBS                   = libplain40-static.la
libplain40_static_la_SOURCES  = $(plain40_sources)
libplain40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libplain40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES            = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libplain40_minimal_la_SOURCES = $(plain40_sources)
libplain40_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

