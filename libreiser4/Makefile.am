includedir		     = -I$(top_srcdir)/include

libreiser4_sources	     = libreiser4.c filesystem.c format.c journal.c alloc.c \
			       oid.c factory.c node.c tree.c key.c object.c place.c \
		 	       master.c status.c item.c param.c fake.c print.c semantic.c \
			       flow.c

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libreiser4-alone.la
else
ALONE_LIBS                   =
endif

lib_LTLIBRARIES		     = libreiser4.la $(ALONE_LIBS)
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

if ENABLE_STAND_ALONE
libreiser4_alone_la_LIBADD   = $(top_builddir)/libaux/libaux-alone.la \
			       $(top_builddir)/plugin/libreiser4-plugin-alone.la \
			       @AAL_ALONE_LIBS@ 

libreiser4_alone_la_SOURCES  = $(libreiser4_sources)

libreiser4_alone_la_CFLAGS   = @ALONE_CFLAGS@
libreiser4_alone_la_LDFLAGS  = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			       -release $(LT_RELEASE)
endif

INCLUDES                     = $(includedir)
