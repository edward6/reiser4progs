includedir		    = -I$(top_srcdir)/include

file40_sources              = file40.c file40.h

libfile40_static_la_SOURCES = $(file40_sources)

if ENABLE_ALONE
ALONE_LIBS                  = libfile40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES	    = libfile40-static.la $(ALONE_LIBS)

if ENABLE_ALONE

libfile40_alone_la_SOURCES  = $(file40_sources)
libfile40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin

endif

INCLUDES = $(includedir)

