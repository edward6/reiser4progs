includedir			= -I$(top_srcdir)/include

bbox40_sources			= bbox40.c bbox40_repair.c bbox40_repair.h

STATIC_LIBS			= libbbox40-static.la
libbbox40_static_la_SOURCES	= $(bbox40_sources)
libbbox40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libbbox40-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libbbox40_minimal_la_SOURCES	= $(bbox40_sources)
libbbox40_minimal_la_CFLAGS	= @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

