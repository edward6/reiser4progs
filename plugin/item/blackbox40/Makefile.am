includedir			= -I$(top_srcdir)/include

blackbox40_sources		= blackbox40.c blackbox40_repair.c blackbox40_repair.h

STATIC_LIBS			= libblackbox40-static.la
libblackbox40_static_la_SOURCES = $(blackbox40_sources)
libblackbox40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= libblackbox40-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libblackbox40_alone_la_SOURCES  = $(blackbox40_sources)
libblackbox40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

