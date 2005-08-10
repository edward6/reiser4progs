auxincludedir			= -I$(top_srcdir)/include

if ENABLE_MINIMAL
MINIMAL_LIBS			= libaux-minimal.la
else
MINIMAL_LIBS =
endif

noinst_LTLIBRARIES		= libaux-static.la $(MINIMAL_LIBS)

libaux_static_la_SOURCES	= aux.c gauge.c
libaux_static_la_CFLAGS		= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
libaux_minimal_la_SOURCES	= aux.c gauge.c
libaux_minimal_la_CFLAGS	= @MINIMAL_CFLAGS@
endif

INCLUDES = $(auxincludedir)
