includedir		     = -I$(top_srcdir)/include

libreiser4_sources	     = libreiser4.c filesystem.c format.c journal.c \
			       alloc.c oid.c factory.c node.c tree.c key.c \
			       object.c place.c master.c item.c profile.c print.c

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libreiser4-alone.la
else
ALONE_LIBS                   =
endif

lib_LTLIBRARIES		     = libreiser4.la $(ALONE_LIBS)
noinst_LTLIBRARIES	     = libreiser4-static.la

if ENABLE_MONOLITHIC

libreiser4_la_LIBADD	     = $(top_builddir)/libaal/libaal.la \
			       $(top_builddir)/libaux/libaux-static.la \
			       $(top_builddir)/plugin/libreiser4-plugin.la
libreiser4_ldflags           = -Wl,-T -Wl,libreiser4.lds

else

libreiser4_la_LIBADD	     = $(top_builddir)/libaal/libaal.la \
			       $(top_builddir)/libaux/libaux-static.la \
			       @LIBREISER4_LIBS@
libreiser4_ldflags           = 

endif

libreiser4_la_SOURCES	     = $(libreiser4_sources)

libreiser4_la_LDFLAGS	     = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			       -release $(LT_RELEASE) $(libreiser4_ldflags)
libreiser4_la_CFLAGS         = @GENERIC_CFLAGS@

libreiser4_static_la_LIBADD  = $(top_builddir)/libaal/libaal-static.la \
			       $(top_builddir)/libaux/libaux-static.la \
			       $(top_builddir)/plugin/libreiser4-plugin.la

libreiser4_static_la_SOURCES = $(libreiser4_sources)
libreiser4_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE

libreiser4_alone_la_LIBADD   = $(top_builddir)/libaal/libaal-alone.la \
			       $(top_builddir)/libaux/libaux-alone.la \
			       $(top_builddir)/plugin/libreiser4-plugin-alone.la

libreiser4_alone_la_SOURCES  = $(libreiser4_sources)

libreiser4_alone_la_CFLAGS   = @ALONE_CFLAGS@
libreiser4_alone_la_LDFLAGS  = -Wl,-T -Wl,libreiser4.lds
endif

INCLUDES                     = $(includedir)
EXTRA_DIST                   = libreiser4.lds
