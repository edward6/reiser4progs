includedir		     = -I$(top_srcdir)/include

libreiser4_sources	     = libreiser4.c filesystem.c format.c journal.c alloc.c \
			       oid.c factory.c node.c tree.c key.c object.c place.c \
			       master.c status.c backup.c item.c profile.c pset.c \
			       fake.c print.c semantic.c flow.c debug.c

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libreiser4-minimal.la
else
MINIMAL_LIBS                   =
endif

lib_LTLIBRARIES		     = libreiser4.la $(MINIMAL_LIBS)
noinst_LTLIBRARIES	     = libreiser4-static.la

libreiser4_la_LIBADD	     = $(top_builddir)/libaux/libaux-static.la \
			       $(top_builddir)/plugin/libreiser4-plugin.la \
			       @AAL_LIBS@ 

libreiser4_la_SOURCES	     = $(libreiser4_sources)

libreiser4_la_LDFLAGS	     = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			       -release $(LT_RELEASE)
libreiser4_la_CFLAGS         = @GENERIC_CFLAGS@

libreiser4_static_la_LIBADD  = $(top_builddir)/libaux/libaux-static.la \
			       $(top_builddir)/plugin/libreiser4-plugin.la \
			       @AAL_LIBS@ 

libreiser4_static_la_SOURCES = $(libreiser4_sources)
libreiser4_static_la_CFLAGS  = @GENERIC_CFLAGS@
libreiser4_static_la_LDFLAGS = -static

if ENABLE_MINIMAL
libreiser4_minimal_la_LIBADD   = $(top_builddir)/libaux/libaux-minimal.la \
			       $(top_builddir)/plugin/libreiser4-plugin-minimal.la \
			       @AAL_MINIMAL_LIBS@ 

libreiser4_minimal_la_SOURCES  = $(libreiser4_sources)

libreiser4_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
libreiser4_minimal_la_LDFLAGS  = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			       -release $(LT_RELEASE)
endif

INCLUDES                     = $(includedir)
