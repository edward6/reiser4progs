auxincludedir		    = -I$(top_srcdir)/include

if ENABLE_ALONE
ALONE_LIBS = libaux-alone.la
else
ALONE_LIBS =
endif

noinst_LTLIBRARIES	    = libaux-static.la $(ALONE_LIBS)

libaux_static_la_SOURCES    = aux.c bitmap.c
libaux_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_ALONE
libaux_alone_la_SOURCES     = aux.c bitmap.c
libaux_alone_la_CFLAGS      = @ALONE_CFLAGS@
endif

INCLUDES = $(auxincludedir)
