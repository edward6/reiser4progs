librepair_sources            = filesystem.c tree.c master.c format.c journal.c alloc.c node.c item.c \
			       object.c filter.c disk_scan.c twig_scan.c add_missing.c semantic.c \
			       cleanup.c repair.c key.c

lib_LTLIBRARIES		     = librepair.la

noinst_LTLIBRARIES	     = librepair-static.la

librepair_la_LDFLAGS	     = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -release $(LT_RELEASE)

librepair_la_LIBADD	     = $(top_builddir)/libreiser4/libreiser4.la

librepair_la_SOURCES	     = $(librepair_sources)
librepair_la_CFLAGS	     = @GENERIC_CFLAGS@

librepair_static_la_LIBADD   = $(top_builddir)/libreiser4/libreiser4-static.la

librepair_static_la_SOURCES  = $(librepair_sources)
librepair_static_la_CFLAGS   = @GENERIC_CFLAGS@

INCLUDES = -I$(top_srcdir)/include

