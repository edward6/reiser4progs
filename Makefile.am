if ENABLE_COMPACT
SUBDIRS         = libaal libaux plugin libreiser4 include doc debug
else
SUBDIRS         = libaal libaux plugin libreiser4 librepair progs demos include doc debug
endif

EXTRA_DIST	= ABOUT-NLS AUTHORS COPYING TODO NEWS BUGS README CREDITS THANKS \
		  reiser4progs.spec.in libreiser4.m4 ltconfig

aclocaldir 	= $(datadir)/aclocal
aclocal_DATA	= libreiser4.m4

dist-hook: reiser4progs.spec
	cp reiser4progs.spec $(distdir)

define all-sources
	( find ./ -name '*.[ch]' -print | grep -v SCCS )
endef

TAGS:
	$(all-sources) | etags -
