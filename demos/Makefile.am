noinst_PROGRAMS = ls cat

INCLUDES	= -I$(top_srcdir)/include

ls_SOURCES	= ls.c

ls_LDFLAGS      = @PROGS_LDFLAGS@

ls_CFLAGS	= @GENERIC_CFLAGS@

ls_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)

cat_SOURCES	= cat.c

cat_LDFLAGS     = @PROGS_LDFLAGS@

cat_CFLAGS	= @GENERIC_CFLAGS@

cat_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)
