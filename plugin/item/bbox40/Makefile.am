includedir			= -I$(top_srcdir)/include

bbox40_sources			= bbox40.c bbox40_repair.c bbox40_repair.h

STATIC_LIBS			= libbbox40-static.la
libbbox40_static_la_SOURCES	= $(bbox40_sources)
libbbox40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= libbbox40-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libbbox40_alone_la_SOURCES	= $(bbox40_sources)
libbbox40_alone_la_CFLAGS	= @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

