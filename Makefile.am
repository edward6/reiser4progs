SUBDIRS         = libaux plugin libreiser4 librepair libmisc progs demos include doc debug

EXTRA_DIST	= AUTHORS COPYING TODO NEWS BUGS README CREDITS THANKS \
		  libreiser4.m4 ltconfig

aclocaldir 	= $(datadir)/aclocal
aclocal_DATA	= libreiser4.m4

define all-sources
	( find ./ -name '*.[ch]' -print | grep -v SCCS )
endef

TAGS:
	$(all-sources) | etags -
