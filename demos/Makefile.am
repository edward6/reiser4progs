noinst_PROGRAMS = cat

INCLUDES	= -I$(top_srcdir)/include

cat_SOURCES	= cat.c

cat_LDFLAGS     = @PROGS_LDFLAGS@

cat_CFLAGS	= @GENERIC_CFLAGS@

cat_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)
