librepair_sources            = filesystem.c tree.c master.c format.c journal.c node.c coord.c item.c filter.c \
			       disk_scan.c twig_scan.c add_missing.c

lib_LTLIBRARIES		     = librepair.la

noinst_LTLIBRARIES	     = librepair-static.la

librepair_la_LDFLAGS	     = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -release $(LT_RELEASE)

librepair_la_LIBADD	     = $(top_builddir)/libreiser4/libreiser4.la

librepair_la_SOURCES	     = $(librepair_sources)
librepair_la_CFLAGS	     = @GENERIC_FLAGS@

librepair_static_la_LIBADD   = $(top_builddir)/libreiser4/libreiser4-static.la

librepair_static_la_SOURCES  = $(librepair_sources)
librepair_sttaic_la_CFLAGS   = @GENERIC_FLAGS@

INCLUDES = -I$(top_srcdir)/include

