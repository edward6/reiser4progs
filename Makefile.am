SUBDIRS         = libaux plugin libreiser4 librepair libmisc progs demos include doc debug

EXTRA_DIST	= AUTHORS COPYING TODO NEWS BUGS README CREDITS THANKS \
		  reiser4progs.spec.in reiser4progs.spec libreiser4.m4 \
		  ltconfig laceup prepare

aclocaldir 	= $(datadir)/aclocal
aclocal_DATA	= libreiser4.m4

define all-sources
	( find ./ -name '*.[ch]' | grep -v SCCS | grep -v "Attic" | grep -v "reiser4progs" )
endef

TAGS:
	$(all-sources) | etags -

tags:
	$(all-sources) | xargs -- ctags -w -o TAGS
